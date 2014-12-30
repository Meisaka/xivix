
prefix := "/ixivix/bin"
as := "$(prefix)/i686-elf-as"
ld := "$(prefix)/i686-elf-ld"
cc := "$(prefix)/i686-elf-gcc"
cxx := "$(prefix)/i686-elf-g++"
objdump := "$(prefix)/i686-elf-objdump"
objcopy := "$(prefix)/i686-elf-objcopy"
CXXFLAGS := -std=c++11 -ffreestanding -O2 -Wall -Wextra -fno-rtti -fno-exceptions
KLINKFLAGS := -ffreestanding -O2 -nostdlib
ISA := ia32

KSRC := $(shell find ./kernel -name '*.cpp')
KASM := $(shell find ./kernel -name '*_$(ISA).s')
KSRCBASE := $(patsubst ./kernel/%,./make/%,$(basename $(KSRC)))
KASMBASE := $(patsubst ./kernel/%_$(ISA).s,./make/%,$(KASM))
KOBJ := $(addsuffix .o,$(KASMBASE)) $(addsuffix .o,$(KSRCBASE))
KDEP := $(addsuffix .dep,$(KSRCBASE))

.PHONY: dump
.PHONY: all
.PHONY: mf
.PHONY: mftest

all: pxe.img kernel.img

mftest: $(KSRC) 
	echo $(KOBJ) $(KSRCBASE)

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

make/%.o: kernel/%_$(ISA).s Makefile
	$(as) $< -o $@

make/%.o: kernel/%.cpp Makefile
	$(cxx) -c $< -o $@ $(CXXFLAGS)

kernel.elf: Makefile krnl.ld $(KOBJ)
	$(cxx) -T krnl.ld -o kernel.elf $(KLINKFLAGS) $(KOBJ) -lgcc

kernel.img: kernel.elf
	$(objcopy) -O binary kernel.elf kernel.img
