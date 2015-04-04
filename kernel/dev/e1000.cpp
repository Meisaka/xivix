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

union HWR_CTRL {
	uint32_t value;
	struct {
		uint32_t fd : 1;
		uint32_t rsv0 : 1;
		uint32_t gio_dis : 1;
		uint32_t lrst : 1;
		uint32_t rsv1 : 2;
		uint32_t slu : 1;
		uint32_t rsv2 : 1;
		uint32_t speed : 2;
		uint32_t rsv3 : 1;
		uint32_t frcspd : 1;
		uint32_t frcdplx : 1;
		uint32_t rsv4 : 2;
		uint32_t rsv5 : 1;
		uint32_t rsv6 : 1;
		uint32_t rsv7 : 3;
		uint32_t sdp0_data : 1;
		uint32_t sdp1_data : 1;
		uint32_t advd3wuc : 1;
		uint32_t rsv8 : 1;
		uint32_t sdp0_iodir : 1;
		uint32_t sdp1_iodir : 1;
		uint32_t rsv9 : 2;
		uint32_t rst : 1;
		uint32_t rfce : 1;
		uint32_t tfce : 1;
		uint32_t rsv10 : 1;
		uint32_t vme : 1;
		uint32_t phy_rst : 1;
	};
};

union HWR_STATUS {
	uint32_t value;
	struct {
		uint32_t fd : 1;
		uint32_t lu : 1;
		uint32_t lanid : 2;
		uint32_t txoff : 1;
		uint32_t tbimode : 1;
		uint32_t speed : 2;
		uint32_t asdv : 2;
		uint32_t phyra : 1;
		uint32_t rsv0 : 1;
		uint32_t rsv1 : 7;
		uint32_t giomes : 1;
		uint32_t rsv2 : 12;
	};
	void status() {
		xiv::print("STATUS:");
		if(lu) xiv::print(" LU");
		xiv::printf(" LANID:%x", lanid);
		if(txoff) xiv::print(" TXOFF");
		if(tbimode) xiv::print(" TBIMODE");
		if(asdv) xiv::print(" ASDV");
		if(phyra) xiv::print(" PHYRA");
		xiv::printf(" SPEED:%x", 0x10 << (speed * 4));
		xiv::print(fd? " Full":" Half");
		if(giomes) xiv::print(" GIOMES");
		xiv::putc(10);
	}
};

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
	void status() {
		if(en) xiv::print("EN ");
		if(psp) xiv::print("PSP ");
		xiv::printf("CT=%d ", ct);
		xiv::printf("COLD=%d ", cold);
		if(swxoff) xiv::print("SWXOFF ");
		if(rtlc) xiv::print("RTLC ");
		if(nrtu) xiv::print("NRTU ");
		xiv::putc(10);
	}
};

#pragma pack(push, 1)
struct RECV_DESC {
	uint64_t address;
	uint16_t len;
	uint16_t checksum;
	uint8_t status;
	uint8_t errors;
	uint16_t vlan;
};
struct TRMT_CONTEXT {
	uint8_t ipcss;
	uint8_t ipcso;
	uint16_t ipcse;
	uint8_t tucss;
	uint8_t tucso;
	uint16_t tucse;
	uint32_t len : 20;
	uint32_t dtype : 4;
	uint32_t tucmd : 8;
	uint8_t sta : 4;
	uint8_t rsv : 4;
	uint8_t hdrlen;
	uint16_t mss;
};
struct TRMT_DATA {
	uint64_t address;
	uint32_t len : 20;
	uint32_t dtype : 4;
	uint32_t dcmd : 8;
	uint8_t sta : 4;
	uint8_t rsv : 4;
	uint8_t popts;
	uint16_t vlan;
};
struct DESC_SLOT {
	uint64_t v[2];
};
struct DESC_PAGE {
	DESC_SLOT recv[128];
	DESC_SLOT trmt[128];
};
#pragma pack(pop)

e1000::e1000(pci::PCIBlock &pcib) {
	xiv::print("e1000: init...\n");
	xiv::printf("e1000: base: %x / %x\n", pcib.bar[0], pcib.barsz[0]);
	pcib.writecommand(0x117); // enable mastering on PCI

	// setup the ring buffer pages
	uint64_t piobase = (pcib.bar[0] & 0xfffffff0);
	void * vmbase = (void*)(pcib.bar[0] & 0xfffffff0);
	viobase = (volatile uint32_t *)vmbase;
	mem::request(pcib.barsz[0], vmbase, piobase, mem::RQ_HINT | mem::RQ_RW);
	descbase = mem::alloc_pages(1, 0);
	DESC_PAGE *descpage = (DESC_PAGE*)descbase;
	uint64_t rdbuf = mem::translate_page((uint32_t)descpage->recv);
	uint64_t txbuf = mem::translate_page((uint32_t)descpage->trmt);

	// Device Control
	HWR_CTRL r_ctrl = { .value = 0 };
	viobase[0xd0>>2] = 0; // interrupts off
	r_ctrl.rst = 1;
	r_ctrl.slu = 1;
	r_ctrl.phy_rst = 1;
	viobase[0] = r_ctrl.value; // reset everything
	xiv::print("e1000: reset\n");
	while(viobase[0] & (1 << 31));
	r_ctrl.value = viobase[0];
	r_ctrl.slu = 1;
	viobase[0] = r_ctrl.value; // make sure link up is set
	HWR_STATUS dsr = { .value = viobase[8>>2] };
	while((viobase[8>>2] & (1 << 1)) == 0); // wait link up, TODO not forever!!!1!
	dsr.value = viobase[8>>2];
	dsr.status();
	xiv::printf("e1000: ICR: %x\n", viobase[0xc0>>2]);
	viobase[0xd0>>2] = 0; // turn off interrupts again.
	r_ctrl.value = viobase[0];
	xiv::printf("e1000: CTRL: %x\n", r_ctrl.value);

	// MAC address
	// From EEPROM:
	//for(int x = 0; x < 3; x++) ((uint16_t*)macaddr)[x] = readeeprom(x);
	// From Register:
	((uint32_t*)macaddr)[0] = viobase[0x5400>>2];
	((uint16_t*)macaddr)[2] = (uint16_t)viobase[0x5404>>2];
	xiv::printf("e1000: MAC: %x", macaddr[0]);
	for(int x = 1; x < 6; x++) xiv::printf(":%x", macaddr[x]);
	xiv::putc(10);

	// Recieve Control
	viobase[0x100>>2] = 0x1801a; // some magic to enable receives
	viobase[0x2800>>2] = (uint32_t)rdbuf; // setup the ring buffer
	viobase[0x2804>>2] = (uint32_t)(rdbuf >> 32);
	rxlimit = sizeof(DESC_PAGE::recv);
	viobase[0x2808>>2] = rxlimit;
	rxlimit >>= 4;
	rxtail = viobase[0x2818>>2];
	// Transmit Control
	xiv::printf("e1000: TCTL: ");
	HWR_TCTL tctl;
	tctl.value = viobase[0x400>>2];
	tctl.status();
	tctl.en = 0;
	viobase[0x400>>2] = tctl.value; // make sure transmit is disabled.
	viobase[0x3810>>2] = 0; // setup the descriptor ring buffer
	viobase[0x3818>>2] = 0;
	viobase[0x3800>>2] = (uint32_t)txbuf;
	viobase[0x3804>>2] = (uint32_t)(txbuf>>32);
	txlimit = sizeof(DESC_PAGE::trmt);
	viobase[0x3808>>2] = txlimit;
	txlimit >>= 4;
	txtail = viobase[0x3818>>2];
	tctl.en = 1;
	viobase[0x400>>2] = tctl.value; // enable transmitter again
	tctl.value = viobase[0x400>>2];
	tctl.status();
	xiv::printf("e1000: ICR: %x\n", viobase[0xc0>>2]);
	//viobase[0xd0>>2] = 0x02c7;
}

e1000::~e1000() {
}
bool e1000::init() {
	return true;
}
void e1000::remove() {
}
void e1000::transmit(void * buf, size_t sz) {
	if(sz < 64) {
		xiv::print("e1000: TX: Frame too small\n");
		return;
	}
	DESC_PAGE *descpage = (DESC_PAGE*)descbase;
	TRMT_DATA *txd = (TRMT_DATA*)&descpage->trmt[txtail];
	descpage->trmt[txtail].v[1] = 0;
	txd->address = mem::translate_page((uintptr_t)buf);
	txd->len = sz;
	txd->dtype = 0x1;
	txd->dcmd = 0x23;
	++txtail;
	if(txtail >= txlimit) txtail = 0;
	viobase[0x3818>>2] = txtail;
	uint32_t icr = viobase[0xc0>>2];
	if(icr) xiv::printf("e1000: ICR: %x\n", icr);
}

}

