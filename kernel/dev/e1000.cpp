/* ***
 * e1000.cpp - e1000 / 8257x driver
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
#include "e1000.hpp"
#include "ktext.hpp"
#include "memory.hpp"

namespace hw {

static void readeeprom(uint8_t* base, uint32_t epma) {
	volatile uint32_t *eerd = (uint32_t*)(base+0x14);
	*eerd = 1u | (epma << 2);
	while(!(*eerd & 0x2)) { }
	xiv::printf("Read: %x: %x\n", epma, *eerd);
}

e1000::e1000(pci::PCIBlock &pcib) {
	xiv::print("e1000: init...\n");
	xiv::printf("e1000: base: %x / %x\n", pcib.bar[0], pcib.barsz[0]);
	uint64_t piobase = (pcib.bar[0] & 0xfffffff0);
	uint8_t *viobase = (uint8_t*)(pcib.bar[0] & 0xfffffff0);
	xiv::printhexx(piobase, 64);
	mem::request(pcib.barsz[0], viobase, piobase, mem::RQ_HINT | mem::RQ_RW);
	for(int x = 0; x < 8; x++) {
	xiv::printf("Read: %x %x\n", x*4, ((uint32_t*)viobase)[x]);
	}
	for(uint32_t x = 0; x < 8; x++) {
		readeeprom(viobase, x);
	}
}

e1000::~e1000() {
}

}

