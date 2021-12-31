/* ***
 * memory.hpp - functions to map, allocate, and free memory and pages
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#ifndef MEMORY_HAI
#define MEMORY_HAI

#include "ktypes.hpp"

namespace mem {

constexpr size_t const PAGE_SIZE = 0x1000;
constexpr size_t const PAGE_SIZE_LARGE = 0x200000;

struct PhysicalAddress {
	uint64_t m;
	PhysicalAddress(uint64_t a) : m(a) {}
	PhysicalAddress(PhysicalAddress &&p) : m(p.m) {}
	PhysicalAddress(const PhysicalAddress &p) : m(p.m) {}
	PhysicalAddress& operator=(PhysicalAddress &&p) { m = p.m; return *this; }
	PhysicalAddress& operator=(const PhysicalAddress &p) { m = p.m; return *this; }
	PhysicalAddress& operator=(uint64_t a) { m = a; return *this; }
};
typedef PhysicalAddress phyaddr_t;

void ref_destroy(size_t r);
int ref_add(size_t r);
size_t ref_create(void *, size_t);
int ref_lock(size_t r);
int ref_unlock(size_t r);
void * ref_getlockptr(size_t r);

template<typename T>
class lockref final {
private:
	size_t r_id;
	T *hptr;
public:
	lockref() : r_id(0), hptr(nullptr) {}
	lockref(size_t i) : r_id(i) {
		if(r_id) {
			mem::ref_add(r_id);
			mem::ref_lock(r_id);
			hptr = (T*)mem::ref_getlockptr(r_id);
		} else {
			hptr = nullptr;
		}
	}
	~lockref() {
		if(r_id) {
			mem::ref_unlock(r_id);
			mem::ref_destroy(r_id);
		}
	}
	lockref(const lockref<T>&) = delete;
	lockref(lockref<T>&&) = delete;
	lockref& operator=(const lockref<T>&) = delete;
	lockref& operator=(lockref<T>&&) = delete;
	T& operator*() {
		return *hptr;
	}
	T& operator->() {
		return *hptr;
	}
};

template<typename T>
class ref final {
private:
	size_t r_id;
public:
	ref() : r_id(0) {}
	~ref() {
		if(r_id) mem::ref_destroy(r_id);
	}
	ref(T *p) {
		r_id = mem::ref_create(p, sizeof(T));
	}
	ref(const ref<T> &h) {
		mem::ref_add(h.r_id);
		if(r_id) mem::ref_destroy(r_id);
		r_id = h.r_id;
	}
	ref(ref<T> &&h) {
		r_id = h.r_id;
		h.r_id = 0;
	}
	ref<T>& operator=(const ref<T> &h) {
		mem::ref_add(h.r_id);
		if(r_id) mem::ref_destroy(r_id);
		r_id = h.r_id;
		return *this;
	}
	ref<T>& operator=(ref<T> &&h) {
		if(r_id) mem::ref_destroy(r_id);
		r_id = h.r_id;
		h.r_id = 0;
		return *this;
	}
	lockref<T> operator*() {
		return lockref<T>(r_id);
	}
};

enum REQUEST_FLAGS : uint32_t {
	RQ_HINT = 0x1,
	RQ_RW = 0x2,
	RQ_ALLOC = 0x4,
	RQ_EXEC = 0x8,
	RQ_LARGE = 0x80,
	RQ_WCD = 0x400,
	RQ_RCD = 0x800,
};

void initialize();
void debug(int);
uint64_t translate_page(uintptr_t t);
//void map_page(phyaddr_t, uintptr_t, uint32_t);
void * vmm_request(size_t, void*, uint64_t, uint32_t);

} // namespace mem

extern "C" {
void * kmalloc(size_t l);
void * krealloc(void *, size_t);
void kfree(void *);
}

#endif
