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

namespace hw {

class PCI final {
public:
	PCI();
	~PCI();

	static uint16_t readw(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
	static uint32_t readl(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
	void dev_dump(uint8_t);
	void bus_dump();
};

} // ns hw

#endif

