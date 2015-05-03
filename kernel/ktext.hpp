/* ***
 * ktext.hpp - declarations for text output
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

#ifndef KTEXT_HAI
#define KTEXT_HAI

#include "ktypes.hpp"

namespace xiv {
void putc(char);
void printn(const char *, size_t);
void print(const char *);
void printdec(uint32_t d);
void printhex(uint32_t v);
void printhex(uint64_t v);
void printhex(uint32_t v, uint32_t bits);
void printhexx(uint64_t v, uint32_t bits);
void printf(const char *ftr, ...);
void printattr(uint32_t x);
} // ns xiv
#endif

