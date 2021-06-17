/* ***
 * drivers.hpp - driver instancing
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */
#ifndef DEV_DRIVERS_HAI
#define DEV_DRIVERS_HAI

#include "hwtypes.hpp"
#include "pci.hpp"

namespace hw {
void init();
}
namespace pci {
void instance_pci(PCIBlock &);
}

#endif

