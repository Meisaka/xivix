/* ***
 * memory.cpp - implementation of memory management functions
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

#include "memory.hpp"
#include "ktext.hpp"

namespace mem {
	struct PageDirPtr {
		uint64_t pdtpe[4];
	};
	struct PageDir {
		uint64_t pde[512];
	};
	struct PageTable {
		uint64_t pte[512];
	};
	struct MemExtent {
		uint32_t flags;
		union {
			size_t base;
			MemExtent *next;
		};
		size_t length;
	};

	struct MemExtent *kmem;

	extern "C" {
	extern uint8_t _ivix_phy_pdpt;
	extern uint32_t const _ivix_pdpt;
	extern char _kernel_start;
	extern char _kernel_end;
	extern char _kernel_load;
	}

	PageDirPtr *pdpt;
	PageDir *pdirs;
	char *em = reinterpret_cast<char*>(&_kernel_end + 0x10000);

	void initialize() {
		xiv::printf("Starting memory manager.\n");
		size_t const phyptr = ((&_kernel_start) - (&_kernel_load));
		pdpt = (PageDirPtr*)((&_ivix_phy_pdpt) + phyptr);
		xiv::printf("Kernel %x - %x (%x)\n", &_kernel_start, &_kernel_load, phyptr);
		xiv::printf("PDP: %x\n", (size_t)pdpt);
		xiv::printf("Heap start: %x\n", (size_t)&_kernel_end);
		pdirs = (PageDir*)&_kernel_end;

		for(int i = 0; i < 512; i++) {
			pdirs[3].pde[i] = pdirs[0].pde[i];
		}
		pdpt->pdtpe[3] = ( ((uint32_t)pdirs+3) - phyptr ) | 1;
		pdpt->pdtpe[0] = ( ((uint32_t)pdirs  ) - phyptr ) | 1;
		pdpt->pdtpe[1] = ( ((uint32_t)pdirs+1) - phyptr ) | 1;
		pdpt->pdtpe[2] = ( ((uint32_t)pdirs+2) - phyptr ) | 1;
		xiv::printf("Page directories mapped\n");
		for(int i = 0; i < 512; i++) {
			//pdirs[0].pde[i] = 0;
			pdirs[1].pde[i] = 0;
			pdirs[2].pde[i] = 0;
		}
		xiv::printf("Page directories reset\n");
	}

	void map_page(phyaddr_t phy, uintptr_t t, uint32_t f) {
		uint32_t ofs_dp = (t >> 30) & 0x3;
		uint32_t ofs_d = (t >> 21) & 0x1FF;
		uint32_t ofs_t = (t >> 12) & 0x1F;
		size_t const phyptr = ((&_kernel_start) - (&_kernel_load));
		uint32_t dirbase = ((uint32_t)pdpt->pdtpe[ofs_dp]);
		dirbase &= 0xfffff000;
		PageDir *dirptr = (PageDir*)(dirbase + phyptr);
		if(f & MAP_LARGE) {
			dirptr->pde[ofs_d] = phy.m | 1 | (f & 0x9f);
			xiv::printf("map_page %x = (%x)\n", t, dirptr->pde[ofs_d]);
		} else {
			//uint32_t tablebase = ((uint32_t)dirptr->pde[ofs_d]);
			// TODO if(tablebase) not present or present large page
			//tablebase &= 0xfffff000;
			//PageTable *ptptr = (PageTable*)(_kernel_start + tablebase);
			//ptptr->pte[ofs_t] = phy.m | 1 | (f & 0x1f);
		}
	}
	void * alloc_pages(size_t, uint32_t) {
		return nullptr;
	}
	void * request(size_t, void*, uint64_t, uint32_t) {
		return nullptr;
	}
}

extern "C"
void * kmalloc(size_t l) {
	if(l > 1) {
		void *pa = mem::em;
		if(l & 0xf) {
			l += 16 - (l & 0xf);
		}
		mem::em = mem::em + l;
		return pa;
	}
	return nullptr;
}

extern "C"
void * krealloc(void * p, size_t l) {
	return nullptr;
}

extern "C"
void kfree(void *) {
}

