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
#include "ktext.hpp"
#include "dev/drivers.hpp"
#include "dev/vgatext.hpp"
#include "dev/fbtext.hpp"
#include "dev/ps2.hpp"
#include "dev/pci.hpp"

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

extern TextIO *txtout;
extern VirtTerm *txtvc;
extern FramebufferText *txtfb;

void show_mem_map() {
	uint32_t lim = *((uint16_t*)0xc0000500);
	mmentry *mo = (mmentry*)0xc0000800;
	for(uint32_t i = 0; i < lim; i++) {
		printhexx(mo->start, 64);
		printf(" - % x - %d (%x)\n", mo->size, mo->type, mo->exattrib);
		mo++;
	}
}

void pfinit();

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

#include "dev/hwtypes.hpp"
namespace hw {
	extern NetworkMAC *ethdev;
}

VGAText lvga;

extern "C" {
void _kernel_main() {
	using namespace xiv;
	uint32_t k = 0;
	xiv::txtout = &lvga;
	printf(" xivix Text mode hello\n");
	mem::initialize();
	VirtTerm *svt = new VirtTerm(128, 96);
	xiv::txtout = svt;
	printf("Fetching VBE\n");
	VBEModeInfo *vidinfo = reinterpret_cast<VBEModeInfo*>(0xc0001200);
	printf("Mapping video pages...\n");
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
	uint32_t vidl = vidinfo->x_res * vidinfo->y_res;
	printf("Display clear\n");
	for(uint32_t y = 0; y < vidl; y++) {
		vid[y] = 0x0;
	}
	FramebufferText *fbt = new FramebufferText(vid, vidinfo->x_res * (vidinfo->bits_per_pixel / 8), vidinfo->bits_per_pixel);

	{
		uvec2 ulr = uvec2::center_align(uvec2(128*8,96*8), uvec2(vidinfo->x_res, vidinfo->y_res));
		fbt->setoffset(ulr.x, ulr.y);
	}

	txtvc = svt;
	txtfb = fbt;
	printf("Video info: %x %dx%d : %d @%x\nMModel: %x\n",
			vidinfo->mode_attrib,
			vidinfo->x_res,
			vidinfo->y_res,
			vidinfo->bits_per_pixel,
			vidinfo->phys_base,
			vidinfo->mem_model
		 );

	show_mem_map();

	printf("  XIVIX kernel hello!\n");

	pfinit();
	hw::init();

	hw::PS2 *psys = new hw::PS2();
	hw::Keyboard *kb1 = new hw::Keyboard();
	kb1->lastkey2 = 0xffff;
	psys->add_kbd(kb1);
	psys->init();

	pci::bus_dump();

	fbt->render_vc(*svt);

	uint8_t *tei = (uint8_t*)kmalloc(2048);
	if(hw::ethdev) {
		hw::ethdev->init();
		int q = 0;
		for(; q < 6; q++) tei[q] = 0xff;
		hw::ethdev->getmediaaddr(&tei[q]);
		q += 6;
		tei[q++] = 0x08;
		tei[q++] = 0x00;
		tei[q++] = 0x45;
		tei[q++] = 0x00;
		tei[q++] = 0x00; //len
		tei[q++] = 70;
		tei[q++] = 0x00; // Ident
		tei[q++] = 0x00;
		tei[q++] = 0x00; // flags
		tei[q++] = 0x00; // frag
		tei[q++] = 0x08; // TTL
		tei[q++] = 0xee; // Proto
		tei[q++] = 0xb1; // chksum
		tei[q++] = 0xcb;

		tei[q++] = 0x00; // Src
		tei[q++] = 0x00;
		tei[q++] = 0x00;
		tei[q++] = 0x00;
		tei[q++] = 0xFF; // Dst
		tei[q++] = 0xFF;
		tei[q++] = 0xFF;
		tei[q++] = 0xFF;
		const char * const hai = "HELLO WORLD";
		for(int x = 0; hai[x]; x++, q++) tei[q] = (uint8_t)hai[x];
		if(hw::ethdev) hw::ethdev->transmit(tei, 14+70);
	}
	kfree(tei);
	int cmdlen = 4096;
	int cmdx = 0, cmdl = 0;
	char *cmd = (char*)kmalloc(cmdlen);
	
	bool busy = false;
	uint32_t nxf = _ivix_int_n + 40;
	bool flk = false;
	while(true) {
		if(_ivix_int_n > nxf) {
			nxf = _ivix_int_n + 10;
			flk =! flk;
			fbt->putat(svt->getcol(), svt->getrow(), flk?'_':' ');
		}
		psys->handle();
		if(psys->waiting()) {
			psys->handle();
			k++;
			busy = true;
		}
		if(kb1->has_key()) {
			busy = true;
			uint32_t k = kb1->pop_key();
			uint8_t ch = mapchar(k, kb1->mods);
			if(ch) {
				putc(ch);
				if(ch != 10) {
					fbt->render_vc(*svt);
					if(cmdx < cmdlen) cmd[cmdx++] = ch;
				} else {
					if(cmdx == 0) {
						cmdx = cmdl;
					}
					if(cmdx == 1) {
						switch(cmd[0]) {
						case 'd':
							mem::debug(0);
							break;
						case 'a':
							{
							char * leak=(char*)kmalloc(0x4000);
							for(int ux = 0; ux < 0x4000; ux++) leak[ux] = 0x55;
							mem::debug(0);
							}
							break;
						case 's':
							{
							char * leak=(char*)kmalloc(0x800);
							for(int ux = 0; ux < 0x800; ux++) leak[ux] = 0x55;
							mem::debug(0);
							}
							break;
						case 'S':
							for(int mux = 0; mux < 128; mux++) {
							char * leak=(char*)kmalloc(0x800);
							for(int ux = 0; ux < 0x800; ux++) leak[ux] = 0x55;
							}
							mem::debug(3);
							break;
						case 'h':
							{
							char * leak=(char*)kmalloc(0x1000000);
							for(int ux = 0; ux < 0x1000000; ux++) leak[ux] = 0xAA;
							}
							mem::debug(3);
							break;
						case 'l':
							{
							char * leak=(char*)kmalloc(0x10000);
							for(int ux = 0; ux < 0x10000; ux++) leak[ux] = 0x55;
							}
							mem::debug(3);
							break;
						case 'L':
							{
							char * leak=(char*)kmalloc(0x10000);
							leak[0] = 0xaa;
							}
							mem::debug(3);
							break;
						case 'p':
							mem::debug(1);
							break;
						case 'v':
							mem::debug(2);
							break;
						}
					}
					cmdl = cmdx;
					cmdx = 0;
				}
				nxf = _ivix_int_n + 10;
				flk = true;
				fbt->putat(svt->getcol(), svt->getrow(), '_');
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

