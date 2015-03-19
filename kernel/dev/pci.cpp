/* ***
 * pci.cpp - PCI bus driver
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

#include "pci.hpp"
#include "kio.hpp"
#include "ktext.hpp"

namespace pci {

PCI::PCI() {
}
PCI::~PCI() {
}

union Multiword {
	uint32_t w;
	uint16_t h[2];
	uint8_t b[4];
};

static const char * const PCI_CLASSES[] = {
	"Unknown",
	"Mass Store",
	"Network Ctrl",
	"Display Ctrl",
	"Multimedia",
	"Memory Ctrl",
	"Bridge",
	"Simple Comm",
	"System Prph",
	"Input Dev",
	"Docking",
	"Processor",
	"Serial Bus",
	"Wireless",
	"Int I/O Ctrl",
	"Sat Comm",
	"Crypto Ctrl",
	"DA/DSP Ctrl",
	"-Reserved-",
	"-Undef-",
};

static const char * get_class(uint8_t cc) {
	if(cc == 0xff) return PCI_CLASSES[0x13];
	if(cc > 0x12) cc = 0x12;
	return PCI_CLASSES[cc];
}

uint16_t readw(uint8_t b, uint8_t d, uint8_t f, uint8_t o) {
	uint32_t dva = (1 << 31) | (b << 16) | (d << 11) | (f << 8) | (o & ~3);
	_ix_outl(0x0cf8, dva);
	if(o & 2)
		return (uint16_t)(_ix_inl(0xcfc) >> 16);
	else
		return (uint16_t)_ix_inl(0xcfc);
}
uint32_t readl(uint8_t b, uint8_t d, uint8_t f, uint8_t o) {
	uint32_t dva = (1 << 31) | (b << 16) | (d << 11) | (f << 8) | (o & ~3);
	_ix_outl(0x0cf8, dva);
	return _ix_inl(0xcfc);
}
void writel(uint32_t val, uint8_t b, uint8_t d, uint8_t f, uint8_t o) {
	uint32_t dva = (1 << 31) | (b << 16) | (d << 11) | (f << 8) | (o & ~3);
	_ix_outl(0x0cf8, dva);
	_ix_outl(0xcfc, val);
}

void scanbar(uint8_t b, uint8_t d, uint8_t f, uint32_t bar, uint32_t &r, uint32_t &sz) {
	if(bar > 5) return;
	bar = 0x10 + (bar << 2);
	uint32_t lr, lsz;
	lr = readl(b, d, f, bar);
	writel(0xffffffff, b, d, f, bar);
	lsz = readl(b, d, f, bar);
	writel(lr, b, d, f, bar);
	lsz &= (lr & 1)? 0xfffffffc : 0xfffffff0;
	lsz = ~lsz + 1;
	r = lr; sz = lsz;
}

void dev_fn_check(uint8_t bus, uint8_t dev, uint8_t fn) {
	Multiword info, dcl, hci, ex;
	uint32_t bar[6];
	uint32_t msz[6];
	info.w = readl(bus, dev, fn, 0);
	dcl.w = readl(bus, dev, fn, 0x8);
	hci.w = readl(bus, dev, 0, 0xC);
	xiv::printf("PCI %d/%d/%d - %x:%x ", bus, dev, fn, info.h[0], info.h[1]);
	if((hci.b[2] & 0x7f) == 0x01 && dcl.h[1] == 0x0604) {
		ex.w = readl(bus, dev, fn, 0x18);
		xiv::printf("bus: %x **nxt: %d\n", ex.w, ex.b[1]);
		dev_dump(ex.b[1]);
	} else if((hci.b[2] & 0x7f) == 0x00) {
		xiv::printf("mf: %x [%x] - ", hci.b[2], dcl.h[1]);
		xiv::print(get_class(dcl.b[3]));
		scanbar(bus, dev, fn, 0, bar[0], msz[0]);
		scanbar(bus, dev, fn, 1, bar[1], msz[1]);
		scanbar(bus, dev, fn, 2, bar[2], msz[2]);
		scanbar(bus, dev, fn, 3, bar[3], msz[3]);
		scanbar(bus, dev, fn, 4, bar[4], msz[4]);
		scanbar(bus, dev, fn, 5, bar[5], msz[5]);
		xiv::printf("  **%x:%x:%x", bar[0], bar[1], bar[2]);
		xiv::printf(" %x:%x:%x", bar[3], bar[4], bar[5]);
		//v::printf(" (%x:%x:%x)", msz[0], msz[1], msz[2]);
		xiv::putc(10);
	} else {
		xiv::printf("other: %x [%x] - ", hci.b[2], dcl.h[1]);
		xiv::print(get_class(dcl.b[3]));
		xiv::putc(10);
	}
}

void dev_dump(uint8_t bus) {
	for(int i = 0; i < 31; i++) {
		Multiword info, hci;
		info.w = readl(bus, i, 0, 0);
		if(info.h[0] != 0xffff) {
			hci.w = readl(bus, i, 0, 0xC);
			if(hci.b[2] & 0x80) {
				for(int fn = 0; fn < 8; fn++) {
					info.w = readl(bus, i, fn, 0);
					if(info.h[1] != 0xffff) {
						dev_fn_check(bus, i, fn);
					}
				}
			} else {
				dev_fn_check(bus, i, 0);
			}
		}
	}
}

void bus_dump() {
	dev_dump(0);
}

} // ns pci

