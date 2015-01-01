
prefix := "/ixivix/bin"
ISA = i686
PLAT = elf
ARCH = $(ISA)-$(PLAT)
INCL = ./kernel

as := $(ARCH)-as
ld := $(ARCH)-ld
cc := $(ARCH)-gcc
cxx := $(ARCH)-g++
objdump := $(ARCH)-objdump
objcopy := $(ARCH)-objcopy
CXXFLAGS := -std=c++11 -ffreestanding -iquote $(INCL) -O2 -Wall -Wextra -fno-rtti -fno-exceptions
KLINKFLAGS := -ffreestanding -O2 -nostdlib

.PHONY: dump
.PHONY: all
.PHONY: clean
.PHONY: clear
.PHONY: mf
.PHONY: mftest

all: pxe.img kernel.img mf

mf: $(KOBJ)

KSRC = $(shell find ./kernel -name '*.cpp')
KASM = $(shell find ./kernel/$(ISA) -name '*.s')
KSRCBASE = $(patsubst ./kernel/%,./make/%,$(basename $(KSRC)))
KASMBASE = $(patsubst ./kernel/%.s,./make/%,$(KASM))
KOBJ = $(addsuffix .o,$(KASMBASE)) $(addsuffix .o,$(KSRCBASE))
KDEP = $(addsuffix .dep,$(KSRCBASE))
KCLEAR = $(shell find ./make -name '*.o' -or -name '*.dep')

mftest: $(KSRC) 
	echo $(ISA) $(ARCH)

clean: clear
clear: $(KCLEAR)
	rm -f $(KCLEAR)

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

make/%.o: kernel/%.s Makefile
	$(as) $< -o $@

make/%.o: kernel/%.cpp Makefile
	$(cxx) -c $< -o $@ $(CXXFLAGS)

kernel.elf: Makefile krnl.ld $(KOBJ) mf
	$(cxx) -T krnl.ld -o kernel.elf $(KLINKFLAGS) $(KOBJ) -lgcc

kernel.img: kernel.elf
	$(objcopy) -O binary kernel.elf kernel.img

