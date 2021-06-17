/* ***
 * pci.cpp - PCI bus driver
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "pci.hpp"
#include "drivers.hpp"
#include "kio.hpp"
#include "ktext.hpp"

namespace pci {

PCI::PCI() {
}
PCI::~PCI() {
}

uint32_t verblevel = 0;

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
uint32_t readl(uint32_t pcia, uint8_t o) {
	uint32_t dva = (pcia & 0xffffff00) | (o & ~3);
	_ix_outl(0x0cf8, dva);
	return _ix_inl(0xcfc);
}
void writel(uint32_t val, uint8_t b, uint8_t d, uint8_t f, uint8_t o) {
	uint32_t dva = (1 << 31) | (b << 16) | (d << 11) | (f << 8) | (o & ~3);
	_ix_outl(0x0cf8, dva);
	_ix_outl(0xcfc, val);
}
void writel(uint32_t val, uint32_t pcia, uint8_t o) {
	uint32_t dva = (pcia & 0xffffff00) | (o & ~3);
	_ix_outl(0x0cf8, dva);
	_ix_outl(0xcfc, val);
}
void PCIBlock::writecommand(uint16_t c) {
	writel(c, this->pciaddr, 0x4);
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
	Multiword info, cmst, dcl, hci, ex, iir;
	PCIBlock cdb;
	cdb.pciaddr = (1 << 31) | (bus << 16) | (dev << 11) | (fn << 8);
	info.w = readl(cdb.pciaddr, 0);
	cmst.w = readl(cdb.pciaddr, 0x4);
	dcl.w = readl(cdb.pciaddr, 0x8);
	hci.w = readl(bus, dev, 0, 0xC);
	iir.w = readl(cdb.pciaddr, 0x3C);
	cdb.info.vendor = info.h[0];
	cdb.info.device = info.h[1];
	cdb.info.command = cmst.h[0];
	cdb.info.status = cmst.h[1];
	cdb.info.revision = dcl.b[0];
	cdb.info.progif = dcl.b[1];
	cdb.info.subclass = dcl.b[2];
	cdb.info.devclass = dcl.b[3];
	cdb.info.int_line = iir.b[0];
	cdb.info.int_pin = iir.b[1];
	cdb.info.min_grant = iir.b[2];
	cdb.info.max_latency = iir.b[3];
	if(verblevel >= 2) {
		xiv::printf("PCI %d/%d/%d - %x:%x ", bus, dev, fn, info.h[0], info.h[1]);
		if(hci.b[2] & 0x80) xiv::print("mf-");
	}
	if((hci.b[2] & 0x7f) == 0x01 && dcl.h[1] == 0x0604) {
		ex.w = readl(cdb.pciaddr, 0x18);
		if(verblevel >= 2) xiv::printf("bus: %x **nxt: %d\n", ex.w, ex.b[1]);
		dev_dump(ex.b[1]);
	} else if((hci.b[2] & 0x7f) == 0x00) {
		if(verblevel >= 2) {
			xiv::printf("dev: %x [%x] %x - ", hci.b[2], dcl.h[1], cdb.info.command);
			xiv::print(get_class(dcl.b[3]));
			xiv::print("  **");
		}
		for(int x = 0; x < 6; x++) {
			scanbar(bus, dev, fn, x, cdb.bar[x], cdb.barsz[x]);
			if(verblevel >= 2) {
				xiv::printhex(cdb.bar[x]);
				xiv::putc(':');
			}
		}
		if(verblevel >= 2) xiv::putc(10);
		instance_pci(cdb);
	} else {
		if(verblevel >= 2) {
			xiv::printf("other: %x [%x] - ", hci.b[2], dcl.h[1]);
			xiv::print(get_class(dcl.b[3]));
			xiv::putc(10);
		}
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

