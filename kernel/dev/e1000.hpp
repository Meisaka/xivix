/* ***
 * e1000.hpp - e1000: aka intel 8257x driver
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
#ifndef DEV_E1000_HAI
#define DEV_E1000_HAI

#include "hwtypes.hpp"
#include "pci.hpp"

namespace hw {

class e1000 final : public Hardware, public NetworkMAC {
private:
	volatile uint32_t *viobase;
	uint8_t macaddr[6];
	void * descbase;
	uint32_t rxlimit;
	uint32_t txlimit;
	uint32_t rxtail;
	uint32_t txtail;
	uint32_t rxheadp;
	uint32_t txheadp;
	uint32_t lastint;
private:
	uint16_t readeeprom(uint16_t);
public:
	e1000(pci::PCIBlock &);
	~e1000();
	bool init() override;
	void remove() override;
	void transmit(void *, size_t) override;
	void addreceive(void *, size_t) override;
	void getmediaaddr(uint8_t *) const override;
	uint32_t handle_int();
};

}

#endif

