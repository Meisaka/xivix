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

struct RegFile {
	uint32_t r_edi;
	uint32_t r_esi;
	uint32_t r_ebp;
	uint32_t r_esp_hdl;
	uint32_t r_ebx;
	uint32_t r_edx;
	uint32_t r_ecx;
	uint32_t r_eax;
};

struct BaseRegisterSave {
	uint32_t r_edi;
	uint32_t r_esi;
	uint32_t r_ebp;
	uint32_t r_esp;
	uint32_t r_ebx;
	uint32_t r_edx;
	uint32_t r_ecx;
	uint32_t r_eax;
	uint32_t r_eip;
	uint32_t r_cs;
	uint32_t r_eflags;
};

struct IntHead {
	uint32_t r_eip;
	uint32_t r_cs;
	uint32_t r_eflag;
};

struct IntCtx {
	RegFile ir;
	uint32_t extra;
	union {
		uint32_t ecode;
		uint32_t interrupt_index;
	};
	IntHead ih;
};

typedef void (*InterruptHandle)(void *, uint32_t, IntCtx *);
typedef void (*ExceptionHandle)(void *, uint32_t, IntCtx *);

//__attribute__((noreturn))
void scheduler_from_interrupt(IntCtx *);

struct IntrDef {
	InterruptHandle entry;
	void *rlocal;
};
struct ExceptDef {
	ExceptionHandle entry;
	void *rlocal;
};

#pragma pack(pop)

extern "C" IntrDef iv_interrupt[66];
extern "C" ExceptDef iv_except[32];

#endif

