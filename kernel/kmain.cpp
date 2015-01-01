
#include "ktypes.hpp"
#include "kio.hpp"
#include "dev/vgatext.hpp"
#include "dev/ps2.hpp"

struct mmentry {
	uint64_t start;
	uint64_t size;
	uint32_t type;
	uint32_t exattrib;
};

extern "C" {

void* malloc(size_t v) {
	static char *em = reinterpret_cast<char*>(0x400000);
	if(v > 1) {
		void *pa = em;
		if(v & 0xf) {
			v += 16 - (v & 0xf);
		}
		em = em + v;
		return pa;
	}
	return nullptr;
}
void free(void *f) {
	if(f == ((void*)0x1000) ) {
		*((uint32_t*)f) = 7;
	}
}
void * operator new(size_t v) {
	return malloc(v);
}
void * operator new[](size_t v) {
	return malloc(v);
}
void operator delete(void *d) {
	free(d);
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
	uint32_t k = 0;
	uint32_t lim = *((uint16_t*)0x500);
	VGAText &vga = VGAText::dev;
	vga.putstr("Ixivix hello! ");
	int *q = new int[400];
	int *qq = new int;
	vga.puthex32(reinterpret_cast<uint32_t>(qq));
	hw::PS2 &psys = hw::PS2::dev;
	hw::Keyboard *kb1 = new hw::Keyboard();
	kb1->lastkey2 = 0xffff;
	psys.add_kbd(kb1);
	psys.init();
	while(true) {
		vga.setto(65,10);
		vga.puthex32(kb1->state);
		vga.setto(65,11);
		vga.puthex32(kb1->lastkey1);
		vga.setto(65,12);
		vga.puthex32(kb1->lastkey2);
		vga.setto(65,13);
		vga.puthex8(kb1->lastscan);
		vga.setto(65,14);
		vga.puthex32(reinterpret_cast<uint32_t>(psys.kb_drv));
		vga.setto(65,15);
		vga.puthex32(reinterpret_cast<uint32_t>(psys.port[0].cl_ptr));
		vga.setto(65,16);
		vga.puthex32(reinterpret_cast<uint32_t>(psys.port[1].cl_ptr));
		vga.setto(65,17);
		vga.puthex32(reinterpret_cast<uint32_t>(kb1->mp_serv));
		vga.setto(51,0);
		vga.puthex32(_ivix_int_n);
		vga.setto(51,1);
		vga.puthex32(k);
		vga.setto(45,21);
		vga.puthex32(psys.err_n);
		vga.setto(55,20);
		vga.puthex32(cast<uint32_t>(psys.port[0].status));
		vga.setto(65,20);
		vga.puthex32(cast<uint32_t>(psys.port[1].status));
		vga.setto(55,21);
		vga.puthex32(cast<uint32_t>(psys.port[0].type));
		vga.setto(65,21);
		vga.puthex32(cast<uint32_t>(psys.port[1].type));
		vga.setto(55,22);
		vga.puthex8(psys.icode[0]);
		vga.setto(58,22);
		vga.puthex8(psys.istatus[0]);
		vga.setto(61,22);
		vga.puthex8(psys.icode[1]);
		vga.setto(64,22);
		vga.puthex8(psys.istatus[1]);
		vga.setto(55,23);
		vga.puthex32(psys.interupted);
		if(psys.waiting()) {
			psys.handle();
			k++;
		}
		_ix_halt();
	}
	mmentry *mo = (mmentry*)0x800;
	for(uint32_t i = 0; i < lim; i++) {
		if(mo->type != 1) {
			mo->type = 2;
		}
		mo++;
	}
	_ix_totalhalt();
}

} // extern C

