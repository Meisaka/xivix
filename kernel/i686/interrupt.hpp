/* ***
 * interrupt.hpp - Interrupt and Exception structs for x86
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */
#ifndef I686_INTERRUPT_HAI
#define I686_INTERRUPT_HAI

#include <stdint.h>

#pragma pack(push, 1)

struct ixregfile {
	uint32_t r_edi;
	uint32_t r_esi;
	uint32_t r_ebp;
	uint32_t r_esp_hdl;
	uint32_t r_ebx;
	uint32_t r_edx;
	uint32_t r_ecx;
	uint32_t r_eax;
};

struct ixintrhead {
	uint32_t r_eip;
	uint32_t r_cs;
	uint32_t r_eflag;
};

struct ixintrctx {
	ixregfile r;
	ixintrhead ih;
};

struct ixexptctx {
	uint32_t ec_1;
	uint32_t ec_2;
	ixregfile *ir;
	ixintrhead *ih;
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

extern "C" IntrDef ivix_interrupt[66];
extern "C" ExceptDef ivix_except[32];

#endif

