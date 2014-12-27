
prefix := "/ixivix/bin"
as := "$(prefix)/i686-elf-as"
ld := "$(prefix)/i686-elf-ld"
cc := "$(prefix)/i686-elf-gcc"
objdump := "$(prefix)/i686-elf-objdump"

all: pxe.img

mftest:
	echo $(ld)

wc:
	wc pxe.img

dump: pxe.img
	$(objdump) -D -b binary -m i8086 pxe.img | less

pxeboot.o: pxeboot.s
	$(as) pxeboot.s -o pxeboot.o

pxe.img: pxeboot.o Makefile
	$(ld) -Ttext 07c00 --oformat binary -o pxe.img pxeboot.o

