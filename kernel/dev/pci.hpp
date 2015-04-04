/* ***
 * pci.hpp - class decl for PCI bus driver
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

#ifndef PCI_HAI
#define PCI_HAI

#include "ktypes.hpp"

namespace pci {

uint16_t readw(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint32_t readl(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint32_t readl(uint32_t pcia, uint8_t offset);
void dev_dump(uint8_t);
void bus_dump();

struct PCIInfo {
	uint16_t vendor;
	uint16_t device;
	uint16_t command;
	uint16_t status;
	uint8_t revision;
	uint8_t progif;
	uint8_t subclass;
	uint8_t devclass;
	uint16_t subsys_vendor;
	uint16_t subsys_device;
	uint8_t int_line;
	uint8_t int_pin;
	uint8_t min_grant;
	uint8_t max_latency;
};

struct PCIBlock {
	uint32_t pciaddr;
	uint32_t bar[6];
	uint32_t barsz[6];
	PCIInfo info;
	void writecommand(uint16_t);
};

class PCI final {
public:
	PCI();
	~PCI();
};

} // ns pci

#endif

