/* ***
 * ktext.hpp - declarations for text output
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#ifndef KTEXT_HAI
#define KTEXT_HAI

#include "ktypes.hpp"
#include "textio.hpp"
#include <stdarg.h>

namespace xiv {
void putc(char);
void iputc(TextIO *, char);
void printn(const char *, size_t);
void print(const char *);
//void printdec(uint32_t d);
//void printhex(uint32_t v);
//void printhex(uint64_t v);
//void printhex(uint32_t v, uint32_t bits);
//void printhexx(uint64_t v, uint32_t bits);
void iprintf(TextIO *out, const char *, ...);
void printf(const char *ftr, ...);
void ivprintf(TextIO *out, const char *, va_list v);
void printattr(uint32_t x);
void togglecomm();
} // ns xiv
#endif

