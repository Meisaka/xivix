#ifndef KIO_HAI
#define KIO_HAI

#include "ktypes.hpp"

extern "C" {
	void _ix_halt();
	void _ix_totalhalt();
	void _ix_req();
	void _ix_reqr();
	extern volatile uint32_t _ivix_int_n;
	extern uint8_t _ix_inb(uint16_t a);
	uint16_t _ix_inw(uint16_t a);
	uint32_t _ix_inl(uint16_t a);
	extern void _ix_outb(uint16_t a, uint8_t v);
	void _ix_outw(uint16_t a, uint16_t v);
	void _ix_outl(uint16_t a, uint32_t v);
}

#endif

