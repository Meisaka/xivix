constexpr unsigned release_year=
/* ***
 * random.cpp - Randomness and entropy processing
 * Copyright (c) */2021/* Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */;

#include "kio.hpp"

template<typename T>
T rotleft(T v, int n) {
	return (v << n) | (v >> ((sizeof(T) * 8) - n));
}
namespace Crypto {

class ChaCha {
public:
	uint32_t state[16];

	void init(const void *key, uint32_t block, const void *nonce) {
		state[0] = 0x61707865;
		state[1] = 0x3320646e;
		state[2] = 0x79622d32;
		state[3] = 0x6b206574;
		const uint32_t *ukey = (const uint32_t *)key;
		const uint32_t *unonce = (const uint32_t *)nonce;
		state[4] = ukey[0]; state[5] = ukey[1];
		state[6] = ukey[2]; state[7] = ukey[3];
		state[8] = ukey[4]; state[9] = ukey[5];
		state[10] = ukey[6]; state[11] = ukey[7];
		state[12] = block;
		state[13] = unonce[0];
		state[14] = unonce[1];
		state[15] = unonce[2];
		stir();
	}
#define _ChaCha_Quart(a,b,c,d) \
	lstate[a] += lstate[b]; lstate[d] ^= lstate[a]; \
	lstate[d] = rotleft(lstate[d], 16); \
	lstate[c] += lstate[d]; lstate[b] ^= lstate[c]; \
	lstate[b] = rotleft(lstate[b], 12); \
	lstate[a] += lstate[b]; lstate[d] ^= lstate[a]; \
	lstate[d] = rotleft(lstate[d], 8); \
	lstate[c] += lstate[d]; lstate[b] ^= lstate[c]; \
	lstate[b] = rotleft(lstate[b], 7);
	void stir() {
		uint32_t lstate[16];
		for(size_t i = 0; i < 16; i++) {
			lstate[i] = state[i];
		}
		for(size_t i = 0; i < 20; i++) {
			_ChaCha_Quart(0,4, 8,12);
			_ChaCha_Quart(1,5, 9,13);
			_ChaCha_Quart(2,6,10,14);
			_ChaCha_Quart(3,7,11,15);

			_ChaCha_Quart(0,5,10,15);
			_ChaCha_Quart(1,6,11,12);
			_ChaCha_Quart(2,7, 8,13);
			_ChaCha_Quart(3,4, 9,14);
		}
		for(size_t i = 0; i < 16; i++) {
			state[i] += lstate[i];
			lstate[i] = 0;
		}
	}
	void mix(const uint32_t *in, size_t len) {
		len &= ~3;
		while(len) {
			for(size_t i = 0; i < 8 && len; i++, len -= 4) {
				state[4 + i] ^= in[i];
			}
			stir();
		}
	}
};

ChaCha prng1;
uint32_t prng_index = 0;

void Init() {
	const char *xivix_key = /*
	*0123456789abcdef0123456789abcdef*/
	"Xivix Operating System Software";
	uint32_t n[] = { 0xe5908de5, 0x9d82e383, 0xa6e382ad };
	prng1.init(xivix_key, release_year, n);
}

void Mix(const uint32_t *ent, size_t count) {
	prng1.mix(ent, count * 4);
}

} // namespace

extern "C" uint32_t ixa4random() {
	uint32_t index = Crypto::prng_index;
	index++;
	if(index >= 4) {
		index = 0;
		Crypto::prng1.stir();
	}
	Crypto::prng_index = index;
	return Crypto::prng1.state[index];
}

