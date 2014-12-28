
.bss
gdtsave: .word 0
	.int 0
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
	call dota20
	and %ax, %ax
	jnz 1f
	mov $0x2401, %ax
	int $0x15
	1:
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
dlc:	.word 0
msgb:	.ascii "meivix"
kld:	.int 0x100000	# where to load. incremented during loading
scnb:	.word 0xb800	# screen start segment
msgf:	.ascii "**BOOT FAIL**"
msgok:	.ascii "[OK]"
msgold:	.ascii " PXE LEGACY MODE "
msgpxe: .ascii "PXENV+"
msgmorepxe: .ascii "!PXE"
msga1:	.ascii "A20 OFF "
msga2:	.ascii "A20 ON  "
bootfile: .asciz "kernel.img"

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
tella20:
	call dota20
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
dota20:
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
	stosw
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
	stosw
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
	mov $0x9c, %ah
	1:
	lodsb
	and %al, %al
	jz 1f
	stosw
	jmp 1b
	1:
	xor %ax, %ax
	mov %ax, %es
	mov %di, %es:scnp
	pop %es
	pop %si
	pop %di
	ret
hex16out:  /* output bx as hex 16bits*/
	push %ds
	push %di
	push %es
	xor %ax, %ax
	mov %ax, %ds
	movw scnp, %di
	movw scnb, %es
	mov $0x9c, %ah
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
	mov $0x9C20, %ax
	stosw
	mov %di, scnp
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
	stosw
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
	sti
	mov $0x9E02, %bx
	mov $0x00B8000, %esi
	movw %bx, %ds:0(%esi)
	mov $0x00100000, %esi
	mov $0x9F02, %bx
	movw %bx, %ds:0(%esi)
	movw %ds:0(%esi), %cx
	mov $0x00B8002, %esi
	movw %cx, %ds:0(%esi)
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
	mov $512, %bx
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
1:	mov $0x0200+'D', %bx	# Status 'D'
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
	mov dlc, %ax
	inc %ax
	mov %ax, dlc
	cmp $128, %ax
	jl 2f
	mov $'.', %bl
	call chout
	xor %ax, %ax
	mov %ax, dlc
2:	mov pxestix+4, %ax
	inc %ax
	shr $1, %ax
	mov %ax, %cx
	mov kld, %edi
	mov $0x1000, %si
3:	lodsw
	stosw %ax, %es:(%edi)
	loop 3b
	mov %edi, kld
	mov pxestix+4, %ax
	cmp $512, %ax
	je 1b

	mov $pxestix, %di	# close file
	mov $0x0021, %bx
	call pxecall
	mov $0x0200+'K', %bx	# Status 'K'
	call rmout
	1:
	hlt
	jmp 1b
rmout:
	push %si
	mov $0x00B8002, %esi
	movw %bx, %ds:0(%esi)
	pop %si
	ret

