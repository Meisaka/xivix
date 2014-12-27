.text
_start:
.global _start
.code16

.set gdtsave, 0x500
.set gdtnew, 0x506

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
	push %si
	mov $6, %cx
	call strout
	pop %si
	xor %ax, %ax
	mov %ax, %es		# make ES zero
	mov %ax, %cx
	mov 0x6(%si), %bx
	call hex16out		# Version
	mov 0x8(%si), %bx
	call hex16out		# Length | Checksum_add << 8
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
	mov $4, %cx
	call strout		# tag name
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
	mov 0x16(%si), %bx
	call hex16out
	mov 0x14(%si), %bx
	call hex16out
	mov 0x1C(%si), %bx
	call hex16out
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
	sgdt gdtsave
	mov gdtsave, %bx
	call hex16out
	mov gdtsave+4, %bx
	call hex16out
	mov gdtsave+2, %bx
	call hex16out
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
scnb:	.word 0xb800
msgb:	.ascii "meivix"
msgf:	.ascii "**BOOT FAIL**"
msgok:	.ascii "[OK]"
msgold:	.ascii " PXE LEGACY MODE "
msgpxe: .ascii "PXENV+"
msgmorepxe: .ascii "!PXE"
msga1:	.ascii "A20 OFF "
msga2:	.ascii "A20 ON  "

	nop
	nop
	nop
	nop
pxecall:
	push %ds
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
