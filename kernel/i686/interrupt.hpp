/* ***
 * interrupt.hpp - Interrupt and Exception structs for x86
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
#ifndef I686_INTERRUPT_HAI
#define I686_INTERRUPT_HAI
#include <stdint.h>

#pragma pack(push, 1)

struct ixintrctx {
	uint32_t r_edi;
	uint32_t r_esi;
	uint32_t r_ebp;
	uint32_t r_esp_hdl;
	uint32_t r_ebx;
	uint32_t r_edx;
	uint32_t r_ecx;
	uint32_t r_eax;
	uint32_t r_eip;
	uint32_t r_cs;
	uint32_t r_eflag;
};
struct ixexptctx {
};

typedef void (*InterruptHandle)(void *, uint32_t, ixintrctx *);
typedef void (*ExceptionHandle)(void *, uint32_t, ixexptctx *);

struct IntrDef {
	InterruptHandle entry;
	void *rlocal;
};
struct ExceptDef {
	ExceptionHandle entry;
	void *rlocal;
};

#pragma pack(pop)

extern "C" IntrDef ivix_interrupt[32];
extern "C" ExceptDef ivix_except[32];

#endif

