/* ***
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */
#ifndef CMOS_HAI
#define CMOS_HAI

#include "ktypes.hpp"
#include "hwtypes.hpp"

namespace hw {

class CMOS: public Hardware {
protected:
	uint16_t io_base;
	uint8_t century_reg;
	uint8_t nmi_flag;

	void write(uint8_t reg, uint8_t value);
	uint8_t read(uint8_t reg);
	void add_second();
public:
	uint32_t itimer;
	uint32_t timer;
	uint8_t hour, minute, second, weekday, monthday, month;
	uint16_t year;
	void irq(); // called internally
	static CMOS instance;
	void init_configure(uint16_t base_addr, uint8_t irq, uint8_t century_reg);
	bool init() override;
	void remove() override;
};

} // namespace hw

#endif

