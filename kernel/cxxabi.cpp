

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


