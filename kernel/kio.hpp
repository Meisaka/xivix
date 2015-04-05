/* ***
 * kio.hpp - declarations for asm functions
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

	uint32_t* _ixa_inc(uint32_t *a);
	uint32_t* _ixa_dec(uint32_t *a);
	uint32_t _ixa_xchg(uint32_t *a, uint32_t n);
	void _ixa_or(uint32_t *a, uint32_t n);
	void _ixa_xor(uint32_t *a, uint32_t n);
	uint32_t _ixa_cmpxchg(uint32_t *a, uint32_t c, uint32_t n);
}

#endif

