/* ***
 * drivers.cpp - kernel driver instancing
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
#include "hwtypes.hpp"
#include "ktext.hpp"
#include "pci.hpp"

#include "e1000.hpp"

namespace hw {
void init() {
}

}

namespace pci {

void instance_pci(PCIBlock &dv) {
	switch(dv.info.vendor) {
	case 0x8086:
		switch(dv.info.device) {
		case 0x10D3:
			new hw::e1000(dv);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

}

