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

}

typedef mem::PhysicalAddress phyaddr_t;

namespace mem {

	enum MAP_FLAGS : uint32_t {
		MAP_RW = 0x2,
		MAP_USER = 0x4,
		MAP_EXEC = 0x8,
		MAP_LARGE = 0x80,
	};
	enum REQUEST_FLAGS : uint32_t {
		RQ_RW = 0x2,
		RQ_HINT = 0x1,
	};

	void initialize();
	void debug(int);
	uint64_t translate_page(uintptr_t t);
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

