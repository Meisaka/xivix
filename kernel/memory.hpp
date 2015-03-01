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
	};

	void map_page(phyaddr_t, void*, uint32_t);
	void * alloc_pages(size_t, uint32_t);
	void * request(size_t, void*, uint64_t, uint32_t);
}

extern "C" {
void * kmalloc(size_t l);
void * krealloc(void *, size_t);
void kfree(void *);
}

#endif

