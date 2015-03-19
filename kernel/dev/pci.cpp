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

void dev_dump(uint8_t bus) {
	for(int i = 0; i < 31; i++) {
		Multiword info, dcl, hci, ex;
		info.w = readl(bus, i, 0, 0);
		if(info.h[0] != 0xffff) {
			dcl.w = readl(bus, i, 0, 0x8);
			hci.w = readl(bus, i, 0, 0xC);
			if(hci.b[2] & 0x80) {
				for(int fn = 0; fn < 8; fn++) {
				info.w = readl(bus, i, fn, 0);
				if(info.h[1] != 0xffff) {
				dcl.w = readl(bus, i, fn, 0x8);
				if((hci.b[2] & 0x7f) == 0x01 && dcl.h[1] == 0x0604) {
					ex.w = readl(bus, i, fn, 0x18);
					xiv::printf("PCI %d/%d/%d - %x:%x ", bus, i, fn, info.h[0], info.h[1]);
					xiv::printf("bus: %x nxt: %d\n", ex.w, ex.b[1]);
					dev_dump(ex.b[1]);
				} else {
					xiv::printf("PCI %d/%d/%d - %x:%x ", bus, i, fn, info.h[0], info.h[1]);
					xiv::printf("mf: [%x] - ", dcl.h[1]);
					xiv::print(get_class(dcl.b[3]));
					xiv::putc(10);
					}
				}
				}
			} else {
			xiv::printf("PCI %d/%d - %x:%x ", bus, i, info.h[0], info.h[1]);
			xiv::printf("[%x] - ", dcl.b[2]);
			xiv::print(get_class(dcl.b[3]));
			xiv::putc(10);
			}
		}
	}
}

void bus_dump() {
	dev_dump(0);
}

} // ns pci

