
prefix := "/ixivix/bin"
ISA = i686
PLAT = elf
ARCH = $(ISA)-$(PLAT)
INCL = ./kernel
PLINCL = ./kernel/$(ISA)

as := $(ARCH)-as
ld := $(ARCH)-ld
cc := $(ARCH)-gcc
cxx := $(ARCH)-g++
objdump := $(ARCH)-objdump
objcopy := $(ARCH)-objcopy
CXXFLAGS := -std=c++17 -ffreestanding -g -iquote $(INCL) -iquote $(PLINCL) -Wall -Wextra -fno-rtti -fno-exceptions
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
	@echo "[OBJDUMP]"; $(objdump) -D -b binary -m i8086 pxe.img | less

pxeboot.o: pxeboot.s
	@echo "[AS PXE ] "; $(as) pxeboot.s -o pxeboot.o

pxe.img: pxeboot.o pxe.ld
	@echo "[LD PXE ] "; $(ld) -T pxe.ld --oformat binary -o pxe.img pxeboot.o

make/%.dep: kernel/%.cpp Makefile
	@echo '|DEP C++|' '$@ $*'
	@set -e; rm -f $@; \
		$(cxx) -MM $< $(CXXFLAGS) -MT make/$*.o -MT $@ > $@

make/%.dep: kernel/%.c Makefile
	@echo '|DEP C  | ' '$@ $*'
	@set -e; rm -f $@; \
		$(cc) -MM $< $(CXXFLAGS) -MT make/$*.o -MT $@ > $@

include $(KDEP)

make/%.o: kernel/%.s Makefile
	@echo "|AS     | $<"; $(as) $< -o $@

make/%.o: kernel/%.c Makefile
	@echo "|CC     | $<"; $(cc) -c $< -o $@ $(CFLAGS)

make/%.o: kernel/%.cpp Makefile
	@echo "|C++    | $<"; $(cxx) -c $< -o $@ $(CXXFLAGS)

kernel.elf: Makefile krnl.ld $(KOBJ) mf
	@echo "[LINK   ] kernel.elf"; $(cxx) -T krnl.ld -o kernel.elf $(KLINKFLAGS) $(KOBJ) -lgcc

kernel.img: kernel.elf
	@echo "[OBJCOPY] kernel"; $(objcopy) -O binary kernel.elf kernel.img

