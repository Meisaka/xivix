/* ***
 * memory.hpp - functions to map, allocate, and free memory and pages
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

#ifndef MEMORY_HAI
#define MEMORY_HAI

#include "ktypes.hpp"

namespace mem {

struct PhysicalAddress {
	uint64_t m;
	PhysicalAddress(uint64_t a) : m(a) {}
	PhysicalAddress(PhysicalAddress &&p) : m(p.m) {}
	PhysicalAddress(const PhysicalAddress &p) : m(p.m) {}
	PhysicalAddress& operator=(PhysicalAddress &&p) { m = p.m; return *this; }
	PhysicalAddress& operator=(const PhysicalAddress &p) { m = p.m; return *this; }
	PhysicalAddress& operator=(uint64_t a) { m = a; return *this; }
};

}

typedef mem::PhysicalAddress phyaddr_t;

namespace mem {

	enum MAP_FLAGS : uint32_t {
		MAP_RW = 0x2,
		MAP_USER = 0x4,
		MAP_EXEC = 0x8,
		MAP_LARGE = 0x80,
	};

	void initialize();
	void debug();
	void map_page(phyaddr_t, uintptr_t, uint32_t);
	void * alloc_pages(size_t, uint32_t);
	void * request(size_t, void*, uint64_t, uint32_t);
}

extern "C" {
void * kmalloc(size_t l);
void * krealloc(void *, size_t);
void kfree(void *);
}

#endif

