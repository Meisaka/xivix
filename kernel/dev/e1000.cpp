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

uint16_t e1000::readeeprom(uint16_t epma) {
	volatile uint32_t *eerd = (viobase+0x5);
	*eerd = 1u | (epma << 2);
	while(!(*eerd & 0x2)) { }
	return (uint16_t)(*eerd >> 16);
}

union HWR_TCTL {
	uint32_t value;
	struct {
		uint32_t rsv0 : 1;
		uint32_t en : 1;
		uint32_t rsv1 : 1;
		uint32_t psp : 1;
		uint32_t ct : 8;
		uint32_t cold : 10;
		uint32_t swxoff : 1;
		uint32_t rsv2 : 1;
		uint32_t rtlc : 1;
		uint32_t nrtu : 1;
		uint32_t rsv3 : 6;
	};
};

e1000::e1000(pci::PCIBlock &pcib) {
	xiv::print("e1000: init...\n");
	xiv::printf("e1000: base: %x / %x\n", pcib.bar[0], pcib.barsz[0]);
	uint64_t piobase = (pcib.bar[0] & 0xfffffff0);
	viobase = (uint32_t*)(pcib.bar[0] & 0xfffffff0);
	mem::request(pcib.barsz[0], viobase, piobase, mem::RQ_HINT | mem::RQ_RW);
	for(int x = 0; x < 3; x++) ((uint16_t*)macaddr)[x] = readeeprom(x);
	xiv::printf("e1000: MAC: %x", macaddr[0]);
	for(int x = 1; x < 6; x++) xiv::printf(":%x", macaddr[x]);
	xiv::putc(10);
	xiv::printf("e1000: TCTL: ");
	HWR_TCTL tctl;
	tctl.value = viobase[0x400>>2];
	if(tctl.en) xiv::print("EN ");
	if(tctl.psp) xiv::print("PSP ");
	xiv::printf("CT=%d ", tctl.ct);
	xiv::printf("COLD=%d ", tctl.cold);
	if(tctl.swxoff) xiv::print("SWXOFF ");
	if(tctl.rtlc) xiv::print("RTLC ");
	if(tctl.nrtu) xiv::print("NRTU ");
	xiv::putc(10);
	xiv::printf("e1000: TDBAL: %x\n", viobase[0x3800>>2]);
	xiv::printf("e1000: TDBAH: %x\n", viobase[0x3804>>2]);
	xiv::printf("e1000: TDLEN: %x\n", viobase[0x3808>>2]);
	xiv::printf("e1000: TDH: %x\n", viobase[0x3810>>2]);
	xiv::printf("e1000: TDT: %x\n", viobase[0x3818>>2]);
}

e1000::~e1000() {
}

}

