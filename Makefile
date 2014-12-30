
prefix := "/ixivix/bin"
as := "$(prefix)/i686-elf-as"
ld := "$(prefix)/i686-elf-ld"
cc := "$(prefix)/i686-elf-gcc"
cxx := "$(prefix)/i686-elf-g++"
objdump := "$(prefix)/i686-elf-objdump"
objcopy := "$(prefix)/i686-elf-objcopy"
CXXFLAGS := -std=c++11 -ffreestanding -O2 -Wall -Wextra -fno-rtti -fno-exceptions
KLINKFLAGS := -ffreestanding -O2 -nostdlib
KSRC := $(shell find ./kernel -name '*.cpp')
KMAKEBASE := $(patsubst ./kernel/%,./make/%,$(basename $(KSRC)))
KOBJ := $(addsuffix .o,$(KMAKEBASE))
KDEP := $(addsuffix .dep,$(KMAKEBASE))

.PHONY: dump
.PHONY: all
.PHONY: mf
.PHONY: mftest

all: pxe.img kernel.img

mftest: $(KSRC) 
	echo $(KSRC) $(KMAKEBASE)

mf: $(KOBJ)

dump: pxe.img
	$(objdump) -D -b binary -m i8086 pxe.img | less

pxeboot.o: pxeboot.s
	$(as) pxeboot.s -o pxeboot.o

pxe.img: pxeboot.o pxe.ld
	$(ld) -T pxe.ld --oformat binary -o pxe.img pxeboot.o

make/%.dep: kernel/%.cpp Makefile
	@echo Checkdep $@ $*
	@set -e; rm -f $@; \
		$(cxx) -MM $< $(CXXFLAGS) | sed 's,\($*\)\.o[ :]*,make/\1\.o $@ : ,g' > $@

include $(KDEP)

make/%.o: kernel/%.cpp Makefile
	$(cxx) -c $< -o $@ $(CXXFLAGS)

make/ixkm.o: kernel/ixkm_ia32.s Makefile
	$(as) $< -o $@

kernel.elf: Makefile krnl.ld make/ixkm.o $(KOBJ)
	$(cxx) -T krnl.ld -o kernel.elf $(KLINKFLAGS) make/ixkm.o $(KOBJ) -lgcc

kernel.img: kernel.elf
	$(objcopy) -O binary kernel.elf kernel.img
