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

namespace hw {

PCI::PCI() {
}
PCI::~PCI() {
}

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

uint16_t PCI::readw(uint8_t b, uint8_t d, uint8_t f, uint8_t o) {
	uint32_t dva = (1 << 31) | (b << 16) | (d << 11) | (f << 8) | (o & ~3);
	_ix_outl(0x0cf8, dva);
	if(o & 2)
		return (uint16_t)(_ix_inl(0xcfc) >> 16);
	else
		return (uint16_t)_ix_inl(0xcfc);
}
uint32_t PCI::readl(uint8_t b, uint8_t d, uint8_t f, uint8_t o) {
	uint32_t dva = (1 << 31) | (b << 16) | (d << 11) | (f << 8) | (o & ~3);
	_ix_outl(0x0cf8, dva);
	return _ix_inl(0xcfc);
}

void PCI::dev_dump(uint8_t bus) {
	for(int i = 0; i < 31; i++) {
		uint32_t w, ex, dcl, hci;
		uint8_t htype;
		w = readl(bus, i, 0, 0);
		if((w & 0xffff) != 0xffff) {
			dcl = readl(bus, i, 0, 0x8);
			hci = readl(bus, i, 0, 0xC);
			htype = (uint8_t)(hci >> 16);
			if(htype & 0x80) {
				for(int fn = 0; fn < 8; fn++) {
				w = readl(bus, i, fn, 0);
				if((w >> 16) != 0xffff) {
				dcl = readl(bus, i, fn, 0x8);
				if((htype & 0x7f) == 0x01 && (dcl >> 16) == 0x0604) {
					ex = readl(bus, i, fn, 0x18);
					xiv::printf("PCI mf-dev PCI: %d - %x:%d {%d}\n", i, w, fn, ex);
					dev_dump((uint8_t)(ex >> 8));
				} else {
					xiv::printf("PCI mf-dev: %d - %x:%d [%x] - ", i, w, fn, dcl >> 16);
					xiv::print(get_class((uint8_t)(dcl >> 24)));
					xiv::putc(10);
					}
				}
				}
			} else {
			xiv::printf("PCI dev: %d - %x [%x] - ", i, w, dcl >> 16);
			xiv::print(get_class((uint8_t)(dcl >> 24)));
			xiv::putc(10);
			}
		}
	}
}

void PCI::bus_dump() {
	for(int i = 0; i < 256; i++) {
		uint32_t w;
		w = readl(i, 0, 0, 0);
		if((w & 0xffff) != 0xffff) {
			xiv::printf("PCI bus: %d - %x\n", i, w);
			dev_dump(i);
		}
	}
}

} // ns hw

