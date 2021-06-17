/* ***
 * pxeboot.s - PXE graphical bootstrap for Xivix
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

.bss
memmapc: .word 0
kev:	.int 0
kevseg:	.word 0
gdtnew: .word 0
	.int 0
pxestix:
	.fill 142
bootfu:	.fill 290

.text
_start:
.global _start
.code16

	push %es
	push %bx

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
	xor %ax, %ax		# can boot from PXE
	mov %ax, %ds
	mov $msgb, %si
	mov $6, %cx
	call strout
	call tella20		# A20 tests and enable
	call chka20
	and %ax, %ax
	jnz a20good
	mov $0x2401, %ax
	int $0x15
	call tella20
	call chka20
	and %ax, %ax
	jnz a20good
	cli
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
favmode: .word 0xffff
favtype: .word 0
	.set pktsize, 512
msgb:	.ascii " xivix"
	.set kldstart, 0x00100000	# where to load. incremented during loading
kld:	.int kldstart
scnb:	.word 0xb800	# screen start segment
scnw:	.word 80
scnh:	.word 25
msgf:	.asciz "**BOOT FAIL**"
msgok:	.asciz "[OK]"
msgerr:	.asciz "[ERR]"
msgold:	.ascii " PXE LEGACY MODE "
msgpxe: .ascii "PXENV+"
msgmorepxe: .ascii "!PXE"
msga1:	.ascii "A20 OFF "
msga2:	.ascii "A20 ON  "
bootfile: .asciz "kernel.img"
msgmem:	.asciz " getting mem map.."
modemsg: .asciz "preferred video mode: "

	nop
	nop
	nop
	nop
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
	and %ax, %ax
	jz 1f
	mov $msga2, %si
	mov $8, %cx
	call strout
	jmp 2f
	1:
	mov $msga1, %si
	mov $8, %cx
	call strout
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
	mov $1, %ax
	1:
	pop %di
	pop %si
	pop %es
	pop %ds
	popf
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
	1:
	lodsb
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
	2:
	ret
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
	1:
	lodsb
	and %al, %al
	jz 1f
	call scnputc
	jmp 1b
	1:
	xor %ax, %ax
	mov %ax, %es
	mov %di, %es:scnp
	pop %es
	pop %si
	pop %di
	ret
scnputc:
	stosw
	push %bx
	mov scnc, %bx
	inc %bx
	cmp scnw, %bx
	jl 2f
	mov scnr, %bx
	inc %bx
	cmp scnh, %bx
	jl 1f
	xor %bx, %bx
1:	mov %bx, scnr
	xor %bx, %bx
2:	mov %bx, scnc
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
	1:
	add $'0', %al
	call scnputc
	ret

strchk:
	jcxz 2f
	1:
	repe cmpsb
	mov %cx, %ax
	and %ax, %ax # flags update
	ret
	2:
	xor %ax, %ax
	ret

strsum:
	jcxz 2f
	push %si
	xor %ax, %ax
	1:
	lodsb
	add %ah, %al
	mov %al, %ah
	loop 1b
	pop %si
	and %al, %al
	ret
	2:
	or 0xff, %al
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
urgdtptr:
	.word urgdt_end - urgdt - 1
	.int urgdt
urgdt:	.int 0
	.int 0
	.word 0xffff
	.word 0
	.byte 0
	.byte 0x92
	.byte 0xCF
	.byte 0
	.word 0xffff
	.word 0
	.byte 0
	.byte 0x9A
	.byte 0xCF
	.byte 0
urgdt_end:
	nop
gounreal:
	xor %ax, %ax
	mov %ax, %ds
	cli
	push %ds
	push %es
	lgdt urgdtptr
	mov %cr0, %eax
	or $1, %al
	mov %eax, %cr0
	jmp 1f
	1:
	mov $0x08, %bx
	mov %bx, %ds
	mov %bx, %es
	xor $1, %al
	mov %eax, %cr0
	pop %es
	pop %ds
	mov $0x100100, %esi	# try write above 1MiB
	add $2, %esi
	mov $0x1F01, %ax
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
	mov %es:pxestix+8, %bx
	mov %bx, %ds
	mov %es:pxestix+6, %bx
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
	mov %ds:20(%si), %bx	# copy next-server
	mov %bx, %es:pxestix+2
	mov %ds:22(%si), %bx
	mov %bx, %es:pxestix+4
	mov $0x4500, %bx	# set port (BE) and packet size
	mov %bx, %es:pxestix+138
	mov $pktsize, %bx
	mov %bx, %es:pxestix+140
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
	jz 1f
pxefail:
	mov $0x0200+'F', %bx	# Status 'F'
	call rmout
	jmp ohwell
1:	mov $0x0200+'d', %bx	# Status 'D'
	call rmout
	xor %ax, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, pxestix
	mov %ax, pxestix+2
	mov %ax, pxestix+8
	mov $0x1000, %ax
	mov %ax, pxestix+6
1:	mov $0x0022, %bx	# read pkt
	mov $pxestix, %di
	call pxecall
	and %ax, %ax
	jnz pxefail
	mov $0x0200+'D', %bx	# Status 'D'
	call rmout
	mov dlc, %ax
	inc %ax
	mov %ax, dlc
	cmp $256, %ax		# status markers
	jl 2f
	mov $'.', %bl
	call chout
	xor %ax, %ax
	mov %ax, dlc
2:	mov pxestix+4, %cx
	jcxz 4f
	mov kld, %edi		# copy to ext ram
	mov $0x1000, %si
3:	lodsb
	stosb %al, (%edi)
	loop 3b
	mov %edi, kld
	mov pxestix+4, %ax
	cmp $pktsize, %ax
	je 1b
4:
	mov $pxestix, %di	# close file
	mov $0x0021, %bx
	call pxecall
	mov $0x0200+'K', %bx	# Status 'K'
	call rmout
	mov kld+2, %bx
	call hex16out
	mov kld, %bx
	call hex16out
	xor %ax, %ax
	mov %ax, %ds
	mov %ax, %es
	call strzout

	xor %ax, %ax	# "validate" kernel
	mov %ax, %ds
	mov $0xeca70433, %ebx
	mov $256, %cx
	mov $kldstart, %esi
1:	mov %ds:(%esi), %eax
	add $4, %esi
	cmp %ebx, %eax
	je dokernel
	loop 1b
	jmp ohwell
showmemmap:
	mov $msgmem, %si
	call strzout
	call domemmap		# make memory map
	mov $msgok, %si
	jnc 1f
	mov $msgerr, %si
1:
	mov memmapc, %cx
	call scnnl
	mov $0x800, %si
1:
	mov %ds:6(%si), %bx
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

ixentry:
.int ixprot
.word 0x10
dokernel:
	xor %ecx, %ecx
	mov %cx, %ds
	mov %esi, %ds:kev
	mov $0x10, %ax
	mov %ax, %ds:kevseg
1:	dec %ecx
	cmp %ecx, %ecx
	jne 1b
	mov kev+2, %bx
	call hex16out
	mov kev, %bx
	call hex16out
	mov $msgok, %si
	call strzout
	call dovbe
	cli
	lgdt urgdtptr
	mov %cr0, %eax
	or $1, %al
	mov %eax, %cr0
	ljmpl *ixentry
.code32
ixprot:
	mov $0x08, %bx
	mov %bx, %ss
	mov %bx, %ds
	mov %bx, %es
	mov %bx, %gs
	mov %bx, %fs
	ljmpl *kev
1:	hlt
	jmp 1b
.code16
vbe:	.ascii "VBE2"
vesa:	.ascii "VESA"
dovbe:
	xor %ax, %ax
	mov %ax, %es
	mov $0x1000, %di
	mov vbe, %ax
	mov %ax, (%di)
	mov vbe+2, %ax
	mov %ax, 2(%di)
	mov $0x4F00, %ax
	int $0x10
	cmp $0x004F, %ax	# get info blk ok?
	je 1f
	ret
1:
	call scnnl
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
1:
	mov $4, %cx
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
1:
	mov %es:0(%di), %bx
	call hex16out
	mov $'-', %bl
	call chout
	add $2, %di
	loop 1b
	call scnnl
	mov $3, %cx
1:
	mov %es:0(%di), %bx
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
