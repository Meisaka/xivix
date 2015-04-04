/* ***
 * i686/tables.cpp - GDT and IDT functions and tables for x86
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

#include <stdint.h>
#include "interrupt.hpp"

#pragma pack(push, 1)
struct GDTEntry {
	uint16_t limit_lo;
	uint16_t base_lo;
	uint8_t base_16;
	uint8_t type : 4;
	uint8_t system : 1;
	uint8_t priv : 2;
	uint8_t present : 1;
	uint8_t limit_16 : 4;
	uint8_t flags : 4;
	uint8_t base_24;
};

enum GDTFlags {
	TYPE_SYSTEM = 0,
	TYPE_CODE = 1,
	PRIV_SYSTEM = 0,
	PRIV_USER = 0x6,
	PRESENT = 0x8,
	AVAILABLE = 0x10,
	MODE64 = 0x20,
	SEG16 = 0,
	SEG32 = 0x40,
	GRAN_BYTE = 0,
	GRAN_4K = 0x80,
};

enum GDTTypes {
	/* code and data types (bit flags) */
	T_DATA = 0,
	T_CODE = 8,
	T_EXPAND_DOWN = 4,
	T_READWRITE = 2,
	T_READONLY = 0,
	T_EXECONLY = 0,
	T_EXECREAD = 2,
	T_ACCESSED = 1,
	T_CONFORMING = 4,
	/* system types */
	T_TSS16A = 1,
	T_LDT = 2,
	T_TSS16B = 3,
	T_CALL16 = 4,
	T_TASK = 5,
	T_INT16 = 6,
	T_TRAP16 = 7,
	T_TSS32A = 9,
	T_TSS32B = 11,
	T_CALL32 = 12,
	T_INT32 = 14,
	T_TRAP32 = 15
};

constexpr GDTEntry MkGDT(uint32_t base, uint32_t lim, uint8_t t, uint8_t f) {
	return GDTEntry {
	(uint16_t)(lim & 0xffff),
	(uint16_t)(base & 0xffff),
	(uint8_t)(base >> 16),
	t,
	(uint8_t)(f & 1),
	(uint8_t)(f >> 1),
	(uint8_t)(f >> 3),
	(uint8_t)(lim >> 16),
	(uint8_t)(f >> 4),
	(uint8_t)(base >> 24)
	};
}

struct GDTPtr {
	uint16_t len;
	GDTEntry *ptr;
};

struct IDTEntry {
	uint16_t ofs_lo;
	uint16_t selector;
	uint16_t flags : 13;
	uint16_t priv : 2;
	uint16_t present : 1;
	uint16_t ofs_hi;
};

enum PRIVLVL {
	RING_0 = 0,
	RING_1 = 1,
	RING_2 = 2,
	RING_3 = 3,
};

IDTEntry IDTTrap(uintptr_t base, uint16_t seg, int p) {
	return IDTEntry {
		(uint16_t)(((uint32_t)base) & 0xffff),
		seg,
		(uint16_t)(0x700),
		(uint16_t)(p),
		1,
		(uint16_t)(((uint32_t)base) >> 16)
	};
}
IDTEntry IDTInt(uintptr_t base, uint16_t seg, int p) {
	return IDTEntry {
		(uint16_t)(((uint32_t)base) & 0xffff),
		seg,
		(uint16_t)(0x600),
		(uint16_t)(p),
		1,
		(uint16_t)(((uint32_t)base) >> 16)
	};
}
IDTEntry IDTTask(uint16_t seg, int p) {
	return IDTEntry {
		0,
		seg,
		(uint16_t)(0x500),
		(uint16_t)(p),
		1,
		0
	};
}

struct IDTPtr {
	uint16_t len;
	IDTEntry *ptr;
};

#pragma pack(pop)

extern "C" {
	IntrDef xiv_intrpt[32];
	ExceptDef xiv_except[32];
};

extern "C" {
struct GDTEntry gdt_data[256] = {
	MkGDT(0, 0, 0, 0),
	MkGDT(0, 0xffffff, T_DATA | T_READWRITE, TYPE_CODE | PRESENT | PRIV_SYSTEM | SEG32 | GRAN_4K),
	MkGDT(0, 0xffffff, T_CODE | T_EXECREAD,  TYPE_CODE | PRESENT | PRIV_SYSTEM | SEG32 | GRAN_4K),
};

}

