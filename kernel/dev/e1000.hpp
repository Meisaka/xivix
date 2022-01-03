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

struct RingPointer {
	uint32_t tail;
	uint32_t head;
	uint32_t limit;
	uint32_t next() {
		uint32_t c = tail;
		uint32_t n = c + 1;
		if(n >= limit) n = 0;
		tail = n;
		return c;
	}
	bool full() const {
		uint32_t n = tail + 1;
		if(n >= limit) n = 0;
		return (n == head);
	}
	bool empty() const {
		return tail == head;
	}
	uint32_t pop() {
		uint32_t c = head;
		uint32_t n = c + 1;
		if(n >= limit) n = 0;
		head = n;
		return n;
	}
};

struct RegisterBlock {
	volatile uint8_t *base;
	volatile uint32_t & operator[](size_t index) {
		return *(volatile uint32_t*)(base + index);
	}
	uint32_t operator[](size_t index) const {
		return *(volatile uint32_t*)(base + index);
	}
};


class e1000 final : public NetworkMAC {
private:
	struct DESC_PAGE;

	RegisterBlock vio;
	uint8_t macaddr[6];
	DESC_PAGE * descpage;
	RingPointer rxq;
	RingPointer txq;
	uint32_t lastint;
	uint32_t junk_register;
	uint32_t last_junk;
	uint32_t last_itime;
private:
	uint16_t readeeprom(uint16_t);
public:
	e1000(pci::PCIBlock &);
	~e1000();
	bool init() override;
	void remove() override;
	void transmit(Ref<net::Packet>) override;
	void addreceive(Ref<net::Packet>) override;
	void getmediaaddr(uint8_t *) const override;
	void processqueues() override;
	void debug_junk() override;
	void debug_cmd(void *, uint32_t) override;
	uint32_t handle_int();
};

void e1000_start_task(e1000 *eth);

}

#endif

