/* ***
 * drivers.cpp - kernel driver instancing
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "hwtypes.hpp"
#include "ktext.hpp"
#include "pci.hpp"

#include "e1000.hpp"

namespace hw {

NetworkMAC *ethdev;

void init() {
	ethdev = nullptr;
}

}

namespace pci {

void instance_pci(PCIBlock &dv) {
	switch(dv.info.vendor) {
	case 0x8086:
		switch(dv.info.device) {
		case 0x100F: // 82545EM
		case 0x100E: // 82540EM
		case 0x10D3: // 82574L
		case 0x104B: // ICH8/9
		case 0x10CD: // ICH10
		case 0x105E:
		case 0x10A4: // 82571EB (Experimental)
			{
				hw::e1000 *neth = new hw::e1000(dv);
				neth->init();
				e1000_start_task(neth);
				if(!hw::ethdev) hw::ethdev = neth;
			}
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

