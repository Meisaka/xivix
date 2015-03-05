/* ***
 * cxxabi.cpp - C++ ABI functions for kernel use
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


extern "C" {

#define ATEXIT_MAX_FUNCS 128

typedef unsigned uarch_t;

struct atexit_func_entry_t {
	void (*destructor_func)(void *);
	void *obj_ptr;
	void *dso_handle;
};

int __cxa_atexit(void (*f)(void *), void *obj, void *dso);
void __cxa_finalize(void *f);

atexit_func_entry_t __atexit_funcs[ATEXIT_MAX_FUNCS];
uarch_t __atexit_func_count = 0;

void *__dso_handle = nullptr;

int __cxa_atexit(void (*f)(void *), void *obj, void *dso)
{
	if(__atexit_func_count >= ATEXIT_MAX_FUNCS) return -1;
	uarch_t x = __atexit_func_count++;
	__atexit_funcs[x].destructor_func = f;
	__atexit_funcs[x].obj_ptr = obj;
	__atexit_funcs[x].dso_handle = dso;
	return 0;
}

void __cxa_finalize(void *f)
{
	uarch_t i = __atexit_func_count;
	if(!f) {
		while(i--) {
			if(__atexit_funcs[i].destructor_func) {
				(*__atexit_funcs[i].destructor_func)(__atexit_funcs[i].obj_ptr);
			}
		}
		return;
	}
	while(i--) {
		if(__atexit_funcs[i].destructor_func == f) {
			(*__atexit_funcs[i].destructor_func)(__atexit_funcs[i].obj_ptr);
			__atexit_funcs[i].destructor_func = nullptr;
		}
	}
}

} // extern "C"


