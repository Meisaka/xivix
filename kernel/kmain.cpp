
#include "ktypes.hpp"
#include "vgatext.hpp"

struct mmentry {
	uint64_t start;
	uint64_t size;
	uint32_t type;
	uint32_t exattrib;
};

extern "C" {
void _ix_halt();
void _ix_totalhalt();
void _ix_req();
void _ix_reqr();
extern volatile uint32_t _ivix_int_n;

void* malloc(size_t v) {
	if(v < 1) {
		return (void*)0x1000;
	}
	return nullptr;
}
void free(void *f) {
	if(f == ((void*)0x1000) ) {
		*((uint32_t*)f) = 7;
	}
}
void abort() {
	_ix_totalhalt();
}
size_t strlen(char *p) {
	size_t r = 0;
	while(*p != 0) r++;
	return r;
}

void _kernel_main() {
	uint32_t i = 0;
	uint32_t lim = *((uint16_t*)0x500);
	VGAText vga;
	vga.putstr("Ixivix Kernel says hello! ");
	vga.puthex32(0xfedc9876);
	while(true) {
		i++;
		if(i % 900000 == 0) {
			vga.setto(45,18);
			vga.puthex32(i);
			vga.setto(45,19);
			vga.puthex32(_ivix_int_n);
			vga.putat(50,20,'#');
			_ix_halt();
		}
	}
	mmentry *mo = (mmentry*)0x800;
	for(i = 0; i < lim; i++) {
		if(mo->type != 1) {
			mo->type = 2;
		}
		mo++;
	}
	_ix_totalhalt();
}

} // extern C

