#ifndef ARRAYLIST_HAI
#define ARRAYLIST_HAI

#include "ktypes.hpp"

template<class T, int BLOCKSIZE>
class ArrayList {
	struct ArrayUnit {
		ArrayUnit *next;
		T first;
	};
	constexpr static uint32_t blockent = (BLOCKSIZE - sizeof(ArrayUnit)) / sizeof(T);
	struct Iterator {
		ArrayUnit *block;
		size_t index;
		T* operator*() const {
			return (&(block->first)) + index;
		}
		void operator++(int) {
			if(!block) return;
			index++;
			if(index >= blockent) {
				index = 0;
				block = block->next;
			}
		}
		operator bool() {
			return block && index < blockent;
		}
	};
public:
	typedef struct Iterator iterator;
	ArrayUnit *pbase;
	ArrayList() {
		pbase = nullptr;
	}
	void add(void *block) {
		ArrayUnit *pblk = (ArrayUnit*)block;
		pblk->next = nullptr;
		if(!pbase) {
			pbase = pblk;
		} else {
			ArrayUnit *p = pbase;
			while(p->next) {
				p = p->next;
			}
			p->next = pblk;
		}
	}
	iterator begin() {
		return {pbase, 0};
	}
};

#endif
