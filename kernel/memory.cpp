
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
	extern uint8_t const _ivix_phy_pdpt;
	extern uint32_t const _ivix_pdpt;
	extern char const _kernel_start;
	extern char const _kernel_load;
	}

	void map_page(phyaddr_t phy, uintptr_t t, uint32_t f) {
		uint32_t ofs_dp = (t >> 30) & 0x3;
		uint32_t ofs_d = (t >> 21) & 0x1FF;
		uint32_t ofs_t = (t >> 12) & 0x1F;
		size_t const phyptr = ((&_kernel_start) - (&_kernel_load));
		PageDirPtr *pdpt = (PageDirPtr*)((&_ivix_phy_pdpt) + phyptr);
		xiv::printf("Kernel %x - %x\n", &_kernel_start, &_kernel_load);
		xiv::printf("PDP: %x (%x)\n", (size_t)pdpt, phyptr);
		uint32_t dirbase = ((uint32_t)pdpt->pdtpe[ofs_dp]);
		dirbase &= 0xfffff000;
		PageDir *dirptr = (PageDir*)(dirbase + phyptr);
		if(f & MAP_LARGE) {
			xiv::printf("map_page large was (%x)\n", dirptr->pde[ofs_d]);
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
	return nullptr;
}

extern "C"
void * krealloc(void * p, size_t l) {
	return nullptr;
}

extern "C"
void kfree(void *) {
}

