/* ***
 * kmain.cpp - C++ entry point for the kernel
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "ktypes.hpp"
#include "kio.hpp"
#include "ktext.hpp"
#include "dev/drivers.hpp"
#include "dev/vgatext.hpp"
#include "dev/fbtext.hpp"
#include "dev/ps2.hpp"
#include "dev/pci.hpp"
#include "dev/cmos.hpp"

#include "memory.hpp"
#include "acpi.hpp"
#include "scheduler.hpp"

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
void memset(void *dst, int ivalue, size_t len) {
	uint32_t value = (ivalue & 0xffu) * 0x1010101u;
	uint8_t *dest_bytes = (uint8_t*)dst;
	void *end = dest_bytes + len;
	if(len < 16) {
		while(dest_bytes < end) *(dest_bytes++) = value & 0xff;
		return;
	}
	uint32_t align = ((uintptr_t)dest_bytes) & 0xf;
	if(align & 0x3) {
		for(size_t n = 0; n < (4 - (align & 3)); n++) {
			*(dest_bytes++) = value & 0xff;
		}
	}
	uint32_t *dest32 = (uint32_t*)dest_bytes;
	uint32_t *end32 = (uint32_t*)(((uintptr_t)end) & ~0x3);
	while(dest32 < end32) {
		*(dest32++) = value;
	}
	dest_bytes = (uint8_t*)dest32;
	while(dest_bytes < end) {
		*(dest_bytes++) = value & 0xff;
	}
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
		printf("%016lx", mo->start);
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

int memeq(const void * ptra, const void * ptrb, size_t l) {
	int equ = 0;
	const uint8_t *a, *b;
	a = (const uint8_t *)ptra;
	b = (const uint8_t *)ptrb;
	for(; l; l--) {
		equ |= (*a) ^ (*b);
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
FramebufferText *fbt_status;
VGAText lvga;
hw::Keyboard *kb1;
hw::Mouse *mou1;
static hw::PS2 *psys;
static uint32_t *vid = nullptr;
static uint32_t vid_stride = 0;
Scheduler scheduler;
constexpr int cmdlen = 4096;
int cmdx = 0, cmdl = 0;
char *cmd;
uint32_t nxf;
bool flk = false;
void kernel_loop();

net::sockaddr test_sock { 0, 0x0100, 0xffffffff };
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
			case '!':
				scheduler.run();
				break;
			case 'D':
				mem::debug(4);
				break;
			case 'd':
				mem::debug(0);
				break;
			case 'T':
				xiv::printf("CMOS timer count=%x\n", hw::CMOS::instance.timer);
				break;
			case 't':
				xiv::printf("timer count=%x\n", _iv_int_n);
				break;
			case 'a':
				xiv::printf("APIC count=%x\n", _iv_int_ac);
				break;
			case 'w':
				if(hw::ethdev)
					net::send_string_udp(9001, &test_sock, "Xivix network test message\n");
				else xiv::printf("No network dev\n");
				break;
			case 'p':
				mem::debug(1);
				break;
			case 'v':
				mem::debug(2);
				break;
			case 'r':
				xiv::printf("Random: %x\n", ixa4random());
				break;
			}
		} else if(cmdx >= 3) {
			if(!memeq(cmd, "arp", 3)) net::debug_arp();
			if(!memeq(cmd, "pkt", 3)) net::debug_packet();
			if(!memeq(cmd, "die", 3)) {
				*((uint32_t*)0x80000000) = 0;
			}
			if(!memeq(cmd, "eth", 3)) hw::ethdev->debug_cmd(cmd + 3, cmdx - 3);
			if(!memeq(cmd, "acpi", 4)) acpi::debug_acpi(cmd + 4, cmdx - 4);
			if((cmdx == 6) && (memeq(cmd, "serial", 6) == 0)) {
				xiv::togglecomm();
			}
			if((cmdx == 6) && (memeq(cmd, "/debug", 6) == 0)) {
				mem::debug(2);
			}
		}
		cmdl = cmdx;
		cmdx = 0;
	}
	nxf = _iv_int_f + 100;
	flk = true;
	if(fbt) fbt->putat(svt->getcol(), svt->getrow(), '_');
}

xiv::VTCell static_vt_buffer[240*128];

void kernel_loop(void*) {
	uint32_t last_timer = hw::CMOS::instance.timer;
	wake_on_interrupt(0x28);
	while(true) {
		auto &cmos = hw::CMOS::instance;
		if(last_timer != cmos.timer) {
			last_timer = cmos.timer;
			fbt_status->setto(0, 1);
			xiv::iprintf(fbt_status, "RTC: %02d:%02d:%02d d%d m%d y%04d up:%d\n",
				cmos.hour, cmos.minute, cmos.second,
				cmos.monthday, cmos.month, cmos.year, cmos.timer);
		}
		wait_for_interrupts();
		//_ix_halt();
	}
}
void kernel_net_loop(void*) {
	net::init();
	wake_on_interrupt(hw::ethdev->int_line);
	wake_on_interrupt(0x28);
	while(true) {
		net::runsched();
		wait_for_interrupts();
	}
}
extern "C" uint32_t _kernel_test_eq();
void kernel_cli_loop(void*) {
	nxf = _iv_int_f + 40; // TODO proper Timers!
	wake_on_interrupt(0x21);
	wake_on_interrupt(0x28);
	wake_on_interrupt(0x2c);
	while(true) {
		if(_iv_int_f > nxf) {
			nxf = _iv_int_f + 250;
			flk =! flk;
			if(fbt) {
				fbt->putat(svt->getcol(), svt->getrow(), flk?'_':' ');
			}
		}
		psys->handle();
		if(psys->waiting()) {
			psys->handle();
		}
		if(mou1->status & 0x80) {
			mou1->status &= 0x7f;
			if(vid) {
				uint32_t *cur = vid + ((vid_stride / 4) * mou1->y) + mou1->x;
				cur[0] = 0xffffff;
			}
		}
		if(kb1->has_key()) {
			handle_key();
		}
		wait_for_interrupts();
	}
}

extern "C" void _kernel_main() {
	using namespace xiv;
	xiv::txtout = &lvga;
	const char *border_str = "**************************************";
	printf("%s%s\n xivix Text mode hello\n%s%s\n",border_str,border_str,border_str,border_str);
	VirtTerm lvt;
	lvt.width = 200;
	lvt.height = 96;
	lvt.buffer = static_vt_buffer;
	lvt.reset();
	//xiv::txtout = svt = &lvt;
	mem::initialize();
	Crypto::Init();
	// Dynamic VTerm
	//svt = new VirtTerm(240, 128);
	//xiv::txtout = svt;
	printf("Fetching VBE\n");
	VBEModeInfo *vidinfo = reinterpret_cast<VBEModeInfo*>(0xc0001200);
	if(vidinfo->phys_base) {
		printf("Mapping video pages...\n");
		uint64_t pbase = vidinfo->phys_base;
		vid = reinterpret_cast<uint32_t*>(0xd0000000);
		mem::vmm_request(0x800000, vid, pbase, mem::RQ_RW | mem::RQ_LARGE | mem::RQ_HINT);
		uint32_t vidl = vidinfo->x_res * vidinfo->y_res;
		printf("Display clear\n");
		for(uint32_t y = 0; y < vidl; y++) {
			vid[y] = 0x00040a;
		}
		printf("Video info: %x %dx%d : %d @%x\nMModel: %x\n",
				vidinfo->mode_attrib,
				vidinfo->x_res,
				vidinfo->y_res,
				vidinfo->bits_per_pixel,
				vidinfo->phys_base,
				vidinfo->mem_model
			 );
		vid_stride = vidinfo->x_res * (vidinfo->bits_per_pixel / 8);
		fbt = new FramebufferText(vid, vid_stride, vidinfo->bits_per_pixel);
		fbt_status = new FramebufferText(vid, vid_stride, vidinfo->bits_per_pixel);
		svt = &lvt;
		xiv::txtout = svt;
 		uvec2 ulr = uvec2::center_align(uvec2(svt->width*6,svt->height*8), uvec2(vidinfo->x_res, vidinfo->y_res));
 		fbt->setoffset(ulr.x, ulr.y);
		fbt_status->setoffset(ulr.x, 0);
 		txtvc = svt;
 		txtfb = fbt;
		printf("Framebuffer setup complete\n");
	} else {
		printf("Not using VBE\n");
	}

	show_mem_map();

	printf("  XIVIX kernel hello!\n");
	pfinit();
	scheduler.init(); // this has to go before any interrupts
	acpi::load_acpi(); // TODO: move this to directly after mem::initialize
	//_ix_totalhalt();

	hw::init();

	printf("Scanning PS/2\n");
	psys = new hw::PS2();
	kb1 = new hw::Keyboard();
	kb1->lastkey2 = 0xffff;
	psys->add_kbd(kb1);
	psys->add_mou(mou1 = new hw::Mouse());
	mou1->xlim = vidinfo->x_res - 1;
	mou1->ylim = vidinfo->y_res - 1;
	mou1->x = mou1->xlim / 2;
	mou1->y = mou1->ylim / 2;
	psys->init();

	printf("PCI dump\n");
	pci::bus_dump();

	if(vidinfo->phys_base) fbt->render_vc(*svt);


	scheduler.start(kernel_loop, nullptr);
	scheduler.start(kernel_cli_loop, nullptr);
	scheduler.start(kernel_net_loop, nullptr);

	printf("Command loop start\n");
	cmd = (char*)kmalloc(cmdlen);

	while(true) {
		// run periodic tasks
		// owo
		scheduler.run();
		_ix_halt();
	}
	_ix_totalhalt();
}
