
#include "memory.hpp"

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

	extern "C" PageDirPtr *_ivix_pdpt;
	extern "C" void * const _kernel_start;

	void map_page(phyaddr_t phy, void * t, uint32_t f) {
		uint32_t ofs_dp = (((uint32_t)t) >> 30) & 0x3;
		uint32_t ofs_d = (((uint32_t)t) >> 21) & 0x1FF;
		uint32_t ofs_t = (((uint32_t)t) >> 12) & 0x1F;
		phy;
		(void)t;
		(void)f;
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

