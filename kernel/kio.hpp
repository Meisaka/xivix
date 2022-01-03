/* ***
 * kio.hpp - declarations for asm functions
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#ifndef KIO_HAI
#define KIO_HAI

#include "ktypes.hpp"

namespace Crypto {
void Init();
void Mix(const uint32_t *ent, size_t count);
}

struct TaskBlock;

extern "C" {

void _ix_halt();
void _ix_totalhalt();
void _ix_req();
void _ix_reqr();
void _ix_task_switch(TaskBlock *from, TaskBlock *to);
struct IntCtx;
__attribute__((noreturn))
void _ix_task_switch_from_int(IntCtx *, TaskBlock *save, TaskBlock *to);
extern volatile uint32_t _iv_int_n;
extern volatile uint32_t _iv_int_f;
extern volatile uint32_t _iv_int_ac;
extern uint8_t _ix_inb(uint16_t a);
uint16_t _ix_inw(uint16_t a);
uint32_t _ix_inl(uint16_t a);
extern void _ix_outb(uint16_t a, uint8_t v);
void _ix_outw(uint16_t a, uint16_t v);
void _ix_outl(uint16_t a, uint32_t v);
__attribute__((fastcall))
uint16_t _ix_ldnw(const uint8_t *);
__attribute__((regparm(1)))
__attribute__((no_caller_saved_registers))
uint16_t _ix_bswapw(uint16_t v);
__attribute__((regparm(1)))
__attribute__((no_caller_saved_registers))
uint32_t _ix_bswapi(uint32_t v);
void ix_com_putc(int ch);
uint32_t* _ixa_inc(uint32_t *a);
uint32_t* _ixa_dec(uint32_t *a);
uint32_t _ixa_xchg(uint32_t *a, uint32_t n);
void _ixa_or(uint32_t *a, uint32_t n);
void _ixa_xor(uint32_t *a, uint32_t n);
// atomic: if *a == c then *a = n return true else *e = *a return false
bool _ixa_cmpxchg(uint32_t *a, uint32_t c, uint32_t n);
bool _ixa_xcmpxchg(uint32_t *a, uint32_t c, uint32_t n, uint32_t *e);
uint32_t _ix_eflags();
void _ix_sti();
void _ix_cli();

uint32_t ixa4random();
} // extern C

#endif
