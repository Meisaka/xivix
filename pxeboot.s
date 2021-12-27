/* ***
 * pxeboot.s - PXE graphical bootstrap for Xivix
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

.bss
memmapc: .word 0
pxestix: .fill 142
bootfu:	.fill 290

.text
_start:
.global _start
.code16

	push %es
	push %bx
	jmp testp

.align 8
urgdtptr:
	.word urgdt_end - urgdt - 1
	.int urgdt
.align 8
urgdt:	.int 0
	.int 0
	.int 0x0000ffff
	.int 0x00cf9200
	.int 0x0000ffff
	.int 0x00cf9a00
	.int 0x0000ffff
	.int 0x000f9200
	.int 0x0000ffff
	.int 0x000f9a00
urgdt_end:
	nop

.align 2
pxenv:	.word 0
pxenvs:	.word 0
pxeapi:	.word 0
pxeapis: .word 0
pxepl:	.word 0
pxepls:	.word 0
scnp:	.word 0
scnr:	.word 0
scnc:	.word 0
dlc:	.word 0
real_ss: .word 0
real_sp: .word 0
real_fs: .word 0
real_gs: .word 0
favmode: .word 0xffff
favtype: .word 0
.align 4
	.set ELFMAGIC, (('E' << 8) + ('L' << 16) + ('F' << 24) + (0x7f))
	.set pktsize, 512
	.set kldstart, 0x00100000	# where to load. incremented during loading
kldst:	.int kldstart
kld:	.int kldstart
kentry:	.int undef_kentry
kentryseg:	.word 0x10
scnb:	.word 0xb800	# screen start segment
scnw:	.word 80
scnh:	.word 25
scnattr: .byte 0x17
msghihi: .ascii " >< xivix >< "
msgf:	.asciz "**BOOT FAIL**"
msgok:	.asciz "[OK]"
msgerr:	.asciz "[ERR]"
msgenter: .asciz ".text="
msgold:	.ascii " PXE LEGACY MODE "
msgpxe: .ascii "PXENV+"
msgmorepxe: .ascii "!PXE"
vbe:	.ascii "VBE2"
vesa:	.ascii "VESA"
msga20_off:	.ascii "A20 OFF "
msga20_on:	.ascii "A20 ON  "
bootfile: .asciz "kernel.elf"
msgnext: .asciz " next:"
msgelf: .asciz " loaded ELF image "
msgmem:	.asciz " mem map..."
modemsg: .asciz "preferred video mode: "

testp:	mov $0xa69f, %bx
	call hex16out		# A69F

		# PXENV+ check
	pop %si
	pop %ds
	push %si
	mov $6, %cx
	xor %ax, %ax
	mov %ax, %es
	mov $msgpxe, %di
	call strchk		# make sure we have a PXENV+
	jnz ohwell
	pop %si
	xor %ax, %ax
	mov %ax, %es		# make ES zero
	mov %ax, %cx
	mov 0x6(%si), %bx
	call hex16out		# Version
	mov 0x8(%si), %bx	# Length | Checksum_add << 8
	mov %bl, %cl		# compute checksum from length
	call strsum
	jnz ohwell		# it should be zero
	mov 0xA(%si), %bx
	mov %bx, %es:pxeapi	# save API entry
	mov 0xC(%si), %bx
	mov %bx, %es:pxeapis
	mov 0x28(%si), %bx
	mov %bx, %es:pxepl	# PXEPtr offset
	mov 0x2A(%si), %bx
	mov %bx, %es:pxepls	# PXEPtr segment
	xor %ax, %ax
	mov %ax, %ds
	mov $msgok, %si
	mov $4, %cx
	call strout
		# !PXE check
	mov pxepl, %bx		# load in !PXE address
	mov pxepls, %cx
	mov %cx, %ds
	mov %bx, %si
	xor %ax, %ax
	mov %ax, %es
	mov $msgmorepxe, %di
	mov $4, %cx
	push %si
	call strchk
	pop %si
	jnz oldpxe
	mov 0x4(%si), %bx	# Length | Checksum_add << 8
	mov %bl, %cl		# compute checksum from length
	call strsum
	jnz oldpxe		# it should be zero
	mov 0x10(%si), %bx
	mov %bx, %es:pxeapi	# save API entry
	mov 0x12(%si), %bx
	mov %bx, %es:pxeapis
	jmp nbpmain
oldpxe:
	xor %ax, %ax
	mov %ax, %ds
	mov $msgold, %si
	mov $17, %cx
	call strout
nbpmain:
	xor %ax, %ax	# can boot from PXE
	mov %ax, %ds
	mov $msghihi, %si
	mov $13, %cx
	call strout
	call tella20	# A20 tests and enable
	jnz a20good
	mov $0x2401, %ax	# ask bios to do it
	int $0x15
	call tella20
	jnz a20good
	cli		# fallback method
	call ps2waitw
	mov $0xAD, %al	# disable port 1
	out %al, $0x64

	call ps2waitw
	mov $0xD0, %al	# read ctrl out port
	out %al, $0x64

	call ps2waitr
	in $0x60, %al	# get byte
	mov %ax, %bx	# save it!

	call ps2waitw
	mov $0xD1, %al	# write ctrl out port
	out %al, $0x64

	call ps2waitw
	mov %bx, %ax	# get saved byte
	or $2, %al	# set A20 gate
	out %al, $0x60	# write it

	call ps2waitw
	mov $0xAE, %al	# re-enable port 1
	out %al, $0x64
	call ps2waitw	# wait for it
	sti
	call tella20	# what is it now?
a20good:
	jmp gounreal
ohwell:
	xor %ax, %ax
	mov %ax, %ds
	mov $msgf, %si
	mov $13, %cx
	call strout
hang:
	hlt
	jmp hang

pxecall:
	push %es
	push %di
	push %bx
	lcall *pxeapi
	add $6, %sp
	ret
ps2waitr:
	in $0x64, %al
	test $1, %al
	jz ps2waitr
	ret
ps2waitw:
	in $0x64, %al
	test $2, %al
	jnz ps2waitw
	ret
tella20:
	call chka20
	pushf
	jz 1f		# A20 is on if ax == 1
	mov $msga20_on, %si
	mov $8, %cx
	call strout
	jmp 2f
1:	mov $msga20_off, %si
	mov $8, %cx
	call strout
2:	popf
	ret
chka20:
	pushf
	push %ds
	push %es
	push %si
	push %di
	cli
	xor %ax, %ax
	mov %ax, %es	# ES = 0x0000
	not %ax
	mov %ax, %ds	# DS = 0xFFFF
	mov $testp+0x01, %di	# offset
	mov $testp+0x11, %si	# offset + 1MiB
	mov %es:0(%di), %ax
	push %ax
	not %ax
	mov %ax, %es:0(%di)	# invert one side
	cmp %ax, %ds:0(%si)	# other still the same?
	pop %ax
	mov %ax, %es:0(%di)
	mov $0, %ax
	je 1f
	mov $1, %ax	# if they are different A20 is on, set ax = 1
1:	pop %di
	pop %si
	pop %es
	pop %ds
	popf
	and %ax, %ax	# update Z flag from ax
	ret
strout:
	jcxz 2f
	push %di
	push %si
	push %es
	push %ds
	xor %ax, %ax
	mov %ax, %ds
	movw scnp, %di
	movw scnb, %es
	mov $0x9c, %ah
	pop %ds
1:	lodsb
	call scnputc
	loop 1b
	push %ds
	xor %ax, %ax
	mov %ax, %ds
	mov %di, scnp
	pop %ds
	pop %es
	pop %si
	pop %di
2:	ret
chout:
	push %di
	push %es
	xor %ax, %ax
	mov %ax, %es
	mov %es:scnp, %di
	mov %es:scnb, %ax
	mov %ax, %es
	mov %bl, %al
	mov $0x14, %ah
	call scnputc
	xor %ax, %ax
	mov %ax, %es
	mov %di, %es:scnp
	pop %es
	pop %di
	ret
strzout:
	push %di
	push %si
	push %es
	xor %ax, %ax
	mov %ax, %es
	movw %es:scnp, %di
	movw %es:scnb, %ax
	mov %ax, %es
	mov $0x17, %ah
1:	lodsb
	and %al, %al
	jz 1f
	call scnputc
	jmp 1b
1:	xor %ax, %ax
	mov %ax, %es
	mov %di, %es:scnp
	pop %es
	pop %si
	pop %di
	ret
scnputc:
	stosw
	push %bx
	mov scnc, %bx	# increment current column
	inc %bx
	cmp scnw, %bx	# past width?
	jl 2f
	mov scnr, %bx	# increment row
	inc %bx
	cmp scnh, %bx	# past height?
	jl 1f
	xor %bx, %bx	# reset row
1:	mov %bx, scnr
	xor %bx, %bx
2:	mov %bx, scnc	# save column
	pop %bx
	ret
scnnl:
	push %ds
	push %si
	xor %si, %si
	mov %si, %ds
	mov %si, scnc
	mov scnr, %si
	inc %si
	cmp scnh, %si
	jl 1f
	xor %si, %si
1:	mov %si, scnr
	shl $5, %si
	mov %si, scnp
	shl $2, %si
	add scnp, %si
	mov %si, scnp
	pop %si
	pop %ds
	ret
hex16out:  /* output bx as hex 16bits*/
	push %ds
	push %di
	push %es
	xor %ax, %ax
	mov %ax, %ds
	movw scnp, %di
	movw scnb, %es
	mov $0x17, %ah
	mov %bh, %al
	shr $4, %al
	call hexnybout
	mov %bh, %al
	and $0xf, %al
	call hexnybout
	mov %bl, %al
	shr $4, %al
	call hexnybout
	mov %bl, %al
	and $0xf, %al
	call hexnybout
	mov %di, %ax
	cmp $0xFB3, %ax
	jl 1f
	xor %ax, %ax
1:	mov %ax, scnp
	pop %es
	pop %di
	pop %ds
	ret
hexnybout:
	cmp $10, %al
	jl 1f
	add $7, %al
1:	add $'0', %al
	call scnputc
	ret

strchk:
	jcxz 2f
	repe cmpsb
	mov %cx, %ax
	and %ax, %ax # flags update
	ret
2:	xor %ax, %ax
	ret

strsum:
	jcxz 2f
	push %si
	xor %ax, %ax
1:	lodsb
	add %ah, %al
	mov %al, %ah
	loop 1b
	pop %si
	and %al, %al
	ret
2:	or 0xff, %al
	ret
strzcopy:
	push %si
	push %di
1:	lodsb
	stosb
	and %ax, %ax
	jz 2f
	jmp 1b
2:	pop %di
	pop %si
	ret
gounreal:
	xor %ax, %ax
	mov %ax, %ds
	cli
	push %ds
	push %es
	lgdt urgdtptr	# load a p-mode GDT
	mov %cr0, %eax
	or $1, %al
	mov %eax, %cr0	# enter prot
	jmp 1f	# just Intel things :3
1:	mov $0x08, %bx	# select entry 1
	mov %bx, %ds	# use for DS and ES, caching the new limit
	mov %bx, %es
	xor $1, %al	# clear the p-mode bit
	mov %eax, %cr0	# go back to (un)real mode
	pop %es
	pop %ds
	mov $0x100100, %esi	# try write above 1MiB
	mov $0x1F01, %ax	# a character to copy between memory and screen
	mov %ax, (%esi)
	mov $0x0B8000, %edi
	add $4, %edi
	mov (%esi), %ax
	mov %ax, (%edi)
	sti
	call showmemmap
		# now to download!
	mov $0x0241, %bx	# Status 'A'
	call rmout
	xor %ax, %ax
	mov %ax, %es
	mov %ax, %ds
	mov $pxestix, %di
	mov %ax, %es:0(%di)
	mov %ax, %es:10(%di)
	mov %ax, %es:4(%di)
	mov %ax, %es:6(%di)
	mov %ax, %es:8(%di)
	mov $2, %ax		# which to get
	mov %ax, %es:2(%di)
	mov $0x0071, %bx	# get cached info
	call pxecall
	mov pxestix, %bx
	call hex16out
	mov %bx, %ax
	and %ax, %ax
	jnz ohwell
	mov $msgnext, %si
	call strzout
	mov $pxestix, %di
	mov %es:8(%di), %bx
	mov %bx, %ds
	mov %es:6(%di), %bx
	mov %bx, %si
	mov %ds:20(%si), %bx
	call hex16out
	mov %ds:22(%si), %bx
	call hex16out
	xor %ax, %ax
	mov %ax, %es
	mov $pxestix, %di	# wipe the struct
	mov $71, %cx
	rep stosw
	mov $pxestix, %di
	mov %ds:20(%si), %bx	# copy next-server
	mov %bx, %es:2(%di)
	mov %ds:22(%si), %bx
	mov %bx, %es:4(%di)
	mov $0x4500, %bx	# set port (BE) and packet size
	mov %bx, %es:138(%di)
	mov $pktsize, %bx
	mov %bx, %es:140(%di)
	mov %ax, %ds
	mov $pxestix+10, %di	# set filename
	mov $bootfile, %si
	call strzcopy
	mov $0x0200+'O', %bx	# Status 'O'
	call rmout
	mov $pxestix, %di	# open file
	mov $0x0020, %bx
	call pxecall
	and %ax, %ax
	jz pxedl
pxefail:
	mov $0x0200+'F', %bx	# Status 'F'
	call rmout
	jmp ohwell
pxedl:
	mov $0x0200+'d', %bx	# Status 'D'
	call rmout
	xor %ax, %ax
	mov $pxestix, %di
	mov %ax, %ds
	mov %ax, %es
	mov %ax, (%di)
	mov %ax, 2(%di)
	mov %ax, 8(%di)	# buffer is in segment zero
	mov $0x1000, %ax	# set buffer offset
	mov %ax, 6(%di)
1:	mov $0x0022, %bx	# read pkt loop
	mov $pxestix, %di	# reset this before we call
	call pxecall
	and %ax, %ax
	jnz pxefail
	mov $0x0200+'D', %bx	# Status 'D'
	call rmout
	mov dlc, %ax
	inc %ax
	mov %ax, dlc
	cmp $128, %ax		# status markers
	jl 2f
	mov $'.', %bl
	call chout
	xor %ax, %ax
	mov %ax, dlc
2:	mov pxestix+4, %cx	# get packet size
	jcxz 4f			# skip ahead if it's zero
	mov kld, %edi		# copy to ext ram
	mov $0x1000, %si	# read from the PXE buffer
3:	lodsb			# copy loop (bytewise)
	stosb %al, (%edi)	# unreal store!
	loop 3b
	mov %edi, kld		# save the write address
	mov pxestix+4, %ax	# check PXE download done
	cmp $pktsize, %ax
	je 1b
4:	mov $pxestix, %di	# close file
	mov $0x0021, %bx
	call pxecall
	mov $0x0200+'K', %bx	# Status 'K'
	call rmout
	mov $msgok, %si
	call strzout
	mov kld+2, %bx
	call hex16out
	mov kld, %bx
	call hex16out
	xor %ax, %ax
	mov %ax, %ds
	mov %ax, %es
	call scnnl

	xor %ax, %ax	# "validate" kernel
	mov %ax, %ds	# data segment zero
	mov $256, %cx	# search limit
	mov kldst, %esi	# start looking here
	mov %ds:(%esi), %eax	# load first word
	cmp $ELFMAGIC, %eax	# is this the ELF magic number?
	je doelfkernel
1:	mov %ds:(%esi), %eax	# load a word
	cmp $0xeca70433, %eax	# is this the magic number?
	je doeckernel	# if yes then found it
	add $8, %esi	# advance pointer
	loop 1b		# looked too far yet?
E_kern_no_magic: call errfail	# fail!
doelfkernel:
	mov $msgelf, %si
	call strzout
	push %ax
	call enterprot
.code32
	push %ebp
	mov kldst, %ebp	# start looking here
	xor %eax, %eax
	mov %eax, %ecx
	mov 4(%ebp), %eax
	and $0xffff, %eax
	xor $0x00000101, %eax
	or %eax, %ecx
	mov 16(%ebp), %eax
	xor $0x00030002, %eax
	or %eax, %ecx
	jz 1f
	pop %ebp
	push E_elf_wrong_platform
	jmp leaveprot
1:	mov kld, %eax
	mov $0xfffff000, %edx
	and %edx, %eax
	sub %edx, %eax
	mov %eax, kld
	mov 24(%ebp), %ebx	# entry addr
	#call hex32outp
	mov 28(%ebp), %esi	# program header table
	mov %esi, %ebx
	#call hex32outp
	mov 42(%ebp), %ebx
	mov %ebx, %edi
	#call hex32outp
	call nlout32
	shr $16, %edi	# get prog header entry count
1:	mov (%ebp, %esi, 1), %eax
	cmp $1, %eax	# must be a load segment
	jne 2f
	mov 12(%ebp, %esi, 1), %eax	# get phyaddr
	add 20(%ebp, %esi, 1), %eax	# add memsize
	and %edx, %eax	# round up to page
	sub %edx, %eax
	cmp kld, %eax	# is this higher?
	jle 2f
	mov %eax, kld # save it
2:	movzxw 42(%ebp), %eax
	add %eax, %esi
	dec %edi	# loop over entries
	jnz 1b
	mov kld, %ebx	# show the load address
	#call hex32outp
	mov %ebp, %esi
	mov kld, %edi
	mov %edi, %ebp
	mov %edi, %ecx
	sub %esi, %ecx
	call memcopy32
	mov 28(%ebp), %esi	# program header table
	mov 42(%ebp), %edx
	shr $16, %edx	# get prog header entry count
1:	mov (%ebp, %esi, 1), %eax
	cmp $1, %eax	# must be a load segment
	jne 2f
	mov 12(%ebp, %esi, 1), %edi	# get phyaddr
	mov 20(%ebp, %esi, 1), %ecx	# get memsize
	shr $2, %ecx
	xor %eax, %eax	# clear memory first
	rep stosl
	mov 12(%ebp, %esi, 1), %edi	# get phyaddr
	mov 16(%ebp, %esi, 1), %ecx	# get segment file size
	push %esi			# save entry base
	mov 4(%ebp, %esi, 1), %esi	# segment file offset
	add %ebp, %esi			# offset by start of file
	call memcopy32		# copy segment to it's home
	pop %esi		# restore entry base
2:	movzxw 42(%ebp), %eax	# get entry size and advance by it
	add %eax, %esi
	dec %edx	# more entries?
	jnz 1b
	pop %ebp	# restore ebp
	mov kldst, %esi	# XXX debug test to check start of kernel
	mov (%esi), %ebx
	cmp $0xeca70433, %ebx	# is this the magic number?
	je 1f
	push E_elf_notakernel
	jmp leaveprot
1:	add $4, %esi
	mov %esi, kentry
	#mov kldst, %esi	# XXX debug test to check start of kernel
4:	mov %esi, %ebx
	#call hex32outp
	mov 0(%esi), %ebx
	#call hex32outp
	call leaveprot
.code16
	#call dovbe	# select and activate framebuffer
	jmp dokernel
E_elf_load_todo: call errfail	# TODO
E_elf_wrong_platform: call errfail
E_elf_notakernel: call errfail
doeckernel:
	add $4, %esi
	mov %esi, %ds:kentry	# ESI = start of kernel code after magic
	jmp dokernel
errfail:
	call scnnl
	mov $msgerr, %si
	call strzout
	pop %bx
	sub $3, %bx
	call hex16out
	jmp ohwell
showmemmap:
	mov $msgmem, %si
	call strzout
	call domemmap		# make memory map
	mov $msgok, %si
	jnc 1f
	mov $msgerr, %si
1:	mov memmapc, %cx
	call scnnl
	mov $0x800, %si
1:	mov %ds:6(%si), %bx
	call hex16out
	mov %ds:4(%si), %bx
	call hex16out
	mov %ds:2(%si), %bx
	call hex16out
	mov %ds:0(%si), %bx
	call hex16out
	mov $' ', %bl
	call chout
	mov %ds:14(%si), %bx
	call hex16out
	mov %ds:12(%si), %bx
	call hex16out
	mov %ds:10(%si), %bx
	call hex16out
	mov %ds:8(%si), %bx
	call hex16out
	mov $' ', %bl
	call chout
	mov %ds:18(%si), %bx
	call hex16out
	mov %ds:16(%si), %bx
	call hex16out
	mov $' ', %bl
	call chout
	mov %ds:22(%si), %bx
	call hex16out
	mov %ds:20(%si), %bx
	call hex16out
	call scnnl
	add $24, %si
	loop 1b
	ret
dokernel:
	mov $msgenter, %si
	call strzout
	xor %ecx, %ecx
	mov %cx, %ds
	mov kentry+2, %bx	# show entry address
	call hex16out
	mov kentry, %bx
	call hex16out
	call dovbe
	call enterprot
.code32
	ljmpl *kentry
undef_kentry:
	call leaveprot
.code16
E_undef_kentry: call errfail	# no entry point set
enterprot:
	cli
	mov $0x0200+'P', %bx
	call rmout
	mov %ss, %bx
	mov %bx, real_ss
	#call hex16out
	#mov %es, %bx
	#call hex16out
	#mov %ds, %bx
	#call hex16out
	mov %fs, %bx
	mov %bx, real_fs
	#call hex16out
	mov %gs, %bx
	mov %bx, real_gs
	#call hex16out
	pop %bx
	push %bx
	call hex16out
	pop %bx
	mov %sp, real_sp
	lgdt urgdtptr
	mov %cr0, %eax
	or $1, %al
	mov %eax, %cr0	# enter protected mode
	ljmpl $0x10, $enprot	# switch to 32 bit code segment
leaveprot16:
	mov %cr0, %eax
	and $0xfe, %al
	mov %eax, %cr0	# leave protected mode
	jmp $0, $1f
1:	xor %ax, %ax
	mov real_ss, %ss
	mov %ax, %ds
	mov %ax, %es
	mov real_gs, %gs
	mov real_fs, %fs
	mov $0x0400+'L', %bx
	call rmout
	#mov %ss, %bx
	#call hex16out
	#mov %sp, %bx
	#call hex16out
	#pop %bx
	#push %bx
	#mov %bx, %si
	#call hex16out
	#mov (%si), %bx
	#call hex16out
	sti
#1:	hlt
#	jmp 1b
	ret
.code32
leaveprot:
	cli
	movzxw real_ss, %eax
	shl $4, %eax
	sub %eax, %esp
	xor %ebx, %ebx
	#mov %esp, %ebx
	#call hex32out
	xor %eax, %eax
	#mov $0x18, %al	# to resume real-mode limits (not unreal)
	#mov %ax, %ss
	#mov %ax, %ds
	#mov %ax, %es
	#mov %ax, %fs
	#mov %ax, %gs
	jmpl $0x20,$leaveprot16
enprot:
	xor %eax, %eax
	mov $0x08, %al
	mov %eax, %ss
	mov %eax, %ds
	mov %eax, %es
	mov %eax, %fs
	mov %eax, %gs
	movzxw real_ss, %eax
	shl $4, %eax
	xchg %esp, %eax
	movzxw real_sp, %eax
	add %eax, %esp
	#push %ebx
	#mov %esp, %ebx
	#call hex32out
	#pop %ebx
	jmp *%ebx
hex32out: /* output EBX as hex (32 bit mode) */
	push %ecx
	push $8
	pop %ecx
2:	rol $4, %ebx
	mov %bl, %al
	and $0xf, %al
	add $'0', %al
	cmp $58, %al
	jl 1f
	add $7, %al	
1:	call cout32
	loop 2b
	pop %ecx
	ret
hex32outp:	# like hex32out, but with a space at the end
	call hex32out
	mov $'.', %al
cout32:
	push %edi
	movzxw scnp, %edi
	mov scnattr, %ah
	movw %ax, 0xb8000(%edi)
	inc %edi
	inc %edi
	cmp $0xfa0, %edi
	jl 1f
	xor %edi, %edi
1:	mov %di, scnp
	movzxw scnc, %edi
	inc %edi
	cmp scnw, %edi
	jl 1f
	xor %edi, %edi
1:	mov %di, scnc
	pop %edi
	ret
nlout32:
	push %edi
	movzxw scnp, %edi
	movzxw scnc, %eax
	add %eax, %eax
	sub %eax, %edi
	movzxw scnw, %eax
	add %eax, %eax
	add %eax, %edi
	cmp $0xfa0, %edi
	jl 1f
	xor %edi, %edi
1:	mov %di, scnp
	movzxw scnr, %eax
	inc %eax
	cmp scnh, %eax
	jl 1f
	xor %eax, %eax
1:	mov %ax, scnr
	xor %eax, %eax
	mov %ax, scnc
	pop %edi
	ret
memcopy32:
	cld
	shr $2, %ecx
1:	lodsl
	stosl
	loop 1b
	ret
.code16
dovbe:
	xor %ax, %ax
	mov %ax, %es
	mov %ax, %ds
	mov $0x1000, %di
	mov vbe, %ax
	mov %ax, (%di)
	mov vbe+2, %ax
	mov %ax, 2(%di)
	mov $0x4F00, %ax
	int $0x10
	cmp $0x004F, %ax	# get info blk ok?
	je 1f
E_vbe_blocknotok: call errfail
1:	call scnnl
#E_vbe: call errfail
	mov %di, %si
	xor %ax, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %es:0(%di), %ax
	cmp %ax, vesa
	je 1f
	ret
1:	mov %es:2(%di), %ax
	cmp %ax, vesa+2
	je 1f
	ret
1:	mov $4, %cx
	call strout
	call scnnl
	mov $4, %cx
	mov %es:4(%di), %bx
	call hex16out
	call scnnl
	mov %es:6(%di), %bx
	mov %bx, %si
	mov %es:8(%di), %bx
	push %ds
	mov %bx, %ds
	call strzout
	pop %ds
	call scnnl
	mov %es:10(%di), %bx
	call hex16out
	mov $' ', %bl
	call chout
	mov %es:12(%di), %bx
	call hex16out
	mov $' ', %bl
	call chout

	mov $4, %cx
	add $14, %di
1:	mov %es:0(%di), %bx
	call hex16out
	mov $'-', %bl
	call chout
	add $2, %di
	loop 1b
	call scnnl
	mov $3, %cx
1:	mov %es:0(%di), %bx
	call hex16out
	mov %bx, %si
	mov %es:2(%di), %bx
	call hex16out
	mov %bx, %ds
	call strzout
	call scnnl
	add $4, %di
	loop 1b
	xor %ax, %ax
	mov %ax, %ds
	mov %ax, %es
	mov $0x1000, %si	# info block
	mov $0x1200, %di	# mode info
	mov 14(%si), %ax
	mov 16(%si), %bx
	mov %ax, %si
	mov %bx, %ds
modeloop:
	mov 0(%si), %cx		# display modes
	cmp $0xffff, %cx
	je modelexit
	mov $0x4F01, %ax
	int $0x10
	cmp $0x004F, %ax	# get mode info
	jne modelexit
	mov %es:0(%di), %bx	# color mode and min res check
	test $0x10, %bx
	jz modeskip
	mov %es:18(%di), %bx	# Xres
	cmp $1024, %bx
	jl modeskip
	mov %es:20(%di), %bx	# Yres
	cmp $768, %bx
	jl modeskip
	mov %es:0(%di), %bx
	call hex16out
	mov $' ', %bl
	call chout
	mov %es:18(%di), %bx	# Xres
	call hex16out
	mov $' ', %bl
	call chout
	mov %es:20(%di), %bx	# Yres
	call hex16out
	mov $' ', %bl
	call chout
	mov %es:22(%di), %bx	# charsize Y:X
	call hex16out
	mov $' ', %bl
	call chout
	mov %es:24(%di), %bx	# bpp:planes
	call hex16out
	mov $' ', %bl
	call chout
	mov %es:26(%di), %bx	# model:banks
	call hex16out
	mov $' ', %bl
	call chout
	mov %es:28(%di), %bx	# bank sz:n-images
	call hex16out
	mov $' ', %bl
	call chout
	mov %es:42(%di), %bx
	call hex16out
	mov %es:40(%di), %bx	# 40:42 phys base flat
	call hex16out
	mov $' ', %bl
	call chout
	mov %es:50(%di), %bx
	call hex16out

	# preferred mode selection
	mov %es:18(%di), %bx	# Xres
	cmp $1024, %bx
	je vmode1024
	cmp $1280, %bx
	je vmode1280
	cmp $1440, %bx	# mode width
	jne modeend
	mov %es:20(%di), %bx	# Yres
	cmp $900, %bx
	jne modeend
	mov %es:24(%di), %bx	# bpp:planes
	mov $0x22, %ax
	cmp $0x2001, %bx	# @32 (most preferred)
	je vbepickmode
	mov $0x21, %ax
	cmp $0x1001, %bx	# @16	(favor of size over depth)
	je vbepickmode
	jmp modeend
vmode1024:
	mov %es:20(%di), %bx	# Yres
	cmp $768, %bx
	jne modeend	# make sure we want this one
	mov %es:24(%di), %bx	# bpp:planes
	mov $0x02, %ax
	cmp $0x2001, %bx	# 1024x768 @32
	je vbepickmode
	mov $0x01, %ax
	cmp $0x1001, %bx	# 1024x768 @16 (minimum)
	je vbepickmode
	jmp modeend
vmode1280:
	mov %es:20(%di), %bx	# Yres
	cmp $1024, %bx
	jne modeend
	mov %es:24(%di), %bx	# bpp:planes
	mov $0x12, %ax
	cmp $0x2001, %bx	# 1280x1024 @32
	je vbepickmode
	mov $0x11, %ax
	cmp $0x1001, %bx	# 1280x1024 @16
	je vbepickmode
	#jmp modeend	# at end already
modeend:
	mov $'%', %bl
	call chout
	mov %cx, %bx
	call hex16out
	mov $'%', %bl
	call chout
	call scnnl
modeskip:
	add $2, %si
	jmp modeloop
modelexit:
	xor %ax, %ax
	mov %ax, %ds
	mov $modemsg, %si
	call strzout
	mov favmode, %bx
	call hex16out
	mov favtype, %bx
	call hex16out
	mov favmode, %cx
	cmp $0xffff, %cx
	jne vbesetmode
	xor %ax, %ax
	mov %ax, %ds
	mov %ax, %es
	ret
vbepickmode:
	cmp favtype, %ax
	jl modeend
	mov %cx, %es:favmode
	mov %ax, %es:favtype
	jmp modeend

vbesetmode:
	mov $0x4F01, %ax	# get info about it
	int $0x10
	mov %cx, %bx
	#and $0x1ff, %bx
	or $0x4000, %bx
	mov $0x4F02, %ax	# set it
	int $0x10
	xor %ax, %ax
	mov %ax, %ds
	mov %ax, %es
	ret
domemmap:
	xor %ebx, %ebx
	mov %bx, %bp
	mov %bx, %es
	mov %ebx, %ecx
	mov $0x800, %di
	mov $0x534D4150, %edx
	movl $1, %es:20(%di)
	mov $0xE820, %eax
	mov $24, %ecx
	int $0x15
	jc 6f
	mov $0x534D4150, %edx
	cmp %eax, %edx
	jne 6f
	test %ebx, %ebx
	je 6f
	jmp 2f
1:	movl $1, %es:20(%di)
	mov $0xE820, %eax
	mov $24, %ecx
	int $0x15
	jc 5f
	mov $0x534D4150, %edx
2:	jcxz 4f
	cmp $20, %cl
	jbe 3f
	testb $1, %es:20(%di)
	je 4f
3:	mov %es:8(%di), %ecx
	or %es:12(%di), %ecx
	jz 4f
	inc %bp
	add $24, %di
4:	test %ebx, %ebx
	jne 1b
5:	mov %bp, memmapc
	clc
	ret
6:	stc
	ret
rmout:
	push %si
	mov $0x00B8002, %esi
	movw %bx, %ds:0(%esi)
	pop %si
	ret
