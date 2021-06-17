/* ***
 * memory.cpp - implementation of memory management functions
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "memory.hpp"
#include "ktext.hpp"
#include "kio.hpp"

extern "C" void _ix_loadcr3(uint32_t);

namespace mem {

enum PageFlags : uint64_t {
	NotPresent = 0,
	Present = 1,
	Writable = 1<<1,
	Usermode = 1<<2,
	Writethrough = 1<<3,
	CacheDisable = 1<<4,
	Accessed = 1<<5,
	Dirty = 1<<6,
	Large = 1<<7,
	Global = 1<<8,
	NoExec = 1ull<<63,
};

struct PageDir {
	volatile uint64_t entry[512];
	template<typename... Args>
	void set(int index, uint64_t addr, Args... flags) {
		if( (flags | ...) & Large)
			entry[index] = (addr & 0x0007ffffffe00000ull) | (flags | ...);
		else
			entry[index] = (addr & 0x0007fffffffff000ull) | (flags | ...);
	}
};

struct PageTable {
	volatile uint64_t pte[512];
};

struct PageDirIndex {
	uint64_t entry[512];
	void set(int index, void *ptr) {
		if(ptr == 0) {
			entry[index] = 0;
		} else {
			entry[index] = (((uint64_t)ptr) & 0xfffff000ull) | 1;
		}
	}
	PageTable* get(int index) {
		return (PageTable*)(entry[index] & 0xfffff000ull);
	}
};

struct PageDirPtr {
	volatile uint64_t pdtpe[4];
	PageDir *pgdir[4];
	PageDirIndex *pgdir_idx[4];
};

struct MemMap {
	uint64_t start;
	uint64_t size;
	uint32_t type;
	uint32_t attrib;
};

extern "C" {
extern uint8_t _ivix_phy_pdpt;
extern PageDirPtr _ivix_pdpt;
extern char _kernel_start;
extern char _kernel_end;
extern char _kernel_load;
}

enum MEMEXTENT : uint32_t {
	MX_UNSET, MX_CONTROL, MX_LINK, MX_IO,
	MX_EMPTY, MX_FREE, MX_CLEAR,
	MX_FIXED, MX_ALLOC, MX_PRE,
	MX_LAST // this must be last
};
const char * const EXTENTCLASS[] = {
	"---","CTL","NLK","I/O",
	"-E-","FRE","CLR",
	"SYS","ALC","PTA",
};

int req_control(void (*)(void *, void *, uint32_t), void *);
int req_allocrange(void (*)(void *, uint64_t, uint32_t), void *, uint32_t);

enum TRIE_TYPE : uint8_t {
	TT_UNSET, TT_LINK, TT_LEAF,
	TT_PREFIX,
	TT_MASK_1,
	TT_MASK_2,
	TT_MASK_3,
	TT_LIST_3,
	TT__TRIE_TYPE // last entry
};
const char * const TRIECLASS[] = {
	"-----","*LINK","LEAF_",
	"PREFIX",
	"MASK1",
	"MASK2",
	"MASK3",
	"LIST3",
};
struct TrieRadix {
	uint32_t radix;
	uint8_t ref[8];
} __attribute__((packed));
struct TriePrefix {
	uint16_t pfx_start;
	uint16_t pfx_end;
	uint64_t value;
} __attribute__((packed));
struct TrieLeaf {
	uint32_t ref;
	MEMEXTENT flag;
	uint32_t length;
} __attribute__((packed));
struct TrieControl {
	TRIE_TYPE type;
	uint8_t res[3];
	union {
		uint32_t raw[3];
		TrieRadix radix;
		TriePrefix prefix;
		TrieLeaf leaf;
	};
	TrieControl& init(TRIE_TYPE t) {
		raw[0] = 0;
		raw[1] = 0;
		raw[2] = 0;
		type = t;
		return *this;
	}
	const char *type_name() const {
		if(type >= TT__TRIE_TYPE) return "???";
		return TRIECLASS[type];
	}
} __attribute__((packed));

struct Extent {
	MEMEXTENT flags;
	union {
		uint64_t base;
		Extent *next;
	};
	size_t length;
	uint64_t end() const {
		return base + (length * PAGE_SIZE);
	}
	bool operator==(const Extent &rhs) const {
		return (this->flags == rhs.flags)
		&& (this->length == rhs.length)
		&& (((this->base ^ rhs.base) & 0xfffffffffffff) == 0);
	}
	bool operator!=(const Extent &rhs) const {
		return (this->flags != rhs.flags)
		|| (this->length != rhs.length)
		|| (((this->base ^ rhs.base) & 0xfffffffffffff) != 0);
	}
};

class PageAllocator {
public:
	static constexpr uint32_t BLOCKSIZE = PAGE_SIZE / 2;

	struct ExtentTable;
	struct ExtentTableHeader {
		ExtentTable *next;
		uint32_t ifree;
	};
	static constexpr size_t max_ext =
		(BLOCKSIZE - sizeof(ExtentTableHeader)) / sizeof(Extent);
	static constexpr size_t max_trie =
		(BLOCKSIZE) / sizeof(TrieControl);
	struct ExtentTable : ExtentTableHeader {
		Extent extent[max_ext];
		TrieControl trie[max_trie];
		Extent* free() { return &extent[ifree]; }
		void init(ExtentTable *end) {
			this->next = end;
			this->ifree = 0;
			Extent *p = this->extent;
			uint32_t index = 0;
			while(index <= max_ext) {
				p->length = ~0ul;
				p->base = ~0ull;
				p->flags = MX_UNSET;
				p++;
				index++;
			}
			// trie system
			index = 0;
			while(index <= max_trie) {
				TrieControl &t = this->trie[index];
				t.type = TT_UNSET;
				index++;
			}
			this->trie[0].init(TT_MASK_2);
		}
	};
	struct ExtentInterator {
		ExtentTable *start;
		ExtentTable *cur;
		PageAllocator &pa;
		uint32_t index;
		ExtentInterator(PageAllocator &_pa, ExtentTable *begin) : pa(_pa) {
			cur = start = begin;
			index = 0;
		}
		ExtentTable* next_table() const {
			if(cur == pa.end) {
				return pa.begin;
			}
			return cur->next;
		}
		void set_free() { cur->ifree = index; }
		bool is_free() const { return index == cur->ifree; }
		bool has_next() const {
			if(index < max_ext) {
				return true;
			}
			return !(next_table() == start);
		}
		void operator++(int) {
			if(index < max_ext) {
				index++;
				return;
			}
			cur = next_table();
		}
		Extent *operator->() const { return &cur->extent[index]; }
		Extent &operator*() const { return cur->extent[index]; }
	};
public:
	const char *vmt;
	ExtentTable *begin;
	ExtentTable *end;
	ExtentTable *active;
public:
	PageAllocator() { vmt = nullptr; active = nullptr; }
	PageAllocator(const char *n) { vmt = n; }
	PageAllocator(const PageAllocator&) = delete;
	PageAllocator(PageAllocator&&) = delete;
	PageAllocator& operator=(const PageAllocator&) = delete;
	PageAllocator& operator=(PageAllocator&&) = delete;

	void init(void * ldaddr) {
		xiv::printf("%s: Initializing at %x\n", vmt, (uintptr_t)ldaddr);
		active = begin = end = (ExtentTable*)ldaddr;
		active->init(end);
	}

	static void add_control_callback(void * t, uint64_t cba, uint32_t cbl) {
		((PageAllocator*)t)->add_control(cba, cbl / BLOCKSIZE);
	}

	static void add_memory_callback(void * t, uint64_t cba, uint32_t cbl) {
		((PageAllocator*)t)->add_extent(cba, cbl, MX_FREE);
	}

	int aquire(uint32_t l) {
		if(l < 16*(BLOCKSIZE/PAGE_SIZE)) l = 16*(BLOCKSIZE/PAGE_SIZE);
		xiv::printf("%s: aquire\n", vmt);
		return req_allocrange(add_memory_callback, this, l);
	}

	int aquire_control() {
		xiv::printf("%s: aquire_control\n", vmt);
		return req_allocrange(add_control_callback, this, BLOCKSIZE);
	}

	void add_control(uint64_t cba, uint32_t cbl) {
		ExtentTable *table;
		table = active;
		while(table->next != end) {
			table = table->next;
		}
		xiv::printf("%s: add_control (%x * %x)\n", vmt, cba, cbl);
		for(uint32_t i = 0; i < cbl; i++) {
			table->next = ((ExtentTable*)cba) + i;
			table->next->init(end);
			table = table->next;
		}
	}

	Extent* next_empty_extent() {
		ExtentInterator extent_iter(*this, active);
		uint32_t index = 0;
		for(;extent_iter.has_next(); extent_iter++) {
			Extent &p = *extent_iter;
			if(p.flags >= MX_LAST) {
				xiv::printf("%s: invalid table entry at %08x[%d]\n", vmt, extent_iter.cur, index);
				return nullptr;
			} else if (p.flags == MX_UNSET) {
				return &p;
			}
		}
		return nullptr;
	}

private:
	void add_extent(uint64_t b, uint32_t l, MEMEXTENT f) {
		if(l == 0 || f == MX_UNSET || f >= MX_LAST) return;
		if(f == MX_FREE) {
			auto pfree = active->free();
			if(pfree->flags == MX_FREE) {
				// if we are adding memory on the end, cull it
				// TODO cull and collapse extents elsewhere in the table
				if(pfree->end() == b) {
					xiv::printf("%s: Adding Free %0lx+%d\n",
							vmt, pfree->end(), l * PAGE_SIZE);
					pfree->length += l;
					return;
				}
			}
			//active->ifree = active->ilast;
		}
		// TODO cull and collapse extents
		Extent *plast = next_empty_extent();
		if(plast && plast->flags == MX_UNSET) {
			plast->base = b;
			plast->length = l;
			plast->flags = f;
		}
	}

public:
	Extent* find_type_extent(uint32_t l, MEMEXTENT f) {
		ExtentInterator extent_iter(*this, active);
		for(;extent_iter.has_next(); extent_iter++) {
			Extent *p = &*extent_iter;
			if(p->flags == f && p->length >= l) {
				return p;
			}
		}
		return nullptr;
	}

	/* find free extent of at least size l, or last if l == 0 */
	void find_free_extent(uint32_t l) {
		ExtentInterator extent_iter(*this, active);
		for(;extent_iter.has_next(); extent_iter++) {
			Extent *p = &*extent_iter;
			if(p->flags == MX_FREE && p->length >= l) {
				extent_iter.set_free();
				active = extent_iter.cur;
				return;
			}
		}
	}

	Extent* intersect_extent(uint64_t b, uint32_t l) {
		ExtentInterator extent_iter(*this, active);
		uint64_t e = (b + l * PAGE_SIZE);
		for(;extent_iter.has_next(); extent_iter++) {
			Extent *p = &*extent_iter;
			if(p->flags != MX_UNSET) {
				if(b >= p->base && e <= p->end()) {
					return p;
				}
			}
		}
		return nullptr;
	}

	void set_trie_extent(Extent &ext) {
		//size_t upper_mask = (size_t)0x100000000; // 32 bit only
		uint8_t scan_start = 32;
		TrieControl *table = begin->trie;
		uint8_t index = 0;
		while(scan_start > 12) {
			TrieControl &item = table[index];
			switch(item.type) {
			case TT_MASK_1:
			{
				scan_start -= 1;
				uint8_t test_val = (ext.base >> scan_start) & 0x1;
				if(item.radix.ref[test_val]) {
					index = (item.radix.ref[test_val] + index) % max_trie;
				} else {
					size_t k = 0;
					for(;k < max_trie; k++) { // TODO, put this in a function
						if(table[k].type == TT_UNSET) break;
					}
					if(k < max_trie) {
						if(scan_start > 12) {
							table[k].init(TT_LIST_3);
							item.radix.ref[test_val] = (k - index) % max_trie;
							index = k;
						} else {
							table[k].init(TT_LEAF);
							item.radix.ref[test_val] = (k - index) % max_trie;
						}
					} else {
						xiv::printf("%s: trie: no free slots\n", vmt);
						scan_start = 0;
					}
				}
				break;
			}
			case TT_MASK_2:
			{
				scan_start -= 2;
				uint8_t test_val = (ext.base >> scan_start) & 0x3;
				if(item.radix.ref[test_val]) {
					index = (item.radix.ref[test_val] + index) % max_trie;
				} else {
					size_t k = 0;
					for(;k < max_trie; k++) { // TODO, put this in a function
						if(table[k].type == TT_UNSET) break;
					}
					if(k < max_trie) {
						if(scan_start > 12) {
							table[k].init(TT_MASK_3);
							item.radix.ref[test_val] = (k - index) % max_trie;
							index = k;
						} else {
							table[k].init(TT_LEAF);
							item.radix.ref[test_val] = (k - index) % max_trie;
						}
					} else {
						xiv::printf("%s: trie: no free slots\n", vmt);
						scan_start = 0;
					}
				}
				break;
			}
			case TT_MASK_3:
			{
				scan_start -= 3;
				uint8_t test_val = (ext.base >> scan_start) & 0x7;
				if(item.radix.ref[test_val]) {
					index = (item.radix.ref[test_val] + index) % max_trie;
				} else {
					size_t k = 0;
					for(;k < max_trie; k++) { // TODO, put this in a function
						if(table[k].type == TT_UNSET) break;
					}
					if(k < max_trie) {
						item.radix.ref[test_val] = (k - index) % max_trie;
						index = k;
						if(scan_start > 20) {
							table[k].init(TT_MASK_3);
						} else if(scan_start > 12) {
							table[k].init(TT_MASK_1);
						} else {
							table[k].init(TT_LEAF);
						}
					} else {
						xiv::printf("%s: trie: no free slots\n", vmt);
						scan_start = 0;
					}
				}
				break;
			}
			case TT_LIST_3:
			{
				scan_start -= 4;
				uint8_t test_val = (ext.base >> scan_start) & 0xf;
				uint32_t test_radix = item.radix.radix;
				uint32_t test_free = 8;
				uint32_t i;
				for(i = 0; i < 8; i++, test_radix >>= 4) {
					if(!item.radix.ref[i]) {
						if(i < test_free) test_free = i;
						continue;
					}
					if((test_radix & 0xf) == test_val) {
						// decend the trie!
						index = (item.radix.ref[i] + index) % max_trie;
						break;
					}
				}
				if(i == 8) {
					if(test_free == 8) {
						xiv::printf("%s: trie: radix full\n", vmt);
						return;
					}
					item.radix.radix &= ~(0xf << (test_free * 4));
					item.radix.radix |= test_val << (test_free * 4);
					size_t k = 0;
					for(;k < max_trie; k++) { // TODO, put this in a function
						if(table[k].type == TT_UNSET) break;
					}
					if(k < max_trie) {
						if(scan_start > 12) {
							table[k].init((scan_start == 24) ? TT_MASK_3 : TT_LIST_3);
							item.radix.ref[test_free] = (k - index) % max_trie;
							index = k;
						} else {
							TrieControl &lea = table[k].init(TT_LEAF);
							lea.leaf.flag = ext.flags;
							lea.leaf.length = ext.length;
							item.radix.ref[test_free] = (k - index) % max_trie;
						}
					} else {
						xiv::printf("%s: trie: no free slots\n", vmt);
						scan_start = 0;
					}
				}
				break;
			}

			default:
				scan_start = 0;
				break;
			}
		}
	}

	void set_extent(uint64_t b, uint32_t l, MEMEXTENT f) {
		Extent to_set;
		to_set.base = b;
		to_set.length = l;
		to_set.flags = f;
		set_trie_extent(to_set);
		Extent *p;
		p = intersect_extent(b, l);
		// !!! important !!! this function does NOT perform bounds validation
		if(p) {
			if(b == p->base) {
				Extent *prev = nullptr;
				if(b >= PAGE_SIZE) {
					prev = intersect_extent(b - PAGE_SIZE, 1);
				}
				if(prev && prev->end() == b && prev->flags == f) {
					// combine with the previous adjacent extent
					prev->length += l;
					if(p->length > l) {
						// shrink the intersected extent
						p->length -= l;
						p->base += l * PAGE_SIZE;
					} else {
						// consume the intersected extent
						if(p->length < l) {
							// TODO if we exceeded the length of the intersected extent
							// we should check that we have not run over more extents
							xiv::printf("%s: Warning: Exceeded extent limit %0lx\n", vmt, b);
						}
						p->flags = MX_UNSET;
					}
				} else {
					if(p->length > l) {
						// shrink the intersected extent
						p->length -= l;
						p->base += l * PAGE_SIZE;
						// add this new one to where the intersected extent started
						add_extent(b, l, f);
					} else {
						// consume the intersected extent
						if(p->length < l) {
							// TODO if we exceeded the length of the intersected extent
							// we should check that we have not run over more extents
							xiv::printf("%s: Warning: Exceeded extent limit %0lx\n", vmt, b);
						}
						p->length = l;
						p->flags = f;
					}
				}
			} else {
				uint64_t adru = l * PAGE_SIZE;
				uint64_t adrm = (b - p->base) / PAGE_SIZE;
				add_extent(b, l, f);
				add_extent(b + adru, p->length - (l + adrm), p->flags);
				p->length = adrm;
			}
		} else {
			add_extent(b, l, f);
		}
		if(active->free()->flags != MX_FREE) find_free_extent(0);
	}

	uint64_t allocate(uint32_t l) {
		return allocate(l, MX_ALLOC);
	}

	uint64_t allocate_res(uint32_t l, MEMEXTENT f) {
		uint64_t mm;
		if(!active) {
			return 0;
		}
		Extent *te = find_type_extent(l, MX_PRE);
		if(te && te->length >= l) {
			mm = te->base;
			set_extent(mm, l, f);
			return mm;
		} else {
			xiv::printf("%s: no reserve for length %d\n", vmt, l);
		}
		return 0;
	}

	uint64_t allocate(uint32_t l, MEMEXTENT f) {
		uint64_t mm;
		if(!active) {
			return 0;
		}
		// FIXME
		//if(active->last()->flags == MX_UNSET) {
			//xiv::printf("%s: At list end\n", vmt);
			//if(aquire_control()) {
			//	xiv::printf("%s: FAILED to add at reserved block.\n", vmt);
			//}
		//}
		// free isn't looking so free
		if(active->free()->flags != MX_FREE) {
			find_free_extent(l);
			if(active->free()->flags != MX_FREE) {
				xiv::printf("%s: No FREE blocks\n", vmt);
				xiv::printf("%s: Allocation fail\n", vmt);
				return 0;
			}
		}
		Extent *pfree = active->free();
		if(pfree->length >= l) {
			mm = pfree->base;
			xiv::printf("%s: F-Allocated: %x @ %0lx\n", vmt, l, mm);
			set_extent(mm, l, f);
			return mm;
		}
		find_free_extent(l);
		pfree = active->free();
		if(pfree->length >= l && pfree->flags == MX_FREE) {
			mm = pfree->base;
			xiv::printf("%s: L-Allocated: %x @ %0lx\n", vmt, l, mm);
			set_extent(mm, l, f);
			return mm;
		}
		find_free_extent(0); // find last free
		if(aquire(l) == 0) {
			mm = active->free()->base;
			xiv::printf("%s: A-Allocated: %x @ %0lx\n", vmt, l, mm);
			set_extent(mm, l, f);
			find_free_extent(1);
			return mm;
		}
		xiv::printf("%s: Length Exceeded %d/%d\n", vmt, l, active->free()->length);
		return 0;
	}

	void debug_trie(ExtentTable *table, size_t iaddr, size_t index, size_t depth) {
		if(depth > 32) return;
		TrieControl &tc = table->trie[index];
		if(tc.type != TT_UNSET) {
			xiv::printf("TRIE:%03d %s ", index, tc.type_name());
			const char * so_align = "              |";
			for(size_t i = 0; i < (depth / 4); i++) xiv::putc(' ');
			switch(tc.type) {
			case TT_LEAF:
			{
				const char *eclass = "???";
				if(tc.leaf.flag < MX_LAST) {
					eclass = EXTENTCLASS[tc.leaf.flag];
				}
				xiv::printf("<%s> % 5x\n", eclass, tc.leaf.length);
				break;
			}
			case TT_MASK_1:
				depth += 1;
				xiv::putc('\n');
				for(size_t i = 0; i < 2; i++) {
					if(tc.radix.ref[i]) {
						size_t naddr = iaddr | i << (32 - depth);
						uint8_t nindex = 0xff & (tc.radix.ref[i] + index);
						if(naddr != iaddr) {
							xiv::print(so_align);
							xiv::printf("% 8x->%d\n", naddr, nindex);
						}
						debug_trie(table, naddr, nindex, depth);
					}
				}
				depth -= 1;
				break;
			case TT_MASK_2:
				depth += 2;
				xiv::putc('\n');
				for(size_t i = 0; i < 4; i++) {
					if(tc.radix.ref[i]) {
						size_t naddr = iaddr | i << (32 - depth);
						uint8_t nindex = 0xff & (tc.radix.ref[i] + index);
						if(naddr != iaddr) {
							xiv::print(so_align);
							xiv::printf("% 8x->%d\n", naddr, nindex);
						}
						debug_trie(table, naddr, nindex, depth);
					}
				}
				depth -= 2;
				break;
			case TT_MASK_3:
				depth += 3;
				xiv::putc('\n');
				for(size_t i = 0; i < 8; i++) {
					if(tc.radix.ref[i]) {
						size_t naddr = iaddr | i << (32 - depth);
						uint8_t nindex = 0xff & (tc.radix.ref[i] + index);
						if(naddr != iaddr) {
							xiv::print(so_align);
							xiv::printf("% 8x->%d\n", naddr, nindex);
						}
						debug_trie(table, naddr, nindex, depth);
					}
				}
				depth -= 3;
				break;
			case TT_LIST_3:
				depth += 4;
				xiv::putc('\n');
				for(size_t i = 0; i < 8; i++) {
					if(tc.radix.ref[i]) {
						size_t naddr = iaddr | ((0xf & (tc.radix.radix >> (4 * i))) << (32 - depth));
						uint8_t nindex = 0xff & (tc.radix.ref[i] + index);
						if(naddr != iaddr) {
							xiv::print(so_align);
							xiv::printf("% 8x->%d\n", naddr, nindex);
						}
						debug_trie(table, naddr, nindex, depth);
					}
				}
				depth -= 4;
				break;
			default:
				break;
			}
		}
	}
	void debug_table() {
		uint32_t addrc = 0;
		uint32_t last_show = 0;
		ExtentInterator extent_iter(*this, this->begin);
		Extent *prev = nullptr;
		int vis = 3;
		ExtentTable *last_table = nullptr;
		for(;extent_iter.has_next(); extent_iter++) {
			if(last_table != extent_iter.cur) {
				last_table = extent_iter.cur;
				if((vis & 1) != 1) {
					xiv::printf("%s: missing:", vmt);
					if(!(vis & 1)) xiv::printf(" FREE");
					xiv::putc(10);
					vis = 0;
				}
				xiv::printf("%s: Block: %08x/%d\n", vmt, extent_iter.cur, addrc);
				debug_trie(extent_iter.cur, 0, 0, 0);
			}
			Extent *p = &*extent_iter;
			if((extent_iter.index < 3) || (extent_iter.index > (max_ext - 3))
				|| (*prev != *p) // the index < 0 is *required* to void null deref here
				|| extent_iter.is_free() ) {
				prev = p;
				if((last_show + 1) < addrc) {
					xiv::printf(":%03d .. %03d\n", last_show + 1, addrc - 1);
				}
				last_show = addrc;
				xiv::printf(":%03d %016lx<", addrc, p->base);
				if(p->flags >= MX_LAST) {
					xiv::print("INVALID>\n");
					return;
				}
				xiv::print(EXTENTCLASS[p->flags]);
				xiv::printf(">% 8x", p->length);
				if(extent_iter.is_free()) {
					xiv::print("<-FREE");
					vis |= 1;
				}
				xiv::putc(10);
			}
			addrc++;
		}
		xiv::printf("%s: End of blocks: %d slots\n", vmt, addrc);
	}
	void debug_counts() {
		uint32_t n_free = 0;
		uint32_t n_alloc = 0;
		uint32_t n_empty = 0;
		ExtentInterator extent_iter(*this, begin);
		uint32_t index = 0;
		for(;extent_iter.has_next(); extent_iter++) {
			switch(extent_iter->flags) {
			case MX_FREE: n_free++; break;
			case MX_ALLOC: n_alloc++; break;
			case MX_EMPTY: n_empty++; break;
			default: break;
			}
			index++;
		}
		xiv::printf("Free: %d, Alloc: %d, Empty: %d\n",
				n_free, n_alloc, n_empty);
	}
};

//ExtentAllocator<PAGE_SIZE, 1, false> memalloc("sysmem");
PageAllocator phymm("sysphy");
PageAllocator vmm("sysvmm");

PageDirPtr *pdpt;
static uint32_t tablefreeblocks;
static uint32_t tablefreelock;

size_t const phyptr = ((&_kernel_start) - (&_kernel_load));

void load_memmap() {
	using namespace xiv;
	print("loading memory map...\n");
	uint32_t limit = *((uint16_t*)(phyptr + 0x500));
	MemMap *mm = (MemMap*)(phyptr + 0x800);
	for(uint32_t i = 0; i < limit; i++, mm++) {
		if(mm->start < 0x100000) {
			continue;
		}
		if(mm->size & (PAGE_SIZE - 1)) {
			print("odd block size\n");
			continue;
		}
		if(mm->type != 2) {
			printf("%x pages at %x\n", mm->size >> 12, mm->start);
			phymm.set_extent(mm->start, mm->size >> 12, (mm->type == 1) ? MX_FREE : MX_FIXED);
		}
	}
	print("done.\n");
}

void initialize() {
	xiv::print("Starting memory manager.\n");
	pdpt = &_ivix_pdpt; // get the address of the directory pointer table
	uintptr_t kv_start = (uintptr_t)&_kernel_start;
	uintptr_t kp_start = (uintptr_t)&_kernel_load;
	uintptr_t kv_end = (uintptr_t)&_kernel_end;
	uintptr_t k_pages = (kv_end - kv_start) >> 12;
	xiv::printf("Kernel %x - %x (%x)\n", kv_start, kp_start, phyptr);
	xiv::printf("PDP: %x\n", (size_t)pdpt);
	xiv::printf("Heap start: %x\n", kv_end);
	tablefreeblocks = 0;
	tablefreelock = 0;
	char *blockptr = reinterpret_cast<char*>(kv_end);
	// initial page directories come directly after kernel in physical memory
	// each directory manages 1GB of memory in 2MB chunks
	pdpt->pgdir[0] = (PageDir*)blockptr; blockptr += PAGE_SIZE;
	pdpt->pgdir_idx[0] = (PageDirIndex*)blockptr; blockptr += PAGE_SIZE;
	pdpt->pgdir[1] = (PageDir*)blockptr; blockptr += PAGE_SIZE;
	pdpt->pgdir_idx[1] = (PageDirIndex*)blockptr; blockptr += PAGE_SIZE;
	pdpt->pgdir[2] = (PageDir*)blockptr; blockptr += PAGE_SIZE;
	pdpt->pgdir_idx[2] = (PageDirIndex*)blockptr; blockptr += PAGE_SIZE;
	pdpt->pgdir[3] = (PageDir*)blockptr; blockptr += PAGE_SIZE;
	pdpt->pgdir_idx[3] = (PageDirIndex*)blockptr; blockptr += PAGE_SIZE;
	phymm.init(blockptr); blockptr += PAGE_SIZE;
	// load the physical memory map into the blocks table
	load_memmap();
	// the blocks the kernel is sitting in are definitely being used
	phymm.set_extent(kp_start, k_pages, MX_FIXED);
	phymm.allocate(8); // the page directories and indicies
	phymm.allocate(2); // the PMM and VMM control blocks
	//memalloc.init(blockptr); blockptr += PAGE_SIZE;
	vmm.init(blockptr); blockptr += PAGE_SIZE;
	vmm.set_extent(0xc0000000, 0x3fff0, MX_FREE); // the top 1GB is all usable by the kernel
	vmm.set_extent(0xc0000000, 0x100, MX_FIXED); // legacy memory area
	vmm.set_extent(kv_start, k_pages, MX_FIXED); // kernel
	vmm.allocate(8);
	vmm.allocate(2);

	uint32_t i;
	for(i = 0; i < 1; i++) {
		pdpt->pgdir[3]->set(i, i * 0x200000, Large, Writable, Present);
		pdpt->pgdir_idx[3]->set(i, 0);
	}
	for(; i < 512; i++) {
		pdpt->pgdir[3]->set(i, 0, NotPresent);
		pdpt->pgdir_idx[3]->set(i, 0);
	}
	for(int i = 0; i < 4; i++) {
		pdpt->pdtpe[i] = ( ((uint32_t)(pdpt->pgdir[i])) - phyptr ) | Present;
		xiv::printf("PDP %d: %010lx\n", i, pdpt->pdtpe[i]);
	}
	_ix_loadcr3((uint32_t)&_ivix_phy_pdpt); // reload page tables
	xiv::printf("Page directories mapped\n");
	for(int i = 0; i < 512; i++) {
		pdpt->pgdir[0]->set(i, 0, NotPresent);
		pdpt->pgdir[1]->set(i, 0, NotPresent);
		pdpt->pgdir[2]->set(i, 0, NotPresent);
		pdpt->pgdir_idx[0]->set(i, 0);
		pdpt->pgdir_idx[1]->set(i, 0);
		pdpt->pgdir_idx[2]->set(i, 0);
	}
	xiv::printf("Page directories reset\n");
}

uint64_t translate_page(uintptr_t t) {
	uint32_t ofs_dp = (t >> 30) & 0x3;
	uint32_t ofs_d = (t >> 21) & 0x1FF;
	uint32_t ofs_t = (t >> 12) & 0x1FF;
	PageDir *dirptr = pdpt->pgdir[ofs_dp];
	if(!dirptr) return 0;
	uint64_t tablebase = dirptr->entry[ofs_d];
	if(!(tablebase & Present)) return 0;
	if(tablebase & Large) {
		return (tablebase & ~0x1ffffful) | (t & 0x1ffffful);
	}
	PageTable *ptptr = pdpt->pgdir_idx[ofs_dp]->get(ofs_d);
	if(!ptptr) return 0;
	return (ptptr->pte[ofs_t] & ~0xffful) | (t & 0xffful);
}

size_t count_page_structs(uintptr_t t, size_t npg) {
	uintptr_t eos = t + PAGE_SIZE * npg;
	uint32_t ofs_dp = (t >> 30) & 0x3;
	uint32_t ofs_d = (t >> 21) & 0x1FF;
	uint32_t ofs_t = (t >> 9) & 0x1FF;
	uint32_t efs_dp = (eos >> 30) & 0x3;
	uint32_t efs_d = (eos >> 21) & 0x1FF;
	size_t res = (efs_d - ofs_d) + (efs_dp - ofs_dp);
	PageDirIndex *pdi = nullptr;
	PageTable *ptptr = nullptr;
	xiv::printf("CPS: lookup: %02x @ %08x %01x:%03x:%03x > ", npg, t & ~0x1ff, ofs_dp, ofs_d, ofs_t);
	pdi = pdpt->pgdir_idx[ofs_dp];
	uint64_t lg_flags = (Present | Large);
	if((pdpt->pgdir[ofs_dp]->entry[ofs_d] & lg_flags) == lg_flags) {
		xiv::printf("PgDir[isMapped]\n");
		return 0;
	} else {
		xiv::printf("PgDir[%x]>", pdi);
		if(pdi) {
			ptptr = pdi->get(ofs_d);
			xiv::printf("PgTab[%x]>", ptptr);
		}
		xiv::print("\n");
	}
	if(!pdi) {
		return res + 2;
	}
	if(!ptptr) {
		return res + 1;
	}
	return res;
}

static void map_page(phyaddr_t phy, uintptr_t t, uint64_t f) {
	uint32_t ofs_dp = (t >> 30) & 0x3;
	uint32_t ofs_d = (t >> 21) & 0x1FF;
	uint32_t ofs_t = (t >> 12) & 0x1FF;
	PageDir *dirptr = pdpt->pgdir[ofs_dp];
	if(f & Large) {
		xiv::printhex((size_t)&dirptr->entry[ofs_d], 32);
		dirptr->set(ofs_d, phy.m, Present, Large, (PageFlags)(f & (Writable | Usermode)));
		xiv::printf(" map_page %x = ((%x))\n", t, dirptr->entry[ofs_d]);
		_ix_loadcr3((uint32_t)&_ivix_phy_pdpt); // reset page tables
	} else {
		uint64_t tablebase = dirptr->entry[ofs_d];
		// TODO if(tablebase) not present or present large page
		if(!(tablebase & Present)) {
			xiv::printf("map_page: %01x:%03x: tablebase not present\n", ofs_dp, ofs_d);
		}
		//xiv::printf("VMM:/%x/%x/%x/", ofs_dp, ofs_d, ofs_t);
		if((tablebase & Present) && (tablebase & Large)) {
			// ignore large mapping overwrite
		} else {
			PageDirIndex *pdi = pdpt->pgdir_idx[ofs_dp];
			if(!pdi) {
				xiv::print("map_page:ERR: Table Index not exist.\n");
				return;
			}
			PageTable *ptptr = pdi->get(ofs_d);
			if(!ptptr) {
				xiv::print("map_page: add pagetable/");
				pdi->set(ofs_d, (void*)vmm.allocate_res(1, MX_ALLOC));
				ptptr = pdi->get(ofs_d);
				xiv::print("zero table/");
				auto *cpte = ptptr->pte;
				for(int x = 0; x < 512; x++) {
					cpte[x] = 0;
				}
			} else {
				//xiv::printf("table exist", ptptr, t, phy.m);
				//xiv::printhexx(ptptr->pte[ofs_t], 64); xiv::putc('>');
				//xiv::printhexx(phy.m, 64); xiv::putc(10);
			}
			if((tablebase & Present) == 0) {
				xiv::print("map pagetable\n");
				dirptr->set(ofs_d, translate_page((uintptr_t)pdi->get(ofs_d)), Present, Writable);
				_ix_loadcr3((uint32_t)&_ivix_phy_pdpt); // reset page tables
			}
			//xiv::printf("/map %0x=%0x>%0lx\n", ptptr, t, phy.m);
			ptptr->pte[ofs_t] = phy.m | Present | (f & 0x1f);
		}
	}
}

void * vmm_request(size_t sz, void* hint, uint64_t phys, uint32_t flag) {
	uint64_t mapflag = 0;
	uintptr_t bega, enda;
	uintptr_t pg_size = PAGE_SIZE;
	if(flag & RQ_RW) mapflag |= Writable;
	if(flag & RQ_LARGE) {
		mapflag |= Large;
		pg_size = PAGE_SIZE_LARGE;
	}
	if(flag & RQ_HINT) {
		bega = (uintptr_t)hint & ~(PAGE_SIZE-1);
		enda = bega + (sz & ~(PAGE_SIZE-1));
		if(sz & (PAGE_SIZE-1)) enda += PAGE_SIZE;
		if(enda <= bega) return nullptr;
		if(bega < 0x10000) { // don't allow allocations at <64k VM
			return nullptr;
		}
		vmm.set_extent(bega, (enda - bega) / PAGE_SIZE, ((flag & RQ_ALLOC) == 0) ? MX_IO : MX_ALLOC);
	} else {
		size_t pgc = (sz & ~(PAGE_SIZE-1)) / PAGE_SIZE;
		if(sz & (PAGE_SIZE-1)) pgc++;
		bega = vmm.allocate(pgc);
		enda = bega + pgc * PAGE_SIZE;
		if(enda < bega) return nullptr;
	}
	xiv::printf("vmm_request: vaddr: %0x-%0x\n", bega, enda);
	hint = (void*)bega;
	if(!(flag & RQ_LARGE)){
		size_t pgc = (enda - bega) / PAGE_SIZE;
		size_t pgsr = count_page_structs(bega, pgc);
		xiv::printf("vmm_request: struct requirement: %0x-%0x %d/%d\n", bega, enda, pgsr, pgc);
		if(pgsr) {
			Extent *preext = vmm.find_type_extent(1, MX_PRE);
			if(preext == nullptr) {
				xiv::printf("vmm_request: no prealloc exist\n");
				pgsr+= 8;
			}
			uint64_t bu = vmm.allocate(pgsr, MX_PRE);
			if(bu == 0) {
				xiv::print("vmm_request: Virtual allocation failed\n");
				return nullptr;
			}
			while(pgsr) {
				uint64_t ph = phymm.allocate(1);
				if(ph == 0) {
					xiv::print("vmm_request: Physical allocation failed\n");
					return 0;
				}
				map_page(ph, bu, Writable);
				bu += PAGE_SIZE;
				pgsr--;
			}
		}
	}
	if(flag & RQ_ALLOC) {
		do {
			uint64_t ph = phymm.allocate(1);
			if(ph == 0) {
				xiv::print("vmm_request: Physical allocation failed\n");
				return 0;
			}
			map_page(ph, bega, mapflag);
			bega += pg_size;
		} while(bega < enda);
		return hint;
	} else {
		do {
			map_page(phys, bega, mapflag);
			phys += pg_size;
			bega += pg_size;
		} while(bega < enda);
		return hint;
	}
	return nullptr;
}

int req_allocrange(void (*allocadd)(void *, uint64_t, uint32_t), void * t, uint32_t ml) {
	uint32_t mmb = (ml / PAGE_SIZE) + ((ml % PAGE_SIZE)>0? 1 : 0);
	void *bu = vmm_request(mmb * PAGE_SIZE, nullptr, 0, RQ_RW | RQ_ALLOC);
	if(!bu) return 1;
	allocadd(t, (uint64_t)bu, PAGE_SIZE * mmb);
	return 0;
}

void debug(int t) {
	switch(t) {
	case 1:
		phymm.debug_table();
		break;
	case 2:
		vmm.debug_table();
		break;
	case 3:
		//memalloc.debug_counts();
		xiv::print("sysmem allocator is disabled\n");
		break;
	default:
		//memalloc.debug_table();
		xiv::print("sysmem allocator is disabled\n");
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

} // namespace mem

extern "C"
void * kmalloc(size_t l) {
	using namespace mem;
	xiv::printf("MMA: kmalloc: %x bytes\n", l);
	return vmm_request(l, nullptr, 0, RQ_RW | RQ_ALLOC);
	if(l > 1) {
		if(l & 0xf) {
			l += 16 - (l & 0xf);
		}
		//return (void*)mem::memalloc.allocate(l);
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
	xiv::printf("kfree: %x\n", p);
	//mem::memalloc.free((uintptr_t)p);
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

void operator delete(void *d, size_t) {
	kfree(d);
}
