/* ***
 * i686/ixkm.s - Initialization and main entry for the kernel
 * Copyright (C) 2014-2015  Meisaka Yukara
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 *     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

.section .initstack
	.fill 0xffff
.align 16
ixstk:

.data

# settings / some macros
.set PIC1_CMD, 0x20
.set PIC1_DAT, 0x21
.set PIC2_CMD, 0xA0
.set PIC2_DAT, 0xA1

.macro idt_trap offs=0, sel=0, dpl=0
.int \offs
.word \sel
.byte 0
.byte 0b10001111 | (\dpl << 5)
.endm
.macro idt_int offs=0, sel=0, dpl=0
.int \offs
.word \sel
.byte 0
.byte 0b10001110 | (\dpl << 5)
.endm

# data tables

_ivix_idt_ptr:
.word 0x07ff 
.int _ivix_idt

_ivix_gdt_ptr:
.word 0x0100 * 8
.int gdt_data	# linked externally

_ivix_dump_n:
.int 0
_ivix_int_n:
.global _ivix_int_n
.int 0

_ivix_irq1_fn:
.int 0
.global _ivix_irq1_fn
_ivix_irq12_fn:
.int 0
.global _ivix_irq12_fn

.section .ixboot,"ax"
	.int 0xeca70433
_ix_entry:
	.global _ix_entry
	lgdt .ixboot.gdtptr	# load initial GDT
	leal _kernel_phy_end, %eax	# set up page directory at end of kernel
	mov %eax, %edi
	orl $0x1, %eax
	movl %eax, %edx
	xor %eax, %eax
	movl %edx, _ivix_phy_pdpt	# map 0x0.. and 0xc.. in virtual memory
	movl %eax, _ivix_phy_pdpt + 4
	movl %edx, _ivix_phy_pdpt + 0x08
	movl %eax, _ivix_phy_pdpt + 0x0c
	movl %edx, _ivix_phy_pdpt + 0x10
	movl %eax, _ivix_phy_pdpt + 0x14
	movl %edx, _ivix_phy_pdpt + 0x18
	movl %eax, _ivix_phy_pdpt + 0x1c
	movl %eax, %edx
	orl $0x83, %eax		# set flags on it 2M pages
	movl %eax, _kernel_phy_end
	movl %edx, _kernel_phy_end + 4
	addl $0x200000, %eax 	# identity map 0 - 4M
	movl %eax, _kernel_phy_end + 8
	movl %edx, _kernel_phy_end + 12
	movl %cr4, %eax
	orl $0x20, %eax	# enable PAE
	movl %eax, %cr4
	leal _ivix_phy_pdpt, %eax	# load CR3 with the page table
	movl %eax, %cr3
	movl %cr0, %eax
	orl $0x80000000, %eax	# enable paging bit
	movl %eax, %cr0
	ljmp $0x10, $1f
1:
	jmp _kernel_entry

.align 8
.ixboot.gdtptr:
.word 4 * 8
.int .ixboot.gdt
.align 8
.ixboot.gdt:
.int 0
.int 0
.int 0x0000ffff
.int 0x00cf9200
.int 0x0000ffff
.int 0x00cf9a00

.align 32
_ivix_phy_pdpt:
.global _ivix_phy_pdpt
.set _ivix_pdpt, _ivix_phy_pdpt + 0xc0000000
.global _ivix_pdpt
.fill 32, 2, 0xfe00

.section .ctors.begin
.int 0xffffffff
.section .ctors.end

.text
_kernel_entry:
	mov $ixstk, %esp
	lgdt _ivix_gdt_ptr	# load GDT
	call _ix_makeidt	# make and load IDT
	lidt _ivix_idt_ptr
	call _ix_initpic	# configure the PIC

	sub $0xc, %esp		# align the stack
	call _ix_construct	# call ctors
	sti			# interupts on
	call _kernel_main	# jump to C/C++
	jmp _ix_halt		# idle loop if we get here

_iv_regdump:
	push %ebp
	mov %esp, %ebp
	movl 0x0c(%ebp), %ecx
	movw $0x0500+'0', %ax
	mov $0xC00B8000 + 20 + (6 * 160), %edi
	mov $rdumpnames, %esi
	call putzstr
	movl 0x1c(%ecx), %ebx	# EAX
	call puthex32
	call putzstr
	movl 0x18(%ecx), %ebx	# ECX
	call puthex32
	call putzstr
	movl 0x14(%ecx), %ebx	# EDX
	call puthex32
	call putzstr
	movl 0x10(%ecx), %ebx	# EBX
	call puthex32
	add $56, %edi
	call putzstr
	movl 0x0c(%ecx), %ebx	# ESP
	call puthex32
	call putzstr
	movl 0x08(%ecx), %ebx	# EBP
	call puthex32
	call putzstr
	movl 0x04(%ecx), %ebx	# ESI
	call puthex32
	call putzstr
	movl 0x0(%ecx), %ebx	# EDI
	call puthex32
	add $56, %edi
	call putzstr
	movl 0x08(%ebp), %ecx
	movl 0x0(%ecx), %ebx	# EIP
	call puthex32
	call putzstr
	movl 0x4(%ecx), %ebx	# CS
	call puthex32
	call putzstr
	movl 0x8(%ecx), %ebx	# EFlag
	call puthex32
	call putzstr
	movl -0x4(%ecx), %ebx	# N (Extra)
	call puthex32
	add $56, %edi
	call putzstr
	mov %cr2, %ebx
	call puthex32
	add $68, %edi
	call putzstr
	movl 0x0c(%ebp), %ecx
	movl 0x0c(%ecx), %esi	# ESP
	mov $32, %edx
	add $56, %edi
2:	and %edx, %edx
	je 3f
	add $66, %edi
	mov %esi, %ebx
	call puthex32
	add $6, %edi
1:	mov 0(%esi), %ebx
	call puthex32
	add $2, %edi
	add $4, %esi
	sub $1, %edx
	test $3, %dx
	je 2b
	and %edx, %edx
	jne 1b
3:
	add $66, %edi
	mov _ivix_irq1_fn, %ebx
	call puthex32
	pop %ebp
	ret
rdumpnames:
	.asciz " EAX="
	.asciz " ECX="
	.asciz " EDX="
	.asciz " EBX="
	.asciz " ESP="
	.asciz " EBP="
	.asciz " ESI="
	.asciz " EDI="
	.asciz " EIP="
	.asciz "  CS="
	.asciz " FLG="
	.asciz "  N ="
	.asciz " CR2="
	.asciz " *STACK* "
puthex32:
	push %ecx
	movw $0x5F00, %ax
	mov $8, %ecx
1:	rol $4, %ebx
	mov %bl, %al
	call puthexchar
	loop 1b
	pop %ecx
	ret
putzstr:
	mov $0x5700, %ax
1:	lodsb
	test %al, %al
	jz 1f
	stosw
	jmp 1b
1:	ret
puthexchar:
	and $0xf, %al
	cmp $0xa, %al
	jl 1f
	add $7, %al
1:	add $'0', %al
	stosw
	ret

.set COM_DATA, 0x3f8
.set COM_INTR, COM_DATA + 1
.set COM_FC, COM_DATA + 2
.set COM_LC, COM_DATA + 3
.set COM_MC, COM_DATA + 4
.set COM_LS, COM_DATA + 5

ix_initcom:
.global ixcom_init
	mov $0, %al
	mov $COM_INTR, %dx
	out %al, %dx
	mov $0x80, %al
	mov $COM_LC, %dx
	out %al, %dx
	mov $0x02, %al
	mov $COM_DATA, %dx
	out %al, %dx
	mov $0, %al
	mov $COM_INTR, %dx
	out %al, %dx
	mov $0x03, %al
	mov $COM_LC, %dx
	out %al, %dx
	mov $0xC7, %al
	mov $COM_FC, %dx
	out %al, %dx
	mov $0x03, %al
	mov $COM_MC, %dx
	out %al, %dx
	ret

ixcom_putc:
.global ixcom_putc
	mov $COM_LS, %dx
	mov %al, %ah
1:	inb %dx, %al
	test $0x20, %al
	jz 1b
	mov $COM_DATA, %dx
	mov %ah, %al
	outb %al, %dx
	ret

ixcom_printz:
1:	lodsb
	test %al, %al
	jz 1f
	call ixcom_putc
	jmp 1b
1:	ret

ixcom_hello:
.global ixcom_hello
	mov $_lc_hello, %esi
	call ixcom_printz
	ret
_lc_hello: .asciz "hello"

_ix_ecentry:
	call ix_initcom
	call ixcom_hello
	ret

__cxa_pure_virtual:
.global __cxa_pure_virtual
	push %cs
	pushf
	pusha
	movw $0x0500+'P', %ax
	movw %ax, 0xC00B8086
	movw $0x0500+'V', %ax
	movw %ax, 0xC00B8088
	call _iv_regdump
	cli
	hlt
_ive_DE:
.global _ive_DE
	pusha
	movw $0x0500+'0', %ax
	movw %ax, 0xC00B8082
	call _iv_regdump
	popa
	iret
_ive_DB:
.global _ive_DB
	pusha
	call _iv_regdump
	movw $0x1C00+'1', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_X2:
.global _ive_X2
	pusha
	call _iv_regdump
	movw $0x1C00+'2', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_BP:
.global _ive_BP
	pusha
	call _iv_regdump
	movw $0x1C00+'3', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_OF:
.global _ive_OF
	pusha
	call _iv_regdump
	movw $0x1C00+'4', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_BR:
.global _ive_BR
	pusha
	call _iv_regdump
	movw $0x1C00+'5', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_UD:
.global _ive_UD
	pusha
	call _iv_regdump
	movw $0x1C00+'6', %ax
	movw %ax, 0xC00B8082
	cli
	hlt
	popa
	iret
_ive_NM:
.global _ive_NM
	pusha
	call _iv_regdump
	movw $0x1C00+'7', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_DF:
.global _ive_DF
	pusha
	call _iv_regdump
	movw $0x6C00+'D', %ax
	movw %ax, 0xC00B8082
	movw $0x6C00+'F', %ax
	movw %ax, 0xC00B8084
	cli
	hlt
_ive_X9:
.global _ive_X9
	pusha
	call _iv_regdump
	movw $0x1C00+'9', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_TS:
.global _ive_TS
	pusha
	call _iv_regdump
	movw $0x1C00+'A', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_NP:
.global _ive_NP
	pusha
	call _iv_regdump
	movw $0x1C00+'B', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_SS:
.global _ive_SS
	pusha
	call _iv_regdump
	movw $0x1C00+'C', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_GP:
.global _ive_GP
	pusha
	call _iv_regdump
	movw $0x1C00+'G', %ax
	movw %ax, 0xC00B8082
	movw $0x1C00+'P', %ax
	movw %ax, 0xC00B8084
	cli
	hlt
	popa
	iret
_ive_PF:
.global _ive_PF
	pusha			# "new" page fault / debug code
	mov %esp, %eax
	push %eax
	add $0x24, %eax
	push %eax
	call _iv_regdump
	add $8, %esp
	movw $0x1C00+'P', %ax
	movw %ax, 0xC00B8082
	movw $0x1C00+'F', %ax
	movw %ax, 0xC00B8084
	call _ix_ecentry
	cli	# just stop
	hlt

	popa
	add $4, %esp
	iret
_ive_X15:
.global _ive_X15
	pusha
	call _iv_regdump
	movw $0x1C00+'F', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_MF:
.global _ive_MF
	pusha
	call _iv_regdump
	movw $0x1C00+'M', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_AC:
.global _ive_AC
	pusha
	call _iv_regdump
	movw $0x1C00+'L', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_ive_MC:
.global _ive_MC
	pusha
	call _iv_regdump
	movw $0x1C00+'M', %ax
	movw %ax, 0xC00B8082
	cli
	hlt
_ive_XM:
.global _ive_XM
	pusha
	call _iv_regdump
	movw $0x1C00+'X', %ax
	movw %ax, 0xC00B8082
	popa
	iret
_iv_irq0:
.global _iv_irq0
	pusha
	call _iv_add_n
	mov $0, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq1:
.global _iv_irq1
	pusha
	call _iv_add_n
	lea _ivix_irq1_fn, %esi
	mov (%esi), %eax
	test %eax, %eax
	je 1f
	call *%eax
	1:
	mov $1, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq2:
.global _iv_irq2
	pusha
	call _iv_add_n
	mov $2, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq3:
.global _iv_irq3
	pusha
	call _iv_add_n
	mov $3, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq4:
.global _iv_irq4
	pusha
	call _iv_add_n
	mov $4, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq5:
.global _iv_irq5
	pusha
	call _iv_add_n
	mov $5, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq6:
.global _iv_irq6
	pusha
	call _iv_add_n
	mov $6, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq7:
.global _iv_irq7
	pusha
	call _iv_add_n
	mov $7, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq8:
.global _iv_irq8
	pusha
	call _iv_add_n
	mov $8, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq9:
.global _iv_irq9
	pusha
	call _iv_add_n
	mov $9, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq10:
.global _iv_irq10
	pusha
	call _iv_add_n
	mov $10, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq11:
.global _iv_irq11
	pusha
	call _iv_add_n
	mov $11, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq12:
.global _iv_irq12
	pusha
	call _iv_add_n
	lea _ivix_irq12_fn, %esi
	mov (%esi), %eax
	test %eax, %eax
	je 1f
	call *%eax
	1:
	mov $12, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq13:
.global _iv_irq13
	pusha
	call _iv_add_n
	mov $13, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq14:
.global _iv_irq14
	pusha
	call _iv_add_n
	mov $14, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq15:
.global _iv_irq15
	pusha
	call _iv_add_n
	mov $15, %al
	call _iv_irq_eoi
	popa
	iret
_iv_irq_eoi:
	cmp $8, %al
	mov $0x20, %al
	jl 1f
	out %al, $PIC2_CMD
1:	out %al, $PIC1_CMD
	ret
_iv_add_n:
	movl $_ivix_int_n, %edx
	movl 0(%edx), %eax
	add $1, %eax
	movl %eax, _ivix_int_n
	ret
_iv_nothing:
.global _iv_nothing
	iret

_ix_outb:
	.global _ix_outb
	.type _ix_outb,@function
	mov 4(%esp), %edx
	mov 8(%esp), %eax
	outb %al, %dx
	ret

_ix_outw:
	.global _ix_outw
	.type _ix_outw,@function
	mov 4(%esp), %edx
	mov 8(%esp), %eax
	outw %ax, %dx
	ret

_ix_outl:
	.global _ix_outl
	.type _ix_outl,@function
	mov 4(%esp), %edx
	mov 8(%esp), %eax
	outl %eax, %dx
	ret

_ix_loadcr3:
	.global _ix_loadcr3
	.type _ix_loadcr3,@function
	mov 4(%esp), %eax	# load CR3 with the page table
	movl %eax, %cr3
	ret

_ix_inb:
	.global _ix_inb
	.type _ix_inb,@function
	mov 4(%esp), %edx
	xor %eax, %eax
	inb %dx, %al
	ret

_ix_inw:
	.global _ix_inw
	.type _ix_inw,@function
	mov 4(%esp), %edx
	xor %eax, %eax
	inw %dx, %ax
	ret

_ix_inl:
	.global _ix_inl
	.type _ix_inl,@function
	mov 4(%esp), %edx
	xor %eax, %eax
	inl %dx, %eax
	ret

_ix_makeidt:
	movl $_ivix_idt, %edi
	movl $_ivix_itb, %edx
	movzxw 0(%edx), %ebx
	add $2, %edx
1:	movzxw 0(%edx), %eax
	movzxw 2(%edx), %ecx
	add $4, %edx
	shl $3, %eax
	movl $_ivix_itable, %esi
	add %eax, %esi
2:	movw 0(%esi), %ax	# scramble the values into the IDT entry format
	movw %ax, 0(%edi)
	movw 2(%esi), %ax
	movw %ax, 6(%edi)
	movw 4(%esi), %ax
	movw %ax, 2(%edi)
	movw 6(%esi), %ax
	movw %ax, 4(%edi)
	add $8, %edi
	loop 2b
	dec %ebx
	and %ebx, %ebx
	jne 1b
	ret

_ix_construct:
	mov $.ctors.end - 4, %ebx
	jmp 2f
1:	sub $4, %ebx
	call *%eax
2:	mov (%ebx), %eax
	cmp $0xffffffff, %eax
	jne 1b
	ret

_ix_initpic:	# setup the PICs
	mov $0x11, %al
	out %al, $PIC1_CMD	# init the PICs
	out %al, $PIC2_CMD
	call RETN
	mov $0x70, %al
	out %al, $PIC1_DAT	# set offsets
	mov $0x78, %al
	out %al, $PIC2_DAT
	call RETN
	mov $0x04, %al
	out %al, $PIC1_DAT	# set cascades
	mov $0x02, %al
	out %al, $PIC2_DAT
	call RETN
	mov $0x01, %al
	out %al, $PIC1_DAT	# set mode
	out %al, $PIC2_DAT
	call RETN
	xor %ax, %ax
	out %al, $PIC1_DAT	# set mask (allow all)
	out %al, $PIC2_DAT
RETN:	ret

_ix_totalhalt:	# totally halt the system. well, except if interrupts are on.
	.global _ix_totalhalt
1:	hlt
	jmp 1b

_ix_halt:	# halt the system. wait for interrupt, if they are on.
	.global _ix_halt
	hlt
	ret

_ix_reqr:
	.global _ix_reqr
	push %ebx
	push %ecx
	xor %eax, %eax
	mov %eax, %ebx
	mov %eax, %ecx
	dec %ecx
1:	dec %ebx
	test %ebx, %ebx
	jne 1b
	loop 1b
	pop %ecx
	pop %ebx
	ret

_ix_req:
	.global _ix_req
	xor %eax, %eax
	movl %eax, 0x520
	ret

.data
.align 8
_ivix_idt:	# the IDT itself
.global _ivix_idt
.fill 2048

_ivix_gtable:
.word 0

.macro ITBENT fn=0,count=1
.word \fn
.word \count
.endm
.macro ITBARRY sfn=0,efn=0
.word \sfn
.word 1
.if \efn-\sfn
ITBARRY "(\sfn+1)",\efn
.endif
.endm

_ivix_itb:	# how to build the IDT
.word (_ivix_itb_end - _ivix_itb_start) >> 2	# count
_ivix_itb_start:
# 0 - 1f
ITBARRY 0x00, 0x1f
# 20 - 6f
ITBENT 0x30, 0x50
# 70 - 7f
ITBARRY 0x20, 0x2f
# 80 - ff
ITBENT 0x30, 128
_ivix_itb_end:

.set KSEG, 0x10
_ivix_itable:	# functions to use in IDT
idt_int _ive_DE, KSEG
idt_int _ive_DB, KSEG
idt_int _ive_X2, KSEG
idt_int _ive_BP, KSEG	# 3
idt_int _ive_OF, KSEG
idt_int _ive_BR, KSEG
idt_int _ive_UD, KSEG
idt_int _ive_NM, KSEG	# 7
idt_int _ive_DF, KSEG
idt_int _ive_X9, KSEG
idt_int _ive_TS, KSEG
idt_int _ive_NP, KSEG	# 0x0b
idt_int _ive_SS, KSEG
idt_int _ive_GP, KSEG
idt_int _ive_PF, KSEG
idt_int _ive_X15, KSEG	# 0x0f
idt_int _ive_MF, KSEG
idt_int _ive_AC, KSEG
idt_int _ive_MC, KSEG
idt_int _ive_XM, KSEG	# 0x13
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG	# 0x17
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG	# 0x1b
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG	# 0x1f
idt_int _iv_irq0, KSEG	# 0x20
idt_int _iv_irq1, KSEG
idt_int _iv_irq2, KSEG
idt_int _iv_irq3, KSEG	# 0x23
idt_int _iv_irq4, KSEG
idt_int _iv_irq5, KSEG
idt_int _iv_irq6, KSEG
idt_int _iv_irq7, KSEG	# 0x27
idt_int _iv_irq8, KSEG
idt_int _iv_irq9, KSEG
idt_int _iv_irq10, KSEG
idt_int _iv_irq11, KSEG	# 0x2b
idt_int _iv_irq12, KSEG
idt_int _iv_irq13, KSEG
idt_int _iv_irq14, KSEG
idt_int _iv_irq15, KSEG	# 0x2f
idt_int _iv_nothing, KSEG

