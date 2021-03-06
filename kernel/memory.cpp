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
 */

#include "memory.hpp"
#include "ktext.hpp"
#include "kio.hpp"

extern "C" void _ix_loadcr3(uint32_t);

namespace mem {
	struct PageDir {
		uint64_t pde[512];
	};
	struct PageTable {
		uint64_t pte[512];
	};
	struct PageTableIndex {
		PageTable *ptp[512];
	};
	struct PageDirPtr {
		uint64_t pdtpe[4];
		PageDir *pdp[4];
		PageTableIndex *ptip[4];
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
		MX_CLEAR,
		MX_FIXED,
		MX_CONTROL,
		MX_ALLOC,
		MX_IO,
		MX_LINK,
		MX_RESV,
		MX_LAST // this must be last
	};
	const char * const EXTENTCLASS[] = {
		"UNSET",
		"-----",
		"FREE ",
		"CLEAR",
		"SYSTM",
		"CTRL ",
		"ALLOC",
		"I/O  ",
		"NLINK",
		"RESRV"
	};
	
	constexpr static int CONTROL_UNITS = 2;

	int req_control(void (*)(void *, void *, uint32_t), void *);
	int req_allocrange(void (*)(void *, uint64_t, uint32_t), void *, uint32_t);

	template<int ADDRSIZE, int UNIT, bool CULL = false, int RESERVE = 0, class T = MemExtent>
	class ExtentAllocator {
	public:
		const char *vmt;
		T *pbase;
		T *pfree;
		T *plast;
		T *pcblock;
		bool resv_alloc;
		constexpr static const uint32_t max_ext = ADDRSIZE / sizeof(T);
	public:
		ExtentAllocator() { vmt = nullptr; }
		ExtentAllocator(const char *n) { vmt = n; }
		ExtentAllocator(const ExtentAllocator&) = delete;
		ExtentAllocator(ExtentAllocator&&) = delete;
		ExtentAllocator& operator=(const ExtentAllocator&) = delete;
		ExtentAllocator& operator=(ExtentAllocator&&) = delete;

		void init(void * ldaddr, uint32_t adl) {
			xiv::printf("VMM: Initializing at %x", (uintptr_t)ldaddr);
			pbase = (T*)ldaddr;
			pbase->flags = MX_UNSET;
			pfree = pcblock = plast = pbase;
			T *p = plast;
			uint32_t c = (adl * ADDRSIZE) - sizeof(T);
			uint32_t ce = c - (RESERVE * sizeof(T));
			uint32_t sz = 0;
			while(sz < ce) {
				p->flags = MX_EMPTY;
				p++;
				sz += sizeof(T);
			}
			if(sz < c) {
				p->flags = MX_RESV;
				p++;
				sz += sizeof(T);
			}
			while(sz < c) {
				p->flags = MX_EMPTY;
				p++;
				sz += sizeof(T);
			}
			p->flags = MX_UNSET;
			resv_alloc = false;
			add_extent(0, adl, MX_CONTROL);
		}
		static void add_control_callback(void * t, uint64_t cba, uint32_t cbl) {
			((ExtentAllocator<ADDRSIZE,UNIT,CULL,RESERVE,T>*)t)->add_control(cba, cbl / ADDRSIZE);
		}
		static void add_memory_callback(void * t, uint64_t cba, uint32_t cbl) {
			((ExtentAllocator<ADDRSIZE,UNIT,CULL,RESERVE,T>*)t)->add_extent(cba, cbl, MX_FREE);
		}
		int aquire(uint32_t l) {
			if(l < 16*(ADDRSIZE/UNIT)) l = 16*(ADDRSIZE/UNIT);
			return req_allocrange(add_memory_callback, this, l);
		}
		int aquire_control() {
			return req_allocrange(add_control_callback, this, ADDRSIZE * CONTROL_UNITS);
		}
		void add_control(uint64_t cba, uint32_t cbl) {
			T *p = (T*)cba;
			T *initp = p;
			uint32_t c = (cbl * ADDRSIZE) - sizeof(T);
			uint32_t ce = c - (RESERVE * sizeof(T));
			uint32_t sz = 0;

			xiv::print("VMM:"); if(vmt) xiv::print(vmt);
			xiv::printf(" Adding control block (%x) > %x\n", cbl, initp);
			while(sz < ce) {
				p->flags = MX_EMPTY;
				p++;
				sz += sizeof(T);
			}
			if(sz < c) {
				p->flags = MX_RESV;
				p++;
				sz += sizeof(T);
			}
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
				if(p->flags >= MX_LAST) {
					xiv::print("VMM: Invalid table\n");
					return;
				}
				if(p->flags == MX_RESV) {
					p->flags = MX_EMPTY;
				}
				if(p->flags == MX_LINK) {
					p = p->next;
				} else {
					p++;
				}
			}
			xiv::print("VMM:"); if(vmt) xiv::print(vmt);
			xiv::printf(" Adding link (%d) %x > %x\n", sz, p, initp);
			p->flags = MX_LINK;
			p->next = initp;
			p->length = sz;
		}
		T *next_empty_extent() {
			T *p = plast;
			uint32_t lc = 0;
			uint32_t lls = 0;
			p = pcblock;
			lls = ((p->length * ADDRSIZE) / sizeof(T)) - 1;
			while(p->flags < MX_LAST
					&& p->flags != MX_UNSET
					&& p->flags != MX_EMPTY
					&& p->flags != MX_RESV) {
				if(p->flags == MX_LINK) {
					xiv::print("VMM:"); if(vmt) xiv::print(vmt);
					if(lc < lls) {
						xiv::printf(" invalid link at (%d/%d) %x\n", lc, lls, p);
						return p;
					} else {
					xiv::printf(" following link at (%d) %x > ", lc, p);
					xiv::printhexx(p->base, 64); xiv::putc(10);
					lls = (p->length / sizeof(T)) - 1;
					p = p->next;
					lc = 0;
					}
				} else {
					p++;
					lc++;
				}
			}
			return p;
		}
		void add_extent(uint64_t b, uint32_t l, uint32_t f) {
			if(l == 0 || f == 0) return;
			if(f == MX_FREE) {
				if(pfree->flags == MX_FREE) {
					// if we are adding memory on the end, cull it
					if(pfree->base + pfree->length * UNIT == b) {
						xiv::printf("VMM: Adding Free ");
						xiv::printhexx(pfree->base + (pfree->length * UNIT), 64);
						xiv::printf("+%d\n", l*UNIT);
						pfree->length += l;
						return;
					}
				}
				pfree = plast;
			} else if(f == MX_CONTROL) {
				pcblock = plast;
			}
			if(plast->flags == MX_UNSET) {
				xiv::printf("VMM: FAILED to add at end of block.\n");
			}
			plast->base = b;
			plast->length = l;
			plast->flags = f;
			plast = next_empty_extent();
		}
		T * find_extent(uint64_t b) {
			T *pt = pbase;
			while(pt->flags != MX_UNSET) {
				if(pt->flags != MX_CONTROL) {
					if(pt->base <= b && b < (pt->base + pt->length * UNIT)) return pt;
				}
				if(pt->flags == MX_LINK) {
					pt = pt->next;
				} else {
					pt++;
				}
			}
			return nullptr;
		}
		/* find free extent of at least size l, or last if l == 0 */
		void find_free_extent(uint32_t l) {
			T *pt = pbase;
			while(pt->flags != MX_UNSET) {
				if(pt->flags == MX_FREE && pt->length >= l) {
					pfree = pt;
					if(l) return;
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
				if(pt->flags != MX_CONTROL
						&& pt->flags != MX_LINK
						&& pt->flags != MX_EMPTY) {
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
			if(pfree->flags != MX_FREE) find_free_extent(0);
		}
		int free(uint64_t b) {
			T *p = find_extent(b);
			if(!p) {
				xiv::printf("MMA: No such block %x%x\n", b);
				return -1;
			}
			if(p->base != b) {
				xiv::print("MMA: Mid block free\n");
				return -1;
			}
			if(p->flags == MX_FREE) {
				xiv::print("MMA: Double free\n");
				return -1;
			}
			if(p->flags == MX_CONTROL) {
				xiv::print("MMA: control block free\n");
			}
			p->flags = MX_FREE;
			if(pfree->base > p->base) {
				if(p->base + (p->length*UNIT) == pfree->base && pfree->flags == MX_FREE) {
					pfree->flags = MX_EMPTY;
					plast = pfree;
					p->length += pfree->length;
				}
				pfree = p;
			}
			return 0;
		}
		uint64_t allocate(uint32_t l) {
			return allocate(l, MX_ALLOC);
		}
		uint64_t allocate(uint32_t l, uint32_t f) {
			uint64_t mm;
			if(!resv_alloc && plast->flags == MX_RESV) {
				resv_alloc = true;
				xiv::print("VMM: Entering Reserve\n");
				if(aquire_control()) {
					xiv::printf("VMM: FAILED to add at reserved block.\n");
				}
				plast = next_empty_extent();
				xiv::print("VMM: Exiting Reserve\n");
				resv_alloc = false;
			} else if(plast->flags == MX_UNSET) {
				xiv::print("VMM: At list end\n");
				if(aquire_control()) {
					xiv::printf("VMM: FAILED to add at reserved block.\n");
				}
			}
			if(pfree->flags != MX_FREE) {
				find_free_extent(l);
				if(pfree->flags != MX_FREE) {
					xiv::print("MMA: No FREE blocks\n");
					if(aquire(l)) {
						xiv::print("MMA: Allocation fail\n");
						return 0;
					}
				}
			}
			if(pfree->length >= l) {
				mm = pfree->base;
				mark_extent(mm, l, f);
				goto exitmm;
			} else {
				find_free_extent(l);
				if(pfree->length >= l && pfree->flags == MX_FREE) {
					mm = pfree->base;
					mark_extent(mm, l, f);
					goto exitmm;
				} else {
					find_free_extent(0); // find last free
					if(!CULL && aquire(l) == 0) {
						mm = pfree->base;
						mark_extent(mm, l, f);
						if(!CULL) find_free_extent(1);
						goto exitmm;
					}
				}
			}
			xiv::printf("MMA: Length Exceeded %d/%d\n", l, pfree->length);
			return 0;
exitmm:
			return mm;
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
				xiv::printf("ALTE: %d ", addrc);
				xiv::printhexx(pt->base, 64);
				xiv::print(" f:");
				if(pt->flags >= MX_LAST) {
					xiv::print("INVALID\n");
					return;
				}
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
				if(pt == plast) {
					xiv::print("<LAST");
				}
				xiv::putc(10);
				lastempty = (pt->flags == MX_EMPTY);
				}
				if(pt->flags == MX_LINK) {
					if(addrc < cbls - 1) {
						xiv::printf("Invalid link! ");
						addrc++;
						pt++;
					} else {
						xiv::printf("Block: %d/%d\n", addrc, cbls);
						addrc = 0;
						pt = pt->next;
					}
				} else {
					addrc++;
					if(pt->flags == MX_UNSET) break;
					pt++;
				}
			}
			xiv::printf("Block: %d/%d\n", addrc, cbls);
		}
		void debug_counts() {
			T *pt = pbase;
			uint32_t addrc = 0;
			uint32_t cbls = 0;
			uint32_t n_free = 0;
			uint32_t n_alloc = 0;
			uint32_t n_ctl = 0;
			uint32_t n_empty = 0;
			while(true) {
				if(pt->flags == MX_CONTROL) {
					cbls = max_ext * pt->length;
					n_ctl++;
				}
				switch(pt->flags) {
				case MX_FREE: n_free++; break;
				case MX_ALLOC: n_alloc++; break;
				case MX_EMPTY: n_empty++; break;
				}
				if(pt->flags == MX_LINK) {
					addrc = 0;
					pt = pt->next;
				} else {
					addrc++;
					if(pt->flags == MX_UNSET) break;
					pt++;
				}
			}
			xiv::printf("Free: %d, Alloc: %d, Ctrl: %d, Empty: %d\n",
					n_free, n_alloc, n_ctl, n_empty);
			xiv::printf("Block: %d/%d\n", addrc, cbls);
		}
	};

	constexpr int const SM_PAGE_SIZE = 0x1000;

	ExtentAllocator<SM_PAGE_SIZE, 1, false, 4> memalloc("sysmem");
	ExtentAllocator<SM_PAGE_SIZE, SM_PAGE_SIZE, true, 4> blkalloc("sysphy");
	ExtentAllocator<SM_PAGE_SIZE, SM_PAGE_SIZE, true, 8> vpalloc("sysvmm");

	extern "C" {
	extern uint8_t _ivix_phy_pdpt;
	extern uint32_t const _ivix_pdpt;
	extern char _kernel_start;
	extern char _kernel_end;
	extern char _kernel_load;
	}

	PageDirPtr *pdpt;
	static uint32_t tablefreeblocks;
	static uint32_t tablefreelock;
	union ChainPtr {
		ChainPtr *next;
		uint8_t block[SM_PAGE_SIZE];
	};
	static ChainPtr *blockfreeptr;

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
		tablefreeblocks = 0;
		tablefreelock = 0;
		blockfreeptr = nullptr;
		pdpt->pdp[0] = (PageDir*)blockptr; blockptr += 0x1000;
		pdpt->pdp[1] = (PageDir*)blockptr; blockptr += 0x1000;
		pdpt->pdp[2] = (PageDir*)blockptr; blockptr += 0x1000;
		pdpt->pdp[3] = (PageDir*)blockptr; blockptr += 0x1000;
		pdpt->ptip[0] = (PageTableIndex*)blockptr; blockptr += 0x1000;
		pdpt->ptip[1] = (PageTableIndex*)blockptr; blockptr += 0x1000;
		pdpt->ptip[2] = (PageTableIndex*)blockptr; blockptr += 0x1000;
		pdpt->ptip[3] = (PageTableIndex*)blockptr; blockptr += 0x1000;
		blkalloc.init(blockptr, CONTROL_UNITS);
		load_memmap();
		blkalloc.mark_extent((size_t)(&_kernel_load), (size_t)(&_kernel_end - &_kernel_start) >> 12, MX_FIXED);
		blkalloc.allocate(8);
		blkalloc.allocate(CONTROL_UNITS);
		blkalloc.allocate(CONTROL_UNITS);
		blkalloc.allocate(CONTROL_UNITS);
		blockptr += 0x2000;
		memalloc.init(blockptr, CONTROL_UNITS);
		blockptr += 0x2000;
		vpalloc.init(blockptr, CONTROL_UNITS);
		vpalloc.add_extent(0xc0000000, 0xffff, MX_FREE);
		vpalloc.add_extent(0xe0000000, 0x20000, MX_FREE);
		vpalloc.mark_extent(0xc0000000, 0x100, MX_FIXED);
		vpalloc.mark_extent((size_t)(&_kernel_start), (size_t)(&_kernel_end - &_kernel_start) >> 12, MX_FIXED);
		vpalloc.allocate(8);
		vpalloc.allocate(CONTROL_UNITS);
		vpalloc.allocate(CONTROL_UNITS);
		vpalloc.allocate(CONTROL_UNITS);

		uint32_t i;
		for(i = 0; i < 2; i++) {
			pdpt->pdp[3]->pde[i] = (i << 21) | 0x83;
		}
		for(; i < 512; i++) {
			pdpt->pdp[3]->pde[i] = 0;
		}
		for(int i = 0; i < 4; i++) {
		pdpt->pdtpe[i] = ( ((uint32_t)(pdpt->pdp[i])) - phyptr ) | 1;
		xiv::printf("PDP %d: ", i);
		xiv::printhexx((size_t)&pdpt->pdp[i], 64);
		xiv::putc(10);
		}
		_ix_loadcr3((uint32_t)&_ivix_phy_pdpt); // reset page tables
		xiv::printf("Page directories mapped\n");
		for(int i = 0; i < 512; i++) {
			pdpt->pdp[0]->pde[i] = 0;
			pdpt->pdp[1]->pde[i] = 0;
			pdpt->pdp[2]->pde[i] = 0;
			pdpt->ptip[0]->ptp[i] = 0;
			pdpt->ptip[1]->ptp[i] = 0;
			pdpt->ptip[2]->ptp[i] = 0;
			pdpt->ptip[3]->ptp[i] = 0;
		}
		xiv::printf("Page directories reset\n");
	}

	void init_page_chain(void * first, int n) {
		if(n < 1) return;
		ChainPtr *nextpage;
		uint32_t tfc = 0;
		if(!first) xiv::print("VMM:ERR: Bad page allocation.\n");
		if(!blockfreeptr) {
			blockfreeptr = (ChainPtr*)first;
			blockfreeptr->next = nullptr;
			nextpage = blockfreeptr;
			xiv::printf("VMM:First block at %0x\n", first);
		} else {
			nextpage = blockfreeptr;
			while(nextpage->next) {
				nextpage = nextpage->next;
				tfc++;
				xiv::printf("VMM:mNext: %d/%d %0x\n", tfc, tablefreeblocks, (uint32_t)nextpage);
			}
			nextpage->next = (ChainPtr*)first;
			nextpage = nextpage->next;
		}
		tablefreeblocks += n;
		while(--n) {
			nextpage->next = nextpage+1;
			nextpage++;
			xiv::printf("VMM:sNext: %0x\n", (uint32_t)nextpage);
		}
		nextpage->next = nullptr;
	}
	uint64_t translate_page(uintptr_t t) {
		uint32_t ofs_dp = (t >> 30) & 0x3;
		uint32_t ofs_d = (t >> 21) & 0x1FF;
		uint32_t ofs_t = (t >> 12) & 0x1FF;
		PageDir *dirptr = pdpt->pdp[ofs_dp];
		if(!dirptr) return 0;
		uint64_t tablebase = dirptr->pde[ofs_d];
		if(!(tablebase & 1)) return 0;
		if(tablebase & MAP_LARGE) {
			return (tablebase & ~0x1ffffful) | (t & 0x1ffffful);
		}
		PageTable *ptptr = pdpt->ptip[ofs_dp]->ptp[ofs_d];
		if(!ptptr) return 0;
		return (ptptr->pte[ofs_t] & ~0xffful) | (t & 0xffful);
	}
	void map_page(phyaddr_t phy, uintptr_t t, uint32_t f) {
		uint32_t ofs_dp = (t >> 30) & 0x3;
		uint32_t ofs_d = (t >> 21) & 0x1FF;
		uint32_t ofs_t = (t >> 12) & 0x1FF;
		PageDir *dirptr = pdpt->pdp[ofs_dp];
		if(f & MAP_LARGE) {
			xiv::printhex((size_t)&dirptr->pde[ofs_d], 32);
			dirptr->pde[ofs_d] = phy.m | 1ull | (uint64_t)(f & 0x9f);
			xiv::printf("map_page %x = (%x)\n", t, dirptr->pde[ofs_d]);
		} else {
			uint64_t tablebase = dirptr->pde[ofs_d];
			// TODO if(tablebase) not present or present large page
			if(!(tablebase & 1)) {
				xiv::print("MEM: tablebase not present\n");
			}
			if(tablebase & MAP_LARGE) {
				// ignore large mapping overwrite
				//xiv::print("VMM:WARN: Large map exist./");
			} else {
				PageTableIndex *pti = pdpt->ptip[ofs_dp];
				if(!pti) {
					xiv::print("VMM:ERR: Table Index not exist.");
					return;
				}
				PageTable *ptptr = pti->ptp[ofs_d];
				if(!ptptr) {
					xiv::printf("VMM:/%x/%x/%x/", ofs_dp, ofs_d, ofs_t);
					xiv::print("add pagetable/");
					if(tablefreeblocks < 20 && !tablefreelock) {
						xiv::print("init_chain/");
						tablefreelock++;
						init_page_chain(alloc_pages(20, 0), 20);
						tablefreelock--;
					}
					ptptr = pti->ptp[ofs_d] = (PageTable*)blockfreeptr;
					xiv::print("get_next/");
					if(blockfreeptr->next == nullptr) {
						xiv::print("end of blocks!/");
					} else {
						blockfreeptr = blockfreeptr->next;
						tablefreeblocks--;
					}
					xiv::print("zero table/");
					uint64_t *cpte = pti->ptp[ofs_d]->pte;
					for(int x = 0; x < 512; x++) {
						cpte[x] = 0;
					}
					_ix_loadcr3((uint32_t)&_ivix_phy_pdpt); // reset page tables
				} else {
					//xiv::print("VMM:ERR: Page Table exist. ");
					//xiv::printhexx(ptptr->pte[ofs_t], 64); xiv::putc('>');
					//xiv::printhexx(phy.m, 64); xiv::putc(10);
				}
				if((tablebase & 1) == 0) {
					xiv::print("map pagetable\n");
					dirptr->pde[ofs_d] = translate_page((uintptr_t)pti->ptp[ofs_d]) | 1 | MAP_RW;
				}
				ptptr->pte[ofs_t] = phy.m | 1 | (f & 0x1f);
			}
		}
	}
	void * request(size_t sz, void* hint, uint64_t phys, uint32_t flag) {
		uint32_t mapflag = 0;
		if(flag & RQ_RW) {
			mapflag |= MAP_RW;
		}
		if(flag & RQ_HINT) {
			uintptr_t ba = (uintptr_t)hint;
			uintptr_t enda = ba + (sz & ~(SM_PAGE_SIZE-1));
			if(ba > 0xffff) { // don't allow allocations at <64k VM
				do {
					map_page(phys, ba, mapflag);
					phys += SM_PAGE_SIZE;
					ba += SM_PAGE_SIZE;
				} while(ba < enda);
				return hint;
			}
		}
		return nullptr;
	}
	void * alloc_pages(size_t mpc, uint32_t) {
		xiv::print("MEM: Request real pages\n");
		uint64_t bu = vpalloc.allocate(mpc);
		if(bu == 0) {
			xiv::print("MEM: Virtual allocation failed\n");
			return nullptr;
		}
		uint64_t ph = blkalloc.allocate(mpc);
		if(ph == 0) {
			xiv::print("MEM: Physical allocation failed\n");
			return nullptr;
		}
		xiv::printf("MEM: Page allocation %0lx+%x > %0lx\n", ph, mpc * SM_PAGE_SIZE, bu);
		request(mpc * SM_PAGE_SIZE, (void*)bu, ph, RQ_RW | RQ_HINT);
		return (void*)bu;
	}
	int req_control(void (*)(void *, void *, uint32_t), void *) {
		return 0;
	}
	int req_allocrange(void (*allocadd)(void *, uint64_t, uint32_t), void * t, uint32_t ml) {
		uint32_t mmb = (ml / SM_PAGE_SIZE) + ((ml % SM_PAGE_SIZE)>0? 1 : 0);
		uint64_t bu = vpalloc.allocate(mmb);
		if(bu == 0) {
			xiv::print("MEM: Virtual allocation failed\n");
			return -2;
		}
		uint64_t ph = blkalloc.allocate(mmb);
		if(ph == 0) {
			xiv::print("MEM: Physical allocation failed\n");
			return -1;
		}
		xiv::printf("MEM: Block allocation %0lx > %0lx\n", ph, bu);
		request(mmb * SM_PAGE_SIZE, (void*)bu, ph, RQ_RW | RQ_HINT);
		allocadd(t, bu, SM_PAGE_SIZE * mmb);
		return 0;
	}
	void debug(int t) {
		switch(t) {
		case 1:
			blkalloc.debug_table();
			break;
		case 2:
			vpalloc.debug_table();
			break;
		case 3:
			memalloc.debug_counts();
			break;
		default:
			memalloc.debug_table();
		}
	}
	void ref_destroy(size_t r) {
		if(!r) return;
	}
	int ref_add(size_t r) {
		if(!r) return 0;
		return 0;
	}
	size_t ref_create(void *p, size_t z) {
		if(!p || !z) return 0;
		return 0;
	}
	int ref_lock(size_t r) {
		if(!r) return 0;
		return 0;
	}
	int ref_unlock(size_t r) {
		if(!r) return 0;
		return 0;
	}
	void * ref_getlockptr(size_t r) {
		if(!r) return nullptr;
		return nullptr;
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
	xiv::print("MMA: bad size\n");
	return nullptr;
}

extern "C"
void * krealloc(void *, size_t) {
	return nullptr;
}

extern "C"
void kfree(void *p) {
	mem::memalloc.free((uintptr_t)p);
}

void * operator new(size_t v) {
	return kmalloc(v);
}
void * operator new[](size_t v) {
	return kmalloc(v);
}
void operator delete(void *d) {
	kfree(d);
}

