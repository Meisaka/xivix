/* ***
 * drivers.hpp - driver instancing
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

