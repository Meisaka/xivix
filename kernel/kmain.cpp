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

int memeq(const uint8_t * a, const uint8_t * b, size_t l) {
	int equ = 0;
	for(; l; l--) {
		equ |= (*a) - (*b);
		a++; b++;
	}
	return equ;
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

xiv::VirtTerm *svt;
FramebufferText *fbt;
VGAText lvga;
hw::Keyboard *kb1;
constexpr int cmdlen = 4096;
int cmdx = 0, cmdl = 0;
char *cmd;
uint32_t nxf;
bool flk = false;
uint8_t *tei = 0;
uint32_t tei_check;

void send_via_udp(const char * msg) {
	if(!hw::ethdev) return;
	int q = 42;
	int udp_length;
	uint32_t udp_check = tei_check;
	for(udp_length = 0; msg[udp_length] && q < 1400; udp_length++, q++) {
		uint8_t c = (uint8_t)msg[udp_length];
		tei[q] = c;
		if(udp_length & 1) {
			udp_check += c;
		} else {
			udp_check += c << 8;
		}
	}
	udp_length += 8;
	uint16_t ip_length = 20 + (uint16_t)udp_length;
	if(ip_length < 50) ip_length = 50;
	tei[16] = (uint8_t)(ip_length >> 8);
	tei[17] = (uint8_t)ip_length;
	// finish the udp checksum
	udp_check += udp_length + udp_length; // psudo header + to be written
	while(udp_check & 0xffff0000u) { // the ones complement thing
		udp_check = (udp_check & 0xfffful) + (udp_check >> 16);
	}
	if(udp_check != 0xffff) udp_check = ~udp_check; // invert it
	// recalculate the IP checksum
	uint32_t ip_check = 0;
	for(q = 14; q < 34; q += 2) {
		if(q == 24) continue;
		ip_check += (tei[q] << 8) | tei[q + 1];
	}
	while(ip_check & 0xffff0000u) {
		ip_check = (ip_check & 0xfffful) + (ip_check >> 16);
	}
	ip_check = ~ip_check;
	tei[24] = (uint8_t)(ip_check >> 8);
	tei[25] = (uint8_t)ip_check;
	tei[38] = (uint8_t)(udp_length >> 8);
	tei[39] = (uint8_t)udp_length;
	tei[40] = (uint8_t)(udp_check >> 8);
	tei[41] = (uint8_t)udp_check;
	hw::ethdev->transmit(tei, 14+ip_length);
}

void handle_key() {
	uint32_t k = kb1->pop_key();
	uint8_t ch = mapchar(k, kb1->mods);
	if(!ch)
		return;
	xiv::putc(ch);
	if(ch != 10) {
		if(fbt) fbt->render_vc(*svt);
		if(cmdx < cmdlen) cmd[cmdx++] = ch;
	} else {
		if(cmdx == 0) {
			cmdx = cmdl;
		} else if(cmdx == 1) {
			switch(cmd[0]) {
			case 'D':
				mem::debug(4);
				break;
			case 'd':
				mem::debug(0);
				break;
			case 'a':
				{
				char * leak=(char*)kmalloc(0x4000);
				if(!leak) { xiv::printf("Allocation failed\n"); break; }
				for(int ux = 0; ux < 0x4000; ux++) leak[ux] = 0x55;
				mem::debug(0);
				}
				break;
			case 's':
				{
				char * leak=(char*)kmalloc(0x800);
				if(!leak) { xiv::printf("Allocation failed\n"); break; }
				for(int ux = 0; ux < 0x800; ux++) leak[ux] = 0x55;
				mem::debug(0);
				}
				break;
			case 'S':
				for(int mux = 0; mux < 128; mux++) {
				char * leak=(char*)kmalloc(0x800);
				if(!leak) { xiv::printf("Allocation failed\n"); break; }
				for(int ux = 0; ux < 0x800; ux++) leak[ux] = 0x55;
				}
				mem::debug(3);
				break;
			case 'w':
				if(hw::ethdev) send_via_udp("Xivix network test message\n");
				else xiv::printf("No network dev\n");
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
				if(!leak) { xiv::printf("Allocation failed\n"); break; }
				for(int ux = 0; ux < 0x10000; ux++) leak[ux] = 0xfe;
				}
				mem::debug(3);
				break;
			case 'L':
				{
				char * leak=(char*)kmalloc(0x10000);
				if(!leak) { xiv::printf("Allocation failed\n"); break; }
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
		} else if(cmdx == 6) {
			if(memeq((const uint8_t*)cmdx, (const uint8_t*)"/debug", 6) == 0) {
				mem::debug(2);
			}
		}
		cmdl = cmdx;
		cmdx = 0;
	}
	nxf = _ivix_int_n + 10;
	flk = true;
	if(fbt) fbt->putat(svt->getcol(), svt->getrow(), '_');
}

xiv::VTCell static_vt_buffer[240*128];

extern "C" void _kernel_main() {
	using namespace xiv;
	uint32_t k = 0;
	xiv::txtout = &lvga;
	const char *border_str = "**************************************";
	printf("%s%s\n xivix Text mode hello\n%s%s\n",border_str,border_str,border_str,border_str);
	VirtTerm lvt;
	lvt.width = 240;
	lvt.height = 96;
	lvt.buffer = static_vt_buffer;
	lvt.reset();
	//xiv::txtout = svt = &lvt;
	mem::initialize();
	// Dynamic VTerm
	//svt = new VirtTerm(240, 128);
	//xiv::txtout = svt;
	printf("Fetching VBE\n");
	VBEModeInfo *vidinfo = reinterpret_cast<VBEModeInfo*>(0xc0001200);
	if(vidinfo->phys_base) {
		printf("Mapping video pages...\n");
		uint64_t pbase = vidinfo->phys_base;
		uint32_t *vid = reinterpret_cast<uint32_t*>(0xd0000000);
		mem::vmm_request(0x800000, vid, pbase, mem::RQ_RW | mem::RQ_LARGE | mem::RQ_HINT);
		uint32_t vidl = vidinfo->x_res * vidinfo->y_res;
		printf("Display clear\n");
		for(uint32_t y = 0; y < vidl; y++) {
			vid[y] = 0x002222;
		}
		printf("Video info: %x %dx%d : %d @%x\nMModel: %x\n",
				vidinfo->mode_attrib,
				vidinfo->x_res,
				vidinfo->y_res,
				vidinfo->bits_per_pixel,
				vidinfo->phys_base,
				vidinfo->mem_model
			 );
		fbt = new FramebufferText(vid, vidinfo->x_res * (vidinfo->bits_per_pixel / 8), vidinfo->bits_per_pixel);
		svt = &lvt;
		xiv::txtout = svt;
 		uvec2 ulr = uvec2::center_align(uvec2(svt->width*6,svt->height*8), uvec2(vidinfo->x_res, vidinfo->y_res));
 		fbt->setoffset(ulr.x, ulr.y);
 		txtvc = svt;
 		txtfb = fbt;
		printf("Framebuffer setup complete\n");
	} else {
		printf("Not using VBE\n");
	}

	show_mem_map();

	printf("  XIVIX kernel hello!\n");

	//_ix_totalhalt();

	pfinit();
	hw::init();

	printf("Scanning PS/2\n");
	hw::PS2 *psys = new hw::PS2();
	kb1 = new hw::Keyboard();
	kb1->lastkey2 = 0xffff;
	psys->add_kbd(kb1);
	psys->init();

	printf("PCI dump\n");
	pci::bus_dump();

	if(vidinfo->phys_base) fbt->render_vc(*svt);

	printf("Setup packet buffer\n");
	tei = (uint8_t*)kmalloc(2048);
	if(hw::ethdev) {
		//hw::ethdev->init();
		int q = 0;
		tei[0] = 0x00; tei[1] = 0xc; tei[2] = 0x29;
		tei[3] = 0x4e; tei[4] = 0x51; tei[5] = 0x2b;
		q += 6;
		hw::ethdev->getmediaaddr(&tei[q]);
		q += 6;
		tei[q++] = 0x08;
		tei[q++] = 0x00;
		tei[q++] = 0x45;
		tei[q++] = 0x00;
		q += 2; // IP header + payload length, to be filled later
		tei[q++] = 0x00; // Ident
		tei[q++] = 0x00;
		tei[q++] = 0x00; // flags
		tei[q++] = 0x00; // frag
		tei[q++] = 0x08; // TTL
		tei[q++] = 17; // Proto
		q += 2; // checksum, to be computed
		tei[q++] = 192; // Src
		tei[q++] = 168;
		tei[q++] = 12;
		tei[q++] = 59;
		tei[q++] = 192; // Dst
		tei[q++] = 168;
		tei[q++] = 12;
		tei[q++] = 12;
		tei[q++] = 0xc0;
		tei[q++] = 0xcc;
		tei[q++] = 0x80;
		tei[q++] = 0x00;
		// length and final checksum computed later
		// compute psudo header initial checksum
		tei_check = tei[23];
		tei_check += (tei[26] << 8) | tei[27];
		tei_check += (tei[28] << 8) | tei[29];
		tei_check += (tei[30] << 8) | tei[31];
		tei_check += (tei[32] << 8) | tei[33];
		tei_check += (tei[34] << 8) | tei[35];
		tei_check += (tei[36] << 8) | tei[37];
		send_via_udp("HELLO WORLz\n");
	}

	printf("Command loop start\n");
	cmd = (char*)kmalloc(cmdlen);

	bool busy = false;
	nxf = _ivix_int_n + 40;

	while(true) {
		if(_ivix_int_n > nxf) {
			nxf = _ivix_int_n + 10;
			flk =! flk;
			if(fbt) {
				fbt->putat(svt->getcol(), svt->getrow(), flk?'_':' ');
			}
		}
		psys->handle();
		if(psys->waiting()) {
			psys->handle();
			k++;
			busy = true;
		}
		if(kb1->has_key()) {
			busy = true;
			handle_key();
		}
		if(!busy) {
			_ix_halt();
		}
		busy = false;
	}
	_ix_totalhalt();
}
