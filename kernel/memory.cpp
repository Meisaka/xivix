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
#include "kio.hpp"

extern "C" void _ix_loadcr3(uint32_t);

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
	struct MemMap {
		uint64_t start;
		uint64_t size;
		uint32_t type;
		uint32_t attrib;
	};
	struct MemExtent {
		uint32_t flags;
		union {
			uint64_t base;
			MemExtent *next;
		};
		size_t length;
	};

	enum MEMEXTENT : uint32_t {
		MX_UNSET,
		MX_EMPTY,
		MX_FREE,
		MX_FIXED,
		MX_CONTROL,
		MX_ALLOC,
		MX_IO,
		MX_LINK,
	};
	const char * const EXTENTCLASS[] = {
		"UNSET",
		"-----",
		"FREE ",
		"SYSTM",
		"CTRL ",
		"ALLOC",
		"I/O  ",
		"NLINK"
	};
	
	constexpr static int CONTROL_UNITS = 2;

	int req_control(void (*)(void *, void *, uint32_t), void *);
	int req_allocrange(void (*)(void *, uint64_t, uint32_t), void *, uint32_t);

	template<int ADDRSIZE, int UNIT, bool CULL = false, class T = MemExtent>
	class ExtentAllocator {
	public:
		T *pbase;
		T *pfree;
		T *plast;
		T *pcblock;
		constexpr static const uint32_t max_ext = ADDRSIZE / sizeof(T);
	public:
		ExtentAllocator() {}
		ExtentAllocator(const ExtentAllocator&) = delete;
		ExtentAllocator(ExtentAllocator&&) = delete;
		ExtentAllocator& operator=(const ExtentAllocator&) = delete;
		ExtentAllocator& operator=(ExtentAllocator&&) = delete;

		void init(void * ldaddr, uint32_t adl) {
			pbase = (T*)ldaddr;
			pbase->flags = MX_UNSET;
			pfree = pcblock = plast = pbase;
			T *p = plast;
			uint32_t c = (adl * ADDRSIZE) - sizeof(T);
			uint32_t sz = 0;
			while(sz < c) {
				p->flags = MX_EMPTY;
				p++;
				sz += sizeof(T);
			}
			p->flags = MX_UNSET;
			add_extent(0, adl, MX_CONTROL);
		}
		static void add_control_callback(void * t, void * cba, uint32_t cbl) {
			((ExtentAllocator<ADDRSIZE,UNIT,CULL,T>*)t)->add_control(cba, cbl);
		}
		static void add_memory_callback(void * t, uint64_t cba, uint32_t cbl) {
			((ExtentAllocator<ADDRSIZE,UNIT,CULL,T>*)t)->add_extent(cba, cbl, MX_FREE);
		}
		void add_control(void * cba, uint32_t cbl) {
			T *p = (T*)cba;
			uint32_t c = (cbl * ADDRSIZE) - sizeof(T);
			uint32_t sz = 0;
			while(sz < c) {
				p->flags = MX_EMPTY;
				p++;
				sz += sizeof(T);
			}
			sz += sizeof(T);
			p->flags = MX_UNSET;
			p = (T*)cba;
			p->flags = MX_CONTROL;
			p->length = cbl;
			p->base = 0;

			p = pcblock;
			while(p->flags != MX_UNSET) {
				if(p->flags == MX_LINK) {
					p = p->next;
				} else {
					p++;
				}
			}
			p->flags = MX_LINK;
			p->next = cba;
			p->length = sz;
		}
		void add_extent(uint64_t b, uint32_t l, uint32_t f) {
			if(l == 0 || f == 0) return;
			if(f == MX_FREE) {
				if(pfree->flags == MX_FREE) {
					// if we are adding memory on the end, cull it
					if(pfree->base + pfree->length * UNIT == b) {
						pfree->length += l;
						return;
					}
				}
				pfree = plast;
			} else if(f == MX_CONTROL) {
				pcblock = plast;
			}
			plast->base = b;
			plast->length = l;
			plast->flags = f;
			plast++;
		}
		T * find_extent(uint64_t b) {
			T *pt = pbase;
			while(pt->flags != MX_UNSET) {
				if(pt->flags != MX_CONTROL) {
					if(pt->base == b) return pt;
				}
				if(pt->flags == MX_LINK) {
					pt = pt->next;
				} else {
					pt++;
				}
			}
			return nullptr;
		}
		void find_free_extent() {
			T *pt = pbase;
			uint32_t lc = 0;
			while(pt->flags != MX_UNSET) {
				if(pt->flags == MX_FREE && pt->length >= lc) {
					pfree = pt;
					lc = pfree->length;
				}
				if(pt->flags == MX_LINK) {
					pt = pt->next;
				} else {
					pt++;
				}
			}
		}
		T * intersect_extent(uint64_t b, uint32_t l) {
			T *pt = pbase;
			while(pt->flags != MX_UNSET) {
				if(pt->flags != MX_CONTROL) {
					if(b >= pt->base && (b + l * UNIT) <= (pt->base + pt->length * UNIT)) {
						return pt;
					}
				}
				if(pt->flags == MX_LINK) {
					pt = pt->next;
				} else {
					pt++;
				}
			}
			return nullptr;
		}
		void mark_extent(uint64_t b, uint32_t l, uint32_t f) {
			T *p;
			p = intersect_extent(b, l);
			if(p) {
				if(b == p->base) {
					add_extent(b + UNIT * l, p->length - l, p->flags);
					p->length = l;
					p->flags = f;
					if(f == MX_CONTROL) {
						pcblock = p;
					}
				} else {
					uint64_t adru = l * UNIT;
					uint64_t adrm = (b - p->base) / UNIT;
					add_extent(b, l, f);
					add_extent(b + adru, p->length - (l + adrm), p->flags);
					p->length = adrm;
				}
			} else {
				add_extent(b, l, f);
			}
			if(pfree->flags != MX_FREE) find_free_extent();
		}
		uint64_t allocate(uint32_t l) {
			return allocate(l, MX_ALLOC);
		}
		uint64_t allocate(uint32_t l, uint32_t f) {
			if(pfree->flags != MX_FREE) {
				find_free_extent();
				if(pfree->flags != MX_FREE) {
					xiv::print("MMA: No FREE blocks\n");
					if(req_allocrange(add_memory_callback, this, l)) {
						xiv::print("MMA: Allocation fail\n");
						return 0;
					}
				}
			}
			if(pfree->length >= l) {
				uint64_t mm = pfree->base;
				mark_extent(mm, l, f);
				return mm;
			} else {
				if(req_allocrange(add_memory_callback, this, l) == 0) {
					uint64_t mm = pfree->base;
					mark_extent(mm, l, f);
					return mm;
				}
			}
			xiv::printf("MMA: Length Exceeded %d/%d\n", l, pfree->length);
			return 0;
		}
		void debug_table() {
			T *pt = pbase;
			uint32_t addrc = 0;
			uint32_t cbls = 0;
			bool lastempty = false;
			while(true) {
				if(pt->flags == MX_CONTROL) {
					cbls = max_ext * pt->length;
				}
				if(pt->flags != MX_EMPTY || !lastempty) {
				xiv::print("ALTE: ");
				xiv::printhexx(pt->base, 64);
				xiv::print(" f:");
				xiv::print(EXTENTCLASS[pt->flags]);
				xiv::printf(" l: %x ", pt->length);
				if(pt == pfree) {
					xiv::print("<FREE");
				}
				if(pt == pcblock) {
					xiv::print("<CTLB");
				}
				if(pt == pbase) {
					xiv::print("<BASE");
				}
				xiv::putc(10);
				lastempty = (pt->flags == MX_EMPTY);
				}
				if(pt->flags == MX_LINK) {
					xiv::printf("Block: %d/%d\n", addrc, cbls);
					addrc = 0;
					pt = pt->next;
				} else {
					addrc++;
					if(pt->flags == MX_UNSET) break;
					pt++;
				}
			}
			xiv::printf("Block: %d/%d\n", addrc, cbls);
		}
	};

	constexpr int const SM_PAGE_SIZE = 0x1000;

	ExtentAllocator<SM_PAGE_SIZE, 1> memalloc;
	ExtentAllocator<SM_PAGE_SIZE, SM_PAGE_SIZE> blkalloc;
	ExtentAllocator<SM_PAGE_SIZE, SM_PAGE_SIZE> vpalloc;

	extern "C" {
	extern uint8_t _ivix_phy_pdpt;
	extern uint32_t const _ivix_pdpt;
	extern char _kernel_start;
	extern char _kernel_end;
	extern char _kernel_load;
	}

	PageDirPtr *pdpt;
	PageDir *pdirs;
	size_t const phyptr = ((&_kernel_start) - (&_kernel_load));
	char *blockptr = reinterpret_cast<char*>(&_kernel_end);

	void load_memmap() {
		using namespace xiv;
		printf("loading memory map...\n");
		uint32_t limit = *((uint16_t*)(phyptr + 0x500));
		MemMap *mm = (MemMap*)(phyptr + 0x800);
		for(uint32_t i = 0; i < limit; i++) {
			if(mm->start >= 0x100000 && mm->type == 1) {
				if(mm->size & 0xfff) {
					print("odd block size\n");
				} else {
					printhex(mm->size >> 12);
					printf(" pages at ");
					printhex(mm->start);
					blkalloc.add_extent(mm->start, mm->size >> 12, MX_FREE);
					putc(10);
				}
			}
			mm++;
		}
		printf("done.\n");
	}

	void initialize() {
		xiv::printf("Starting memory manager.\n");
		pdpt = (PageDirPtr*)((&_ivix_phy_pdpt) + phyptr);
		xiv::printf("Kernel %x - %x (%x)\n", &_kernel_start, &_kernel_load, phyptr);
		xiv::printf("PDP: %x\n", (size_t)pdpt);
		xiv::printf("Heap start: %x\n", (size_t)&_kernel_end);
		pdirs = (PageDir*)blockptr;
		blockptr += 0x4000;
		blkalloc.init(blockptr, CONTROL_UNITS);
		load_memmap();
		blkalloc.mark_extent((size_t)(&_kernel_load), (size_t)(&_kernel_end - &_kernel_start) >> 12, MX_FIXED);
		blkalloc.allocate(4);
		blkalloc.allocate(CONTROL_UNITS);
		blkalloc.allocate(CONTROL_UNITS);
		blkalloc.allocate(CONTROL_UNITS);
		blockptr += 0x2000;
		memalloc.init(blockptr, CONTROL_UNITS);
		blockptr += 0x2000;
		vpalloc.init(blockptr, CONTROL_UNITS);
		vpalloc.add_extent(0xc0000000, 0xffff, MX_FREE);
		vpalloc.mark_extent(0xc0000000, 0x100, MX_FIXED);
		vpalloc.mark_extent((size_t)(&_kernel_start), (size_t)(&_kernel_end - &_kernel_start) >> 12, MX_FIXED);
		vpalloc.allocate(4);
		vpalloc.allocate(CONTROL_UNITS);
		vpalloc.allocate(CONTROL_UNITS);
		vpalloc.allocate(CONTROL_UNITS);

		uint32_t i;
		for(i = 0; i < 2; i++) {
			pdirs[3].pde[i] = (i << 21) | 0x83;
		}
		for(; i < 512; i++) {
			pdirs[3].pde[i] = 0;
		}
		for(int i = 0; i < 4; i++) {
		pdpt->pdtpe[i] = ( ((uint32_t)(&pdirs[i])) - phyptr ) | 1;
		xiv::printf("PDP %d: ", i);
		xiv::printhex((size_t)&pdirs[i], 32);
		xiv::putc(10);
		}
		_ix_loadcr3((uint32_t)&_ivix_phy_pdpt); // reset page tables
		xiv::printf("Page directories mapped\n");
		for(int i = 0; i < 512; i++) {
			pdirs[0].pde[i] = 0;
			pdirs[1].pde[i] = 0;
			pdirs[2].pde[i] = 0;
		}
		xiv::printf("Page directories reset\n");
	}

	void map_page(phyaddr_t phy, uintptr_t t, uint32_t f) {
		uint32_t ofs_dp = (t >> 30) & 0x3;
		uint32_t ofs_d = (t >> 21) & 0x1FF;
		uint32_t ofs_t = (t >> 12) & 0x1FF;
		size_t const phyptr = ((&_kernel_start) - (&_kernel_load));
		uint32_t dirbase = ((uint32_t)pdpt->pdtpe[ofs_dp]);
		dirbase &= 0xfffff000;
		PageDir *dirptr = (PageDir*)(dirbase + phyptr);
		xiv::printf("T/%x/%x/%x/", ofs_dp, ofs_d, ofs_t);
		if(f & MAP_LARGE) {
			xiv::printhex((size_t)&dirptr->pde[ofs_d], 32);
			dirptr->pde[ofs_d] = phy.m | 1ull | (uint64_t)(f & 0x9f);
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
	int req_control(void (*)(void *, void *, uint32_t), void *) {
		return 0;
	}
	int req_allocrange(void (*allocadd)(void *, uint64_t, uint32_t), void * t, uint32_t ml) {
		xiv::print("MEM: Request new allocation blocks\n");
		uint32_t mmb = (ml / SM_PAGE_SIZE) + 1;
		if(mmb < 16) mmb = 16;
		uint64_t ph = blkalloc.allocate(mmb);
		if(ph == 0) return -1;
		uint64_t bu = vpalloc.allocate(mmb);
		if(bu == 0) return -2;
		allocadd(t, bu, SM_PAGE_SIZE * mmb);
		return 0;
	}
	void debug() {
		blkalloc.debug_table();
		memalloc.debug_table();
		vpalloc.debug_table();
	}
}

extern "C"
void * kmalloc(size_t l) {
	if(l > 1) {
		if(l & 0xf) {
			l += 16 - (l & 0xf);
		}
		return (void*)mem::memalloc.allocate(l);
	}
	return nullptr;
}

extern "C"
void * krealloc(void *, size_t) {
	return nullptr;
}

extern "C"
void kfree(void *) {
}

