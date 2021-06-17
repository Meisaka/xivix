/* ***
 * e1000.hpp - e1000: aka intel 8257x driver
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */
#ifndef DEV_E1000_HAI
#define DEV_E1000_HAI

#include "hwtypes.hpp"
#include "pci.hpp"

namespace hw {

class e1000 final : public NetworkMAC {
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
	uint64_t rdbuf;
	uint64_t txbuf;
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

