/* ***
 * i686/ixkm.s - Initialization and main entry for the kernel
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
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
.set PIT_CH0, 0x40
.set PIT_CH1, 0x41
.set PIT_CH2, 0x42
.set PIT_CMD, 0x43

.macro idt_trap offs=0, sel=0, dpl=0
.int \offs
.word \sel
.byte 0
.byte 0b10001111 | (\dpl << 5)
.endm
.macro idt_int offs=0, sel=0, dpl=0
.int \offs
.byte 0
.byte 0b10001110 | (\dpl << 5)
.word \sel
.endm

.set KSEG, 0x10

_ivix_idt_ptr:
.word 0x07ff
.int _ivix_idt

_ivix_gdt_ptr:
.word 0x0100 * 8
.int gdt_data	# linked externally

_ivix_dump_n:
.int 0
_ivix_int_n: .global _ivix_int_n
.int 0
_ivix_int_f: .global _ivix_int_f
.int 0
_ivix_int_ac: .global _ivix_int_ac
.int 0

.section .ixboot,"ax"
	.int 0xeca70433
_ix_entry:
	.global _ix_entry
	leal _kernel_phy_end, %edx	# set up page directory at end of kernel
	mov %edx, %edi	# save this for later
	orl $0x1, %edx	# present flag
	xor %eax, %eax	# high bits are zero
	movl %edx, _ivix_phy_pdpt + 0x0 # map 0x0..
	movl %eax, _ivix_phy_pdpt + 0x4
	movl %edx, _ivix_phy_pdpt + 0x18 # map 0xC..
	movl %eax, _ivix_phy_pdpt + 0x1c
	xor %edx, %edx
	orl $0x83, %eax		# set flags on it 2M pages
	movl %eax, (%edi)	# identity map 0 - 2M
	movl %edx, 4(%edi)
	addl $0x200000, %eax 	# identity map 2M - 4M
	movl %eax, 8(%edi)
	movl %edx, 12(%edi)
	movl %cr4, %eax # control bits
	orl $0x20, %eax	# enable PAE
	movl %eax, %cr4
	leal _ivix_phy_pdpt, %eax	# load CR3 with the page table
	movl %eax, %cr3
	movl %cr0, %eax
	orl $0x80000000, %eax	# enable paging bit
	movl %eax, %cr0		# paging is on past this point!
1:
	jmp _kernel_entry
.align 32
_ivix_phy_pdpt:
.global _ivix_phy_pdpt
.set _ivix_pdpt, _ivix_phy_pdpt + 0xc0000000
.global _ivix_pdpt
.fill 64, 2, 0xfe00

.section .ctors.begin
.int 0xffffffff
.section .ctors.end

.text
_kernel_entry:
	mov $.bss, %edi
	mov $_kernel_end, %ecx  # zero out teh .bss
	sub %edi, %ecx
	shr $2, %ecx
	xor %eax, %eax
	rep stosl %eax, (%edi)

	mov $ixstk, %esp
	lgdt _ivix_gdt_ptr	# load GDT
	call _ix_makeidt	# make and load IDT
	lidt _ivix_idt_ptr
	call _ix_initpic	# configure the PIC

	sub $0xc, %esp		# align the stack
	call _ix_construct	# call ctors
	call _ix_ecentry	# start the comm port
	#sti			# interupts on
	call _kernel_main	# jump to C/C++
	jmp _ix_halt		# idle loop if we get here

_iv_regdump:
	call ixcom_putc_x
	movb $'+', %al
	call ixcom_putc_x
	push %ebp
	mov %esp, %ebp
	lea 0x08(%esp), %ecx
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
	call putzstr
	lea 0x2c(%esp), %ecx
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
	call putzstr
	mov %cr2, %ebx
	call puthex32
	call putzstr
	lea 0x34(%esp), %esi	# ESP
	mov $32, %edi
2:	and %edi, %edi
	je 3f
	mov $10, %al
	call ixcom_putc_x
	mov %esi, %ebx
	call puthex32
1:	mov 0(%esi), %ebx
	call puthex32
	add $4, %esi
	sub $1, %edi
	test $3, %di
	je 2b
	and %edi, %edi
	jne 1b
3:
	pop %ebp
	ret
rdumpnames:
	.asciz "\n EAX="
	.asciz " ECX="
	.asciz " EDX="
	.asciz " EBX="
	.asciz "\n ESP="
	.asciz " EBP="
	.asciz " ESI="
	.asciz " EDI="
	.asciz "\n EIP="
	.asciz "  CS="
	.asciz " FLG="
	.asciz "  N ="
	.asciz "\n CR2="
	.asciz "\n *STACK*"
puthex32:
	push %ecx
	mov $8, %ecx
1:	rol $4, %ebx
	mov %bl, %al
	and $0xf, %al
	cmp $0xa, %al
	jl 2f
	add $7, %al
2:	add $'0', %al
	call ixcom_putc_x
	loop 1b
	pop %ecx
	ret
putzstr:
1:	lodsb
	test %al, %al
	jz 1f
	call ixcom_putc_x
	jmp 1b
1:	ret

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
	movl 4(%esp), %eax
ixcom_putc_x:
	mov $COM_LS, %dx
	mov %al, %ah
1:	inb %dx, %al
	test $0x20, %al
	jz 1b
	mov $COM_DATA, %dx
	mov %ah, %al
	outb %al, %dx
	cmp $10, %ah
	je 1f
	ret
1:	mov $13, %al
	jmp ixcom_putc_x

ixcom_printz:
1:	lodsb
	test %al, %al
	jz 1f
	push %eax
	call ixcom_putc
	pop %eax
	jmp 1b
1:	ret

ixcom_hello:
.global ixcom_hello
	mov $_lc_hello, %esi
	call ixcom_printz
	ret
_lc_hello: .asciz "xivix hello\n"
_lc_totalhalt: .asciz "totalhalt\n"

_ix_ecentry:
	call ix_initcom
	call ixcom_hello
	ret

__cxa_pure_virtual:
.global __cxa_pure_virtual
	push %cs
	pushf
	pusha
	movw $0x0500+'V', %ax
	call _iv_regdump
	call _ix_totalhalt
_ive_DE:
.global _ive_DE
	pusha
	movw $0x0500+'0', %ax
	call _iv_regdump
	popa
	iret
_ive_DB:
.global _ive_DB
	pusha
	movw $0x1C00+'1', %ax
	call _iv_regdump
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
	movw $0x1C00+'3', %ax
	call _iv_regdump
	popa
	iret
_ive_OF:
.global _ive_OF
	pusha
	movw $0x1C00+'4', %ax
	call _iv_regdump
	popa
	iret
_ive_BR:
.global _ive_BR
	pusha
	movw $0x1C00+'5', %ax
	call _iv_regdump
	popa
	iret
_ive_UD:
.global _ive_UD
	pusha
	movw $0x1C00+'6', %ax
	call _iv_regdump
	call _ix_totalhalt
	popa
	iret
_ive_NM:
.global _ive_NM
	pusha
	movw $0x1C00+'7', %ax
	call _iv_regdump
	popa
	iret
_ive_DF:
.global _ive_DF
	pusha
	movw $0x6C00+'D', %ax
	call _iv_regdump
	movw $0x6C00+'F', %ax
	movw %ax, 0xC00B8084
	call _ix_totalhalt
_ive_X9:
.global _ive_X9
	pusha
	movw $0x1C00+'9', %ax
	call _iv_regdump
	popa
	iret
_ive_TS:
.global _ive_TS
	pusha
	movw $0x1C00+'A', %ax
	call _iv_regdump
	popa
	iret
_ive_NP:
.global _ive_NP
	pusha
	movw $0x1C00+'B', %ax
	call _iv_regdump
	popa
	iret
_ive_SS:
.global _ive_SS
	pusha
	movw $0x1C00+'C', %ax
	call _iv_regdump
	popa
	iret
_ive_GP:
.global _ive_GP
	pusha
	mov %esp, %eax
	push %eax
	add $0x24, %eax
	push %eax
	movw $0x1C00+'G', %ax
	call _iv_regdump
	movw $0x1C00+'P', %ax
	movw %ax, 0xC00B8084
	call _ix_totalhalt
	popa
	iret
_ive_PF:
.global _ive_PF
	pusha			# "new" page fault / debug code
	movw $0x1C00+'P', %ax
	call _iv_regdump
	# stack:
	# Hex,Item
	# +34 SS (would go here for CPL changes)
	# +30 ESP (would go here for CPL changes)
	# +2C EFLAGS
	# +28 CS
	# +24 EIP
	# +20 Error Code (Exceptions)
	# +1C EAX
	# +18 ECX
	# +14 EDX
	# +10 EBX
	# +0C ESP (pointing to error code)
	# +08 EBP
	# +04 ESI
	# +00 EDI <- Where current ESP ends up pointing
	mov %esp, %eax
	mov %eax, %edx
	add $0x24, %edx
	push %edx         # edx = ptr to EIP,CS,EFLAGS
	push %eax         # eax = ptr to register dump
	mov %cr2, %edx    # PF address
	push %edx
	mov 0x20(%eax), %edx # edx = error code
	push %edx
	mov %esp, %edx    # edx = ptr to all this stuff
	mov $0x0e, %eax
	# +C ptr to EIP,CS,EFLAGS
	# +8 ptr to registers
	# +4 PF address
	# +0 Error code (again) <- where edx points
	# eax = 0x0E
	# edx = ptr (above)
	call _iv_exhload
	call _ix_totalhalt	# just stop
	popa
	add $4, %esp
	iret
_ive_X15:
.global _ive_X15
	pusha
	movw $0x1C00+'F', %ax
	call _iv_regdump
	popa
	iret
_ive_MF:
.global _ive_MF
	pusha
	movw $0x1C00+'M', %ax
	call _iv_regdump
	popa
	iret
_ive_AC:
.global _ive_AC
	pusha
	movw $0x1C00+'L', %ax
	call _iv_regdump
	popa
	iret
_ive_MC:
.global _ive_MC
	pusha
	movw $0x1C00+'M', %ax
	call _iv_regdump
	call _ix_totalhalt
_ive_XM:
.global _ive_XM
	pusha
	movw $0x1C00+'X', %ax
	call _iv_regdump
	popa
	iret

_iv_exhload:
	# edx holds struct ptr, eax holds exception#
	lea ivix_except(,%eax,8), %esi # load address of ex handler info
	mov (%esi), %ebx    # get address
	test %ebx, %ebx     # test if null
	je 1f
	push %edx           # push the exception and struct ptr
	push %eax
	movl 4(%esi), %eax  # get the locals ptr param
	push %eax           # push that on there
	call *%ebx          # call the handler
	add $12, %esp       # clean up this mess
	1:
	ret

_iv_irqload:
	mov %esp, %edx
	mov (%esp), %eax
	lea ivix_interrupt(,%eax,8), %esi
	mov (%esi), %ebx
	test %ebx, %ebx
	je 1f
	push %edx
	push %eax
	movl 4(%esi), %eax
	push %eax
	call *%ebx
	add $12, %esp
	1:
	pop %eax
	call _iv_irq_eoi
	popa
	iret

_iv_apic_irq:
	mov %esp, %edx	# check interrupt table for IRQ number
	mov (%esp), %eax
	lea ivix_interrupt(,%eax,8), %esi
	mov (%esi), %ebx
	test %ebx, %ebx	# if it has a handler, call it
	je 1f
	push %edx
	push %eax
	movl 4(%esi), %eax
	push %eax
	call *%ebx
	add $12, %esp
1:
	pop %eax
	call _apic_eoi
	popa
	iret

_iv_apic_timer:
	pusha
	push $0x40
	call _iv_add_f
	jmp _iv_apic_irq
_iv_apic_sv:
	pusha
	push $0x41
	call _iv_add_ac
	call _apic_svh
	jmp _iv_apic_irq

.macro AIRQ num
_iv_airq\num:
	pusha
	push $0x\num
	jmp _iv_apic_irq
.endm
AIRQ 20
AIRQ 21
_iv_airq22:
	pusha
	push $0x22
	call _iv_add_n
	jmp _iv_apic_irq
AIRQ 23
AIRQ 24
AIRQ 25
AIRQ 26
AIRQ 27
_iv_airq28:
	pusha
	push $0x28
	call _iv_add_ac
	jmp _iv_apic_irq
AIRQ 29
AIRQ 2a
AIRQ 2b
AIRQ 2c
AIRQ 2d
AIRQ 2e
AIRQ 2f
AIRQ 30
AIRQ 31
AIRQ 32
AIRQ 33
AIRQ 34
AIRQ 35
AIRQ 36
AIRQ 37
AIRQ 38
AIRQ 39
AIRQ 3a
AIRQ 3b
AIRQ 3c
AIRQ 3d
AIRQ 3e
AIRQ 3f

_iv_irq0:
.global _iv_irq0
	pusha
	push $0
	call _iv_add_n
	jmp _iv_irqload
_iv_irq1:
.global _iv_irq1
	pusha
	push $1
	jmp _iv_irqload
_iv_irq2:
.global _iv_irq2
	pusha
	push $2
	jmp _iv_irqload
_iv_irq3:
.global _iv_irq3
	pusha
	push $3
	jmp _iv_irqload
_iv_irq4:
.global _iv_irq4
	pusha
	push $4
	jmp _iv_irqload
_iv_irq5:
.global _iv_irq5
	pusha
	push $5
	jmp _iv_irqload
_iv_irq6:
.global _iv_irq6
	pusha
	push $6
	jmp _iv_irqload
_iv_irq7:
.global _iv_irq7
	pusha
	push $7
	jmp _iv_irqload
_iv_irq8:
.global _iv_irq8
	pusha
	push $8
	jmp _iv_irqload
_iv_irq9:
.global _iv_irq9
	pusha
	push $9
	jmp _iv_irqload
_iv_irq10:
.global _iv_irq10
	pusha
	push $10
	jmp _iv_irqload
_iv_irq11:
.global _iv_irq11
	pusha
	push $11
	jmp _iv_irqload
_iv_irq12:
.global _iv_irq12
	pusha
	push $12
	jmp _iv_irqload
_iv_irq13:
.global _iv_irq13
	pusha
	push $13
	jmp _iv_irqload
_iv_irq14:
.global _iv_irq14
	pusha
	push $14
	jmp _iv_irqload
_iv_irq15:
.global _iv_irq15
	pusha
	push $15
	jmp _iv_irqload
_iv_sti:
.global _iv_sti
	sti
	ret
_iv_irq_eoi:
	cmp $8, %al
	mov $0x20, %al
	jl 1f
	out %al, $PIC2_CMD
1:	out %al, $PIC1_CMD
	ret
_iv_add_n:
	incl _ivix_int_n
	ret
_iv_add_f:
	incl _ivix_int_f
	ret
_iv_add_ac:
	incl _ivix_int_ac
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

_ix_readmsr:
	.global _ix_readmsr
	mov 4(%esp), %ecx
	rdmsr
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

_ix_ldnw:
	.global _ix_ldnw
	.type _ix_ldnw,@function
	movw 0(%ecx), %ax
	xchg %ah,%al
	ret

_ix_bswapw:
	.global _ix_bswapw
	.type _ix_bswapw,@function
	xchg %ah,%al
	ret

_ix_bswapi:
	.global _ix_bswapi
	.type _ix_bswapi,@function
	bswap %eax
	ret

_ixa_inc:
	.global _ixa_inc
	.type _ixa_inc,@function
	mov 4(%esp), %eax
	lock incl (%eax)
	ret

_ixa_dec:
	.global _ixa_dec
	.type _ixa_dec,@function
	mov 4(%esp), %eax
	lock decl (%eax)
	ret

_ixa_or:
	.global _ixa_or
	.type _ixa_or,@function
	mov 4(%esp), %edx
	mov 8(%esp), %eax
	lock or %eax, (%edx)
	ret

_ixa_xor:
	.global _ixa_xor
	.type _ixa_xor,@function
	mov 4(%esp), %edx
	mov 8(%esp), %eax
	lock xor %eax, (%edx)
	ret

_ixa_xchg:
	.global _ixa_xchg
	.type _ixa_xchg,@function
	mov 4(%esp), %edx
	mov 8(%esp), %eax
	lock xchg %eax, (%edx)
	ret

_ixa_cmpxchg:
	.global _ixa_cmpxchg
	.type _ixa_cmpxchg,@function
	push %ebp
	mov 0x8(%esp), %edx
	mov 0xc(%esp), %eax
	mov 0x10(%esp), %ebp
	lock cmpxchg %ebp, (%edx)
	pop %ebp
	ret

_ix_makeidt:
	movl $_ivix_idt, %esi	# the IDT to fill
	mov $0x100, %ecx
	# itable descriptors are size 8
	#movl $_ivix_itable, %esi
	# IDT Descriptor:
	# Offset is the virt-address (offset from CS base)
	# word 0x0: offset(0..15)
	# word 0x2: CS segment to use for interrupt
	# word 0x4:
	# [15|14 13|12 11|10 9 8] [7 6 5|4 3 2 1 0]
	#  P   DPL   0  D  type    0 0 0  reserved
	# P = Present, should be 1 for in-use descriptors
	# DPL = Descriptor Privilege Level
	#  checked by INT n, INT, INTO, aka software interrupts, #GP if violated
	# D: 1 = 32 bit gate, 0 = 16 bit (for Interrupt/Trap gate, otherwise 0)
	# Type: 5=task gate, 6 interrupt gate, 7 trap gate
	# word 0x6: offset(16..31)
1:
	# scramble the values into the IDT entry format
	movw 2(%esi), %ax	# exchange word 2 and 6
	xchgw %ax, 6(%esi)
	movw %ax, 2(%esi)
	add $8, %esi
	loop 1b
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

_ix_initpic:

	# also setup the PIT, because why not
	# byte: ccaa ooob
	# cc = channel 0-2 or 3 readback
	# aa = access: 0 latch, lo, hi, 3 lo/hi
	# ooo= opmode: int on term, hw retrig, rate gen, sqwave
	#   "       ": soft trig st, hw trig st, rate gen, sqwave
	# b  = 0=bin, 1=bcd (yuck)
	mov $0x14, %al # chan 0, lo, rate gen
	out %al, $PIT_CMD
	mov $0xa9, %al
	out %al, $PIT_CH0
	mov $0x24, %al
	out %al, $PIT_CMD
	mov $0x04, %al
	out %al, $PIT_CH0
	# setup the PICs
	mov $0x11, %al	# initialize(ICW2 offset, ICW3 wired, ICW4)
	out %al, $PIC1_CMD	# init the PICs
	out %al, $PIC2_CMD
	call RETN
	mov $0x20, %al	# master's offset
	out %al, $PIC1_DAT	# set offsets
	mov $0x28, %al	# slave's offset
	out %al, $PIC2_DAT
	call RETN
	mov $0x04, %al	# idk
	out %al, $PIC1_DAT	# set cascades
	mov $0x02, %al
	out %al, $PIC2_DAT
	call RETN
	mov $0x01, %al	# 8086/88 mode... I guess
	out %al, $PIC1_DAT	# set mode
	out %al, $PIC2_DAT
	call RETN
	xor %eax, %eax		# allow all
	out %al, $PIC1_DAT	# set mask
	xor %eax, %eax
	out %al, $PIC2_DAT
	call RETN
	call _apic_eoi
	# disable?
	mov $0xfe, %al
	out %al, $PIC2_DAT
	call RETN
	mov $0xfb, %al
	out %al, $PIC1_DAT
	call RETN
RETN:	ret

_ix_totalhalt:	# totally halt the system. well, except if interrupts are on.
	.global _ix_totalhalt
	mov %eax, %ebx
	call puthex32
	mov $_lc_totalhalt, %esi
	call ixcom_printz
	mov 0(%esp), %ebx
	call puthex32
	cli
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

.section .idt

.align 8
_ivix_idt:	# the IDT itself
.global _ivix_idt
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
idt_int _iv_nothing, KSEG # 0x30
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG # 0x33
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG # 0x37
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG # 0x3b
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG # 0x3f
idt_int _iv_airq20, KSEG # 0x40
idt_int _iv_airq21, KSEG
idt_int _iv_airq22, KSEG
idt_int _iv_airq23, KSEG # 0x43
idt_int _iv_airq24, KSEG
idt_int _iv_airq25, KSEG
idt_int _iv_airq26, KSEG
idt_int _iv_airq27, KSEG # 0x47
idt_int _iv_airq28, KSEG
idt_int _iv_airq29, KSEG
idt_int _iv_airq2a, KSEG
idt_int _iv_airq2b, KSEG # 0x4b
idt_int _iv_airq2c, KSEG
idt_int _iv_airq2d, KSEG
idt_int _iv_airq2e, KSEG
idt_int _iv_airq2f, KSEG # 0x4f
idt_int _iv_airq30, KSEG # 0x50
idt_int _iv_airq31, KSEG
idt_int _iv_airq32, KSEG
idt_int _iv_airq33, KSEG # 0x53
idt_int _iv_airq34, KSEG
idt_int _iv_airq35, KSEG
idt_int _iv_airq36, KSEG
idt_int _iv_airq37, KSEG # 0x57
idt_int _iv_airq38, KSEG
idt_int _iv_airq39, KSEG
idt_int _iv_airq3a, KSEG
idt_int _iv_airq3b, KSEG # 0x5b
idt_int _iv_airq3c, KSEG
idt_int _iv_airq3d, KSEG
idt_int _iv_airq3e, KSEG
idt_int _iv_airq3f, KSEG # 0x5f
.fill (0xf0 - 0x60), 8, 0
idt_int _iv_nothing, KSEG # 0xf0
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG # 0xf3
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG # 0xf7
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG # 0xfb
idt_int _iv_nothing, KSEG
idt_int _iv_nothing, KSEG
idt_int _iv_apic_timer, KSEG
idt_int _iv_apic_sv, KSEG # 0xff

_ivix_gtable:
.word 0

