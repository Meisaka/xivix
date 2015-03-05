/* ***
 * kmain.cpp - C++ entry point for the kernel
 * Copyright (C) 2014-2015  Meisaka Yukara
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 *     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "ktypes.hpp"
#include "kio.hpp"
#include "dev/vgatext.hpp"
#include "dev/fbtext.hpp"
#include "dev/ps2.hpp"

#include "memory.hpp"

#include <stdarg.h>

struct mmentry {
	uint64_t start;
	uint64_t size;
	uint32_t type;
	uint32_t exattrib;
};

extern "C" {

extern char _kernel_end;

void* malloc(size_t v) {
	return kmalloc(v);
}
void free(void *f) {
	kfree(f);
}
void * operator new(size_t v) {
	return kmalloc(v);
}
void * operator new[](size_t v) {
	return kmalloc(v);
}
void operator delete(void *d) {
	kfree(d);
}

void abort() {
	_ix_totalhalt();
}
size_t strlen(char *p) {
	size_t r = 0;
	while(*p != 0) r++;
	return r;
}
} // extern C

namespace xiv {

static TextIO *txtout = nullptr;

void putc(char c) {
	if(txtout == nullptr) return;
	switch(c) {
	case 10:
		txtout->nextline();
		break;
	default:
		txtout->putc(c);
	}
}

void printn(const char* s, size_t n) {
	while(n--) { putc(*(s++)); }
}

void print(const char* s) {
	while(*s != 0) { putc(*(s++)); }
}

void printdec(uint32_t d) {
	char num[10];
	unsigned l = 0;
	while(d) {
		num[9-l] = (d % 10) + '0';
		d = d / 10;
		l++;
	}
	printn(num+(10-l), l);
}

void printhex(uint32_t v) {
	int i;
	for(i = 32; i > 4 && ((v >> (i-4)) & 0x0f) == 0; i-=4); // get digit length
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		putc(c);
	} while(i > 0);
}
void printhex(uint64_t v) {
	int i;
	for(i = 64; i > 4 && ((v >> (i-4)) & 0x0f) == 0; i-=4); // get digit length
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		putc(c);
	} while(i > 0);
}
void printhex(uint32_t v, uint32_t bits) {
	if(bits > 32) bits = 32;
	int i = bits ^ (bits & 0x3);
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		putc(c);
	} while(i > 0);
}
void printhexx(uint64_t v, uint32_t bits) {
	if(bits > 64) bits = 64;
	int i = bits ^ (bits & 0x3);
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		putc(c);
	} while(i > 0);
}

void printf(const char *ftr, ...) {
	va_list v;
	unsigned rs = 0;
	uint32_t num;

	va_start(v, ftr);

	while(*ftr) {
		switch(rs) {
		case 0:
			if(*ftr == '%') {
				rs = 1;
			} else {
				putc(*ftr);
			}
			break;
		case 1:
			switch(*ftr) {
			case '%':
				rs = 0;
				putc(*ftr);
				break;
			case 'd':
				rs = 0;
				num = va_arg(v, uint32_t);
				printdec(num);
				break;
			case 'x':
				rs = 0;
				num = va_arg(v, uint32_t);
				printhex(num);
				break;
			default:
				putc('%');
				putc(*ftr);
			}
		}
		ftr++;
	}
	va_end(v);
}

void show_mem_map() {
	uint32_t lim = *((uint16_t*)0x500);
	mmentry *mo = (mmentry*)0x800;
	for(uint32_t i = 0; i < lim; i++) {
		printhexx(mo->start, 64);
		printf(" - ");
		printhex(mo->size);
		printf(" - %d (%x)\n", mo->type, mo->exattrib);
		mo++;
	}
}

} // ns xiv

char skeymap[256] = {
//  0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  '`',  0,
    0,   0,   0,   0,   0,  'q', '1',  0,   0,   0,  'z', 's', 'a', 'w', '2',  0,
    0,  'c', 'x', 'd', 'e', '4', '3',  0,   0,  ' ', 'v', 'f', 't', 'r', '5',  0,
    0,  'n', 'b', 'h', 'g', 'y', '6',  0,   0,   0,  'm', 'j', 'u', '7', '8',  0,
    0,  ',', 'k', 'i', 'o', '0', '9',  0,   0,  '.', '/', 'l', ';', 'p', '-',  0,
    0,   0, '\'',  0,  '[', '=',  0,   0,   0,   0,  10,  ']',  0, '\\',  0,   0,
    0,   0,   0,   0,   0,   0,   8,   0,   0,  '1',  0,  '4', '7',  0,   0,   0,
   '0', '.', '2', '5', '6', '8', 27,   0,   0,  '+', '3', '-', '*', '9',  0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};
char sskeymap[256] = {
//  0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  '~',  0,
    0,   0,   0,   0,   0,  'Q', '!',  0,   0,   0,  'Z', 'S', 'A', 'W', '@',  0,
    0,  'C', 'X', 'D', 'E', '$', '#',  0,   0,  ' ', 'V', 'F', 'T', 'R', '%',  0,
    0,  'N', 'B', 'H', 'G', 'Y', '^',  0,   0,   0,  'M', 'J', 'U', '&', '*',  0,
    0,  '<', 'K', 'I', 'O', ')', '(',  0,   0,  '>', '?', 'L', ':', 'P', '_',  0,
    0,   0, '\"',  0,  '{', '+',  0,   0,   0,   0,  10,  '}',  0,  '|',  0,   0,
    0,   0,   0,   0,   0,   0,   8,   0,   0,  '1',  0,  '4', '7',  0,   0,   0,
   '0', '.', '2', '5', '6', '8', 27,   0,   0,  '+', '3', '-', '*', '9',  0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};
uint8_t mapchar(uint32_t k, uint32_t mods) {
	if(k & 0x10000) return 0;
	if(k & 0x60000) return 0;
	if((k & 0xffff) < 256) {
		if(mods & 0x3) {
			return sskeymap[k & 0xff];
		} else {
			return skeymap[k & 0xff];
		}
	}
	return 0;
}

typedef uint32_t dd;
typedef uint16_t dw;
typedef uint8_t db;

#pragma pack(push, 1)
struct VBEModeInfo {
	dw mode_attrib;
	db wina_attrib;
	db winb_attrib;
	dw win_gran;
	dw win_size;
	dw wina_seg;
	dw winb_seg;
	dd win_func_ptr;
	dw bytes_per_scanline;
	dw x_res;
	dw y_res;
	db x_char;
	db y_char;
	db planes;
	db bits_per_pixel;
	db banks;
	db mem_model;
	db bank_size;
	db imagepages;
	db reserved_1;
	db red_mask;
	db red_pos;
	db green_mask;
	db green_pos;
	db blue_mask;
	db blue_pos;
	db resv_mask;
	db resv_pos;
	db direct_color_mode_info;
	dd phys_base;
	dd reserved_2;
	dw reserved_3;
	dw lin_bytes_per_scanline;
	db bank_imagepages;
	db lin_imagepages;
	db lin_red_mask;
	db lin_red_pos;
	db lin_green_mask;
	db lin_green_pos;
	db lin_blue_mask;
	db lin_blue_pos;
	db lin_resv_mask;
	db lin_resv_pos;
	dd max_pixel_clock;
	db reserved_4[189];
};
#pragma pack(pop)

extern "C" {
void _kernel_main() {
	using namespace xiv;
	uint32_t k = 0;
	VGAText *lvga = new VGAText();
	xiv::txtout = lvga;
	printf("xivix Text mode hello\n");
	mem::initialize();
	printf("fetching VBE...\n");
	VBEModeInfo *vidinfo = reinterpret_cast<VBEModeInfo*>(0x1200);
	printf("mapping pages...\n");
	{
		uint64_t pbase = vidinfo->phys_base;
		uint32_t vbase = 0xd0000000;
		for(int j = 0; j < 4; j++) {
			mem::map_page({pbase}, vbase, mem::MAP_RW | mem::MAP_LARGE);
			pbase += 0x200000;
			vbase += 0x200000;
		}
	}
	uint32_t *vid = reinterpret_cast<uint32_t*>(0xd0000000);
	printf("display clear...\n");
	for(uint32_t y = 0; y < 102400; y++) {
		vid[y] = 0x0;
	}
	FramebufferText *fbt = new FramebufferText(vid, vidinfo->x_res * (vidinfo->bits_per_pixel / 8), vidinfo->bits_per_pixel);
	xiv::txtout = fbt;
	printf("xivix hello!\n");
	printf("Video info: %x %dx%d : %d @%x\nMModel: %x\n",
			vidinfo->mode_attrib,
			vidinfo->x_res,
			vidinfo->y_res,
			vidinfo->bits_per_pixel,
			vidinfo->phys_base,
			vidinfo->mem_model
		 );

	show_mem_map();

	{
		uint16_t vba;
		_ix_outw(0x1CE, 0);
		vba = _ix_inw(0x1cf);
		putc(' ');
		printhex(vba, 16);
	}

	hw::PS2 &psys = hw::PS2::dev;
	hw::Keyboard *kb1 = new hw::Keyboard();
	kb1->lastkey2 = 0xffff;
	psys.add_kbd(kb1);
	psys.init();
	bool busy = false;
	uint32_t nxf = _ivix_int_n + 40;
	bool flk = false;
	while(true) {
		if(_ivix_int_n > nxf) {
			nxf = _ivix_int_n + 10;
			flk =! flk;
			fbt->putat(fbt->getcol(), fbt->getrow(), flk?'_':' ');
		}
		/*
		vga.setto(65,10);
		printhex(kb1->state, 32);
		vga.setto(65,11);
		printhex(kb1->lastkey1, 32);
		vga.setto(65,12);
		printhex(kb1->lastkey2, 32);
		vga.setto(65,13);
		printhex(kb1->lastscan, 8);
		putc(' ');
		printhex(kb1->mods, 32);
		vga.setto(65,14);
		printhex(reinterpret_cast<uint32_t>(psys.kb_drv), 32);
		vga.setto(65,15);
		printhex(reinterpret_cast<uint32_t>(psys.port[0].cl_ptr), 32);
		vga.setto(65,16);
		printhex(reinterpret_cast<uint32_t>(psys.port[1].cl_ptr), 32);
		vga.setto(65,17);
		printhex(reinterpret_cast<uint32_t>(kb1->mp_serv), 32);
		// */
		//lvga->setto(51,0);
		//printhex(_ivix_int_n, 32);
		//lvga->setto(51,1);
		//printhex(k, 32);
		/*
		vga.setto(45,21);
		printhex(psys.err_n, 32);
		vga.setto(55,20);
		printhex(cast<uint32_t>(psys.port[0].status), 32);
		vga.setto(65,20);
		printhex(cast<uint32_t>(psys.port[1].status), 32);
		vga.setto(55,21);
		printhex(cast<uint32_t>(psys.port[0].type), 32);
		vga.setto(65,21);
		printhex(cast<uint32_t>(psys.port[1].type), 32);
		vga.setto(55,22);
		printhex(psys.icode[0], 8);
		vga.setto(58,22);
		printhex(psys.istatus[0], 8);
		vga.setto(61,22);
		printhex(psys.icode[1], 8);
		vga.setto(64,22);
		printhex(psys.istatus[1], 8);
		vga.setto(55,23);
		printhex(psys.interupted, 32);
		// */
		psys.handle();
		if(psys.waiting()) {
			psys.handle();
			k++;
			busy = true;
		}
		if(kb1->has_key()) {
			busy = true;
			uint32_t k = kb1->pop_key();
			uint8_t ch = mapchar(k, kb1->mods);
			if(ch) {
				if(ch == 10) {
				fbt->putat(fbt->getcol(), fbt->getrow(), ' ');
				}
				putc(ch);
				nxf = _ivix_int_n + 10;
				flk = true;
				fbt->putat(fbt->getcol(), fbt->getrow(), '_');
			}
		}
		if(!busy) {
			_ix_halt();
		}
		busy = false;
	}
	_ix_totalhalt();
}

} // extern C

