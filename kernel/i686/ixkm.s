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

_iv_idt_ptr:
.word 0x07ff
.int _iv_idt

_iv_gdt_ptr:
.word 0x0100 * 8
.int gdt_data	# linked externally

_iv_int_n: .global _iv_int_n
.int 0
_iv_int_f: .global _iv_int_f
.int 0
_iv_int_ac: .global _iv_int_ac
.int 0

.section .ixboot,"ax"
	.int 0xeca70433
_ix_entry:
	.global _ix_entry
	leal _kernel_phy_end, %edx	# set up page directory at end of kernel
	mov %edx, %edi	# save this for later
	orl $0x1, %edx	# present flag
	xor %eax, %eax	# high bits are zero
	movl %edx, _iv_phy_pdpt + 0x0 # map 0x0..
	movl %eax, _iv_phy_pdpt + 0x4
	movl %edx, _iv_phy_pdpt + 0x18 # map 0xC..
	movl %eax, _iv_phy_pdpt + 0x1c
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
	leal _iv_phy_pdpt, %eax	# load CR3 with the page table
	movl %eax, %cr3
	movl %cr0, %eax
	orl $0x80000000, %eax	# enable paging bit
	movl %eax, %cr0		# paging is on past this point!
1:
	jmp _kernel_entry
.align 32
_iv_phy_pdpt:
.global _iv_phy_pdpt
.set _iv_pdpt, _iv_phy_pdpt + 0xc0000000
.global _iv_pdpt
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
	lgdt _iv_gdt_ptr	# load GDT
	call _ix_makeidt	# make and load IDT
	lidt _iv_idt_ptr
	call _ix_initpic	# configure the PIC

	sub $0xc, %esp		# align the stack
	call _ix_construct	# call ctors
	call _ix_ecentry	# start the comm port
	#sti			# interupts on
	call _kernel_main	# jump to C/C++
	jmp _ix_halt		# idle loop if we get here

_ix_regdump:
	call ix_com_putc_x
	movb $'+', %al
	call ix_com_putc_x
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
	lea 0x30(%esp), %ecx
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
	call ix_com_putc_x
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
	call ix_com_putc_x
	loop 1b
	pop %ecx
	ret
putzstr:
1:	lodsb
	test %al, %al
	jz 1f
	call ix_com_putc_x
	jmp 1b
1:	ret

.set COM_DATA, 0x3f8
.set COM_INTR, COM_DATA + 1
.set COM_FC, COM_DATA + 2
.set COM_LC, COM_DATA + 3
.set COM_MC, COM_DATA + 4
.set COM_LS, COM_DATA + 5

ix_initcom:
.global ix_com_init
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

ix_com_putc:
.global ix_com_putc
	movl 4(%esp), %eax
ix_com_putc_x:
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
	jmp ix_com_putc_x

ix_com_printz:
1:	lodsb
	test %al, %al
	jz 1f
	push %eax
	call ix_com_putc
	pop %eax
	jmp 1b
1:	ret

ix_com_hello:
#.global ix_com_hello
	mov $_lc_hello, %esi
	call ix_com_printz
	ret
_lc_hello: .asciz "xivix hello\n"
_lc_totalhalt: .asciz "totalhalt\n"

_ix_ecentry:
	call ix_initcom
	call ix_com_hello
	ret

__cxa_pure_virtual:
.global __cxa_pure_virtual
	push %cs
	pushf
	push $0
	push $0
	pusha
	movw $0x0500+'V', %ax
	call _ix_regdump
	call _ix_totalhalt
_ixe_DE:
	push $0
	pusha
	movw $0x0500+'0', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_DB:
	push $0
	pusha
	movw $0x1C00+'1', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_X2:
	push $0
	pusha
	call _ix_regdump
	movw $0x1C00+'2', %ax
	movw %ax, 0xC00B8082
	popa
	add $8, %esp
	iret
_ixe_BP:
	push $0
	pusha
	movw $0x1C00+'3', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_OF:
	push $0
	pusha
	movw $0x1C00+'4', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_BR:
	push $0
	pusha
	movw $0x1C00+'5', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_UD:
	push $0
	pusha
	movw $0x1C00+'6', %ax
	call _ix_regdump
	call _ix_totalhalt
	popa
	add $8, %esp
	iret
_ixe_NM:
	push $0
	pusha
	movw $0x1C00+'7', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_DF:
	push $0
	pusha
	movw $0x6C00+'D', %ax
	call _ix_regdump
	movw $0x6C00+'F', %ax
	movw %ax, 0xC00B8084
	call _ix_totalhalt
_ixe_X9:
	push $0
	pusha
	movw $0x1C00+'9', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_TS:
	push $0
	pusha
	movw $0x1C00+'A', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_NP:
	push $0
	pusha
	movw $0x1C00+'B', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_SS:
	push $0
	pusha
	movw $0x1C00+'C', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_GP:
	push $0
	pusha
	mov %esp, %eax
	push %eax
	add $0x24, %eax
	push %eax
	movw $0x1C00+'G', %ax
	call _ix_regdump
	movw $0x1C00+'P', %ax
	movw %ax, 0xC00B8084
	call _ix_totalhalt
	popa
	add $8, %esp
	iret
# stack:
# Hex,Item
# +38 SS (would go here for CPL changes)
# +34 ESP (would go here for CPL changes)
# +30 EFLAGS
# +2c CS
# +28 EIP
# +24 Error Code (Exceptions)
# +20 extra code (i.e. CR2 for PF) else 0
# +1C EAX
# +18 ECX
# +14 EDX
# +10 EBX
# +0C ESP (pointing to error code)
# +08 EBP
# +04 ESI
# +00 EDI <- Where current ESP ends up pointing
_ixe_PF:
	push $0
	pusha			# "new" page fault / debug code
	movw $0x1C00+'P', %ax
	call _ix_regdump
	mov %esp, %edx
	mov %cr2, %eax    # PF address
	mov %eax, 0x20(%edx)
	mov $0x0e, %eax
	# eax = 0x0E
	# edx = ptr (above)
	call _ix_exhload
	call _ix_totalhalt	# just stop
	popa
	add $8, %esp
	iret
_ixe_X15:
	push $0
	pusha
	movw $0x1C00+'F', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_MF:
	push $0
	pusha
	movw $0x1C00+'M', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_AC:
	push $0
	pusha
	movw $0x1C00+'L', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret
_ixe_MC:
	push $0
	pusha
	movw $0x1C00+'M', %ax
	call _ix_regdump
	call _ix_totalhalt
_ixe_XM:
	push $0
	pusha
	movw $0x1C00+'X', %ax
	call _ix_regdump
	popa
	add $8, %esp
	iret

_ix_exhload: # eax = Ex index, edx = context pointer
	# edx holds struct ptr, eax holds exception#
	lea iv_except(,%eax,8), %esi # load address of ex handler info
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

# IRQ handler: interrupt stack, push #pusha
# 0x30 eflags
# 0x2c cs
# 0x28 eip
# 0x24 pushed irq # <- entry ESP
# 0x20 zero
# 0x1c EAX
# 0x18 ECX
# 0x14 EDX
# 0x10 EBX
# 0x0c ESP (entry esp)
# 0x08 EBP
# 0x04 ESI
# 0x00 EDI <- Where current ESP ends up pointing
#
_ix_apic_irq:
	push $0
	pusha
	mov %esp, %edx	# registers
	# check interrupt table for IRQ number
	mov 0x24(%esp), %eax	# interrupt number
	lea iv_interrupt(,%eax,8), %esi	# IntrDef structure
	mov (%esi), %ebx	# ->entry (function ptr)
	test %ebx, %ebx
	je 1f	# if it has no handler, skip and return
	push %edx	# pointer to registers
	push %eax	# our interrupt number
	movl 4(%esi), %eax	# ->rlocal (void *)
	push %eax
	call *%ebx
	add $12, %esp	# clean up handler stack
1:	mov 0x20(%esp), %eax	# interrupt number
	cmp $0x10, %al	# if it's 0-15, call PIC EOI
	jge 1f
	call _ix_irq_eoi
1:	call _apic_eoi
	mov 0x24(%esp), %eax	# interrupt number
	cmp $0x28, %al	# is it the timer?
	jne 1f
	push %esp	# attempt task switch on timer
	call _Z24scheduler_from_interruptP6IntCtx
	add $4, %esp	# if we didn't task switch
1:	popa
	add $8, %esp	# skip the pushed values
	iret

_ix_apic_svh:
	pusha
	call _apic_svh
	popa
	ret

_ix_apic_timer:
	push $0x40
	incl _iv_int_f
	jmp _ix_apic_irq
_ix_apic_sv:
	push $0x41
	incl _iv_int_ac
	call _ix_apic_svh
	jmp _ix_apic_irq

.macro AIRQ num
_ix_airq\num:
	push $0x\num
	jmp _ix_apic_irq
.endm
.macro LIRQ num
_ix_irq\num:
	push $0x\num
	jmp _ix_apic_irq
.endm
AIRQ 20
AIRQ 21
_ix_airq22:
	push $0x22
	incl _iv_int_n
	jmp _ix_apic_irq
AIRQ 23
AIRQ 24
AIRQ 25
AIRQ 26
AIRQ 27
_ix_airq28:
	push $0x28
	incl _iv_int_ac
	jmp _ix_apic_irq
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

_ix_irq00:
	push $0
	incl _iv_int_n
	jmp _ix_apic_irq
LIRQ 01
LIRQ 02
LIRQ 03
LIRQ 04
LIRQ 05
LIRQ 06
LIRQ 07
LIRQ 08
LIRQ 09
LIRQ 0a
LIRQ 0b
LIRQ 0c
LIRQ 0d
LIRQ 0e
LIRQ 0f

_ix_sti:
.global _ix_sti
	sti
	ret
_ix_cli:
.global _ix_cli
	cli
	ret
_ix_irq_eoi:
	cmp $8, %al
	mov $0x20, %al
	jl 1f
	out %al, $PIC2_CMD
1:	out %al, $PIC1_CMD
	ret
_ix_nothing:
.global _ix_nothing
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

_ixa_xcmpxchg:
	.global _ixa_xcmpxchg
	.type _ixa_xcmpxchg,@function
	push %ebp
	mov %esp, %ebp
	push %ebx
	push %ecx
	xor %ecx, %ecx
	mov 0x8(%ebp), %edx	# where to compare
	mov 0xc(%ebp), %eax	# what to compare
	mov 0x10(%ebp), %ebx # stored when we compare
	lock cmpxchg %ebx, (%edx)
	mov 0x14(%ebp), %edx # get result pointer
	mov %eax, (%edx) # save
	jne 1f
	inc %ecx
1:	pop %eax
	xchg %eax, %ecx
	pop %ebx
	pop %ebp
	ret

_ixa_cmpxchg:
	.global _ixa_cmpxchg
	.type _ixa_cmpxchg,@function
	push %ebp
	mov %esp, %ebp
	push %ebx
	push %ecx
	xor %ecx, %ecx
	mov 0x8(%ebp), %edx	# where to compare
	mov 0xc(%ebp), %eax	# what to compare
	mov 0x10(%ebp), %ebx # stored when we compare
	lock cmpxchg %ebx, (%edx)
	jne 1f
	inc %ecx
1:	mov %ecx, %eax
	pop %ecx
	pop %ebx
	pop %ebp
	ret

_ix_eflags:
	.global _ix_eflags
	.type _ix_eflags,@function
	pushf
	pop %eax
	ret

# +0x34 ... <- procedure stack pointer
# 0x30 eflags
# 0x2c cs
# 0x28 eip
# 0x24 pushed irq
# 0x20 zero
# 0x1c EAX
# 0x18 ECX
# 0x14 EDX
# 0x10 EBX
# 0x0c ESP
# 0x08 EBP
# 0x04 ESI
# 0x00 EDI <- context pointer

# reg save
# 0x28 eflags
# 0x24 cs
# 0x20 eip
# 0x1c EAX
# 0x18 ECX
# 0x14 EDX
# 0x10 EBX
# 0x0c ESP
# 0x08 EBP
# 0x04 ESI
# 0x00 EDI <- context pointer

# void _ix_task_switch_from_int(ixintctx *, TaskBlock *save, TaskBlock *to);
_ix_task_switch_from_int:
	.global _ix_task_switch_from_int
	.type _ix_task_switch_from_int,@function
	pop %eax	# drop return address (don't need it)
	pop %ecx	# interrupt context
	pop %esi	# save task
	pop %edi	# destination task
	mov 0x00(%ecx), %eax	# save EDI from int
	mov %eax, 0x00(%esi)
	mov 0x04(%ecx), %eax	# save ESI from int
	mov %eax, 0x04(%esi)
	mov 0x08(%ecx), %eax	# save EBP from int
	mov %eax, 0x08(%esi)
	mov 0x10(%ecx), %eax	# save EBX from int
	mov %eax, 0x10(%esi)
	mov 0x14(%ecx), %eax	# save EDX from int
	mov %eax, 0x14(%esi)
	mov 0x18(%ecx), %eax	# save ECX from int
	mov %eax, 0x18(%esi)
	mov 0x1c(%ecx), %eax	# save EAX from int
	mov %eax, 0x1c(%esi)
	# 20 and 24 are extra codes
	# eip, cs, eflags (for interrupt)
	mov 0x28(%ecx), %eax	# int->EIP
	mov %eax, 0x20(%esi)	# save->r_eip
	mov 0x2c(%ecx), %eax	# int->CS
	mov %eax, 0x24(%esi)	# save->r_cs
	mov 0x30(%ecx), %eax	# int->EFlags
	mov %eax, 0x28(%esi)	# save->r_eflags
	add $0x34, %ecx			# before interrupt context
	# TODO for user space:
	# extra items will exist on the stack
	mov %ecx, 0x0c(%esi)	# save->r_esp
	
	# TODO use kernel stack for user space tasks
	mov 0x0c(%edi), %esp	# to->r_esp
	mov 0x28(%edi), %eax	# to->r_eflags
	push %eax
	mov 0x24(%edi), %eax	# to->r_cs TODO double check this later on
	push %eax
	mov 0x20(%edi), %eax	# to->r_eip
	push %eax
	mov 0x04(%edi), %esi	# save ESI from int
	mov 0x08(%edi), %ebp	# save EBP from int
	mov 0x10(%edi), %ebx	# save EBX from int
	mov 0x14(%edi), %edx	# save EDX from int
	mov 0x18(%edi), %ecx	# save ECX from int
	mov 0x1c(%edi), %eax	# save EAX from int
	mov 0x00(%edi), %edi	# save EDI from int
	iret

_ix_task_switch:
	.global _ix_task_switch
	.type _ix_task_switch,@function
	push %ebp
	mov %esp, %ebp
	mov 0x8(%ebp), %eax
	mov %edi, 0x00(%eax)
	mov %esi, 0x04(%eax)
	mov %ebx, 0x10(%eax)
	mov %edx, 0x14(%eax)
	mov %ecx, 0x18(%eax)
	mov %eax, %ecx
	#mov %eax, 0x1c(%eax) # unused here
	pushf
	pop %eax
	mov %eax, 0x28(%ecx) # save flags
	mov 0xc(%ebp), %edx # get the "to" pointer
	pop %ebp	# saved ebp
	mov %ebp, 0x08(%ecx) # save the last ebp
	pop %ebp	# return address
	mov %ebp, 0x20(%ecx)	# save return to EIP
	movl $0x10, 0x24(%ecx)	# CS save (unused)
	mov %esp, 0x0c(%ecx)	# save the "from" stack
	# all of "from" is saved
	# EDX is the "to" pointer
	cli
	mov 0x00(%edx), %edi
	mov 0x04(%edx), %esi
	mov 0x08(%edx), %ebp
	mov 0x0c(%edx), %esp	# stack of the procedure
	mov 0x10(%edx), %ebx
	mov 0x18(%edx), %ecx
	mov 0x28(%edx), %eax	# eflags
	push %eax
	mov 0x24(%edx), %eax	# to->r_cs TODO double check this later on
	push %eax
	mov 0x20(%edx), %eax	# EIP save
	push %eax
	mov 0x1c(%edx), %eax	# eax save
	mov 0x14(%edx), %edx	# edx save
	iret	# return to the "to" task

_ix_makeidt:
	movl $_iv_idt, %esi	# the IDT to fill
	mov $0x100, %ecx
	# itable descriptors are size 8
	#movl $_iv_itable, %esi
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
	call ix_com_printz
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
_iv_idt:	# the IDT itself
.global _iv_idt
idt_int _ixe_DE, KSEG
idt_int _ixe_DB, KSEG
idt_int _ixe_X2, KSEG
idt_int _ixe_BP, KSEG	# 3
idt_int _ixe_OF, KSEG
idt_int _ixe_BR, KSEG
idt_int _ixe_UD, KSEG
idt_int _ixe_NM, KSEG	# 7
idt_int _ixe_DF, KSEG
idt_int _ixe_X9, KSEG
idt_int _ixe_TS, KSEG
idt_int _ixe_NP, KSEG	# 0x0b
idt_int _ixe_SS, KSEG
idt_int _ixe_GP, KSEG
idt_int _ixe_PF, KSEG
idt_int _ixe_X15, KSEG	# 0x0f
idt_int _ixe_MF, KSEG
idt_int _ixe_AC, KSEG
idt_int _ixe_MC, KSEG
idt_int _ixe_XM, KSEG	# 0x13
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG	# 0x17
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG	# 0x1b
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG	# 0x1f
idt_int _ix_irq00, KSEG	# 0x20
idt_int _ix_irq01, KSEG
idt_int _ix_irq02, KSEG
idt_int _ix_irq03, KSEG	# 0x23
idt_int _ix_irq04, KSEG
idt_int _ix_irq05, KSEG
idt_int _ix_irq06, KSEG
idt_int _ix_irq07, KSEG	# 0x27
idt_int _ix_irq08, KSEG
idt_int _ix_irq09, KSEG
idt_int _ix_irq0a, KSEG
idt_int _ix_irq0b, KSEG	# 0x2b
idt_int _ix_irq0c, KSEG
idt_int _ix_irq0d, KSEG
idt_int _ix_irq0e, KSEG
idt_int _ix_irq0f, KSEG	# 0x2f
idt_int _ix_nothing, KSEG # 0x30
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG # 0x33
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG # 0x37
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG # 0x3b
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG # 0x3f
idt_int _ix_airq20, KSEG # 0x40
idt_int _ix_airq21, KSEG
idt_int _ix_airq22, KSEG
idt_int _ix_airq23, KSEG # 0x43
idt_int _ix_airq24, KSEG
idt_int _ix_airq25, KSEG
idt_int _ix_airq26, KSEG
idt_int _ix_airq27, KSEG # 0x47
idt_int _ix_airq28, KSEG
idt_int _ix_airq29, KSEG
idt_int _ix_airq2a, KSEG
idt_int _ix_airq2b, KSEG # 0x4b
idt_int _ix_airq2c, KSEG
idt_int _ix_airq2d, KSEG
idt_int _ix_airq2e, KSEG
idt_int _ix_airq2f, KSEG # 0x4f
idt_int _ix_airq30, KSEG # 0x50
idt_int _ix_airq31, KSEG
idt_int _ix_airq32, KSEG
idt_int _ix_airq33, KSEG # 0x53
idt_int _ix_airq34, KSEG
idt_int _ix_airq35, KSEG
idt_int _ix_airq36, KSEG
idt_int _ix_airq37, KSEG # 0x57
idt_int _ix_airq38, KSEG
idt_int _ix_airq39, KSEG
idt_int _ix_airq3a, KSEG
idt_int _ix_airq3b, KSEG # 0x5b
idt_int _ix_airq3c, KSEG
idt_int _ix_airq3d, KSEG
idt_int _ix_airq3e, KSEG
idt_int _ix_airq3f, KSEG # 0x5f
.fill (0xf0 - 0x60), 8, 0
idt_int _ix_nothing, KSEG # 0xf0
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG # 0xf3
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG # 0xf7
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG # 0xfb
idt_int _ix_nothing, KSEG
idt_int _ix_nothing, KSEG
idt_int _ix_apic_timer, KSEG
idt_int _ix_apic_sv, KSEG # 0xff

_iv_gtable:
.word 0

