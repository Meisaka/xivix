/* ***
 * 
 * Copyright (c) 2014-2022  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */
#include "cmos.hpp"
#include "kio.hpp"
#include "ktext.hpp"
#include "interrupt.hpp"

namespace hw {

CMOS CMOS::instance;

enum CMOS_REG : uint8_t {
	RTC_SECOND = 0,
	RTC_SECOND_ALARM = 1,
	RTC_MINUTE = 2,
	RTC_MINUTE_ALARM = 3,
	RTC_HOUR = 4,
	RTC_HOUR_ALARM = 5,
	RTC_WEEKDAY = 6,
	RTC_DAYOFMONTH = 7,
	RTC_MONTH = 8,
	RTC_YEAR = 9,
	RTC_STATUS_A = 0xa,
	RTC_STATUS_B = 0xb,
	RTC_STATUS_C = 0xc,
	RTC_STATUS_D = 0xd,
};

#define RTCA_DIV(a) (((a) & 7) << 4)
#define RTCA_RATE(a) ((a) & 15)
constexpr uint8_t RTCA_UIP = (1<<7);

#define BCD_TO_BIN(a) a = (a & 0xf) + ((a >> 4) * 10)

enum RTC_B_FLAGS {
	RTCB_DST_EN = 0, // this bit should be cleared
	RTCB_24H = (1<<1), // 0=12 or 1=24 hour time (RO)
	RTCB_BIN = (1<<2), // 0=BCD or 1=bin  (RO)
	RTCB_SQW_INT = (1<<3), // square wave interrupts
	RTCB_UE_INT = (1<<4), // update ended interrupts
	RTCB_ALARM_INT = (1<<5), // alarm interrupts
	RTCB_PERIODIC_INT = (1<<6), // interrupts at (rate and div)
	RTCB_SET = (1<<7), // RTC will not update when set
};
constexpr uint8_t RTCD_BATT_SENSE = 0x80;

static void cmos_irq(void *u, uint32_t, ixintrctx*) {
	if(u) ((CMOS*)u)->irq();
}

void CMOS::init_configure(uint16_t base_addr, uint8_t irq, uint8_t cent) {
	io_base = base_addr;
	this->century_reg = cent;
	ivix_interrupt[irq].rlocal = this;
	ivix_interrupt[irq].entry = cmos_irq;
	ivix_interrupt[32 + irq].rlocal = this;
	ivix_interrupt[32 + irq].entry = cmos_irq;
}

void CMOS::add_second() {
	if(++second < 60) return;
	second = 0;
	if(++minute < 60) return;
	minute = 0;
	if(++hour < 24) return;
	hour = 0;
	if(++weekday > 7) { // don't really care about this
		weekday = 1;
	}
	monthday++;
	uint32_t month_flag = 1 << (month);
	uint32_t days_in_month = 30 + (0b101011010101 & month_flag);
	if(month == 2) {
		uint8_t leap = ((year & 3) == 0) ? 1 : 0;
		uint32_t mod_year = (year % 100);
		uint32_t div_year = (year / 100);
		if(mod_year == 0 && !(div_year & 3)) {
			leap = 0;
		}
		days_in_month -= 2 - leap;
	}
	if(monthday <= days_in_month) return;
	monthday = 1;
	if(++month <= 12) return;
	month = 1;
	++year;
}
void CMOS::irq() {
	uint8_t c = read(RTC_STATUS_C);
	if(c & RTCB_PERIODIC_INT) {
		if(++itimer >= 0x400) {
			itimer = 0;
		}
	}
	if(c & RTCB_UE_INT) {
		timer++;
		add_second();
	}
}
void CMOS::write(uint8_t reg, uint8_t value) {
	reg = (reg & 0x7f) | nmi_flag;
	_ix_outb(io_base, reg);
	_ix_outb(io_base + 1, value);
}
uint8_t CMOS::read(uint8_t reg) {
	reg = (reg & 0x7f) | nmi_flag;
	_ix_outb(io_base, reg);
	return _ix_inb(io_base + 1);
}
bool CMOS::init() {
	nmi_flag = 0x80; // disable
	xiv::printf("CMOS setup\n");
	timer = 0;
	write(RTC_STATUS_A, RTCA_DIV(2) | RTCA_RATE(6));
	uint8_t b_reg = read(RTC_STATUS_B);
	b_reg |= RTCB_PERIODIC_INT;
	b_reg |= RTCB_UE_INT;
	//b_reg |= RTCB_SQW_INT;
	write(RTC_STATUS_B, b_reg); // enable RTC interrupt
	read(RTC_STATUS_C);
	uint32_t timeout = _ivix_int_n + 1200;
	bool update_ok = false;
	for(uint32_t i = 0; i < 0x100000 && _ivix_int_n < timeout; i++) { // wait until Update
		if(read(RTC_STATUS_A) & RTCA_UIP) { update_ok = true; break; }
		asm("pause");
	}
	// wait for update to finish
	timeout = _ivix_int_n + 500;
	for(uint32_t i = 0; (read(RTC_STATUS_A) & RTCA_UIP) && i < 0x100000 && _ivix_int_n < timeout; i++) {
		asm("pause");
	}
	uint16_t century;
	second = read(RTC_SECOND);
	minute = read(RTC_MINUTE);
	hour = read(RTC_HOUR);
	weekday = read(RTC_WEEKDAY);
	monthday = read(RTC_DAYOFMONTH);
	month = read(RTC_MONTH);
	year = read(RTC_YEAR);
	century = read(century_reg);
	if(!(b_reg & RTCB_24H) && (hour & 0x80)) {
		hour = hour & 0x7f;
		if(b_reg & RTCB_BIN) {
			hour += 12;
		} else {
			hour += 0x12;
		}
	}
	if(!(b_reg & RTCB_BIN)) {
		BCD_TO_BIN(second);
		BCD_TO_BIN(minute);
		BCD_TO_BIN(hour);
		BCD_TO_BIN(monthday);
		BCD_TO_BIN(month);
		BCD_TO_BIN(year);
		BCD_TO_BIN(century);
	}
	if(century == 0) century = 20;
	year = (century * 100) + year;
	//uint32_t day_of_year = month;
	xiv::printf("RTC: %02d:%02d:%02d %d d%d m%d y%04d\n",
		hour, minute, second, weekday, monthday, month, year);
	xiv::printf("RTC B=%02x read %s\n", b_reg, update_ok ? "ok" : "bad");
	return true;
}
void CMOS::remove() {}

} // namespace hw

