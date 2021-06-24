/* ***
 * Copyright (c) 2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */
#ifndef NET_INET_HAI
#define NET_INET_HAI

#include "ktypes.hpp"

namespace net {

enum ETHERTYPE : uint16_t {
	ETH_IP4 = 0x800,
	ETH_ARP = 0x806,
	ETH_IP6 = 0x86DD,
};

struct mac_addr {
	uint8_t m[6];
	void set(const uint8_t *a) {
		m[0] = a[0]; m[1] = a[1];
		m[2] = a[2]; m[3] = a[3];
		m[4] = a[4]; m[5] = a[5];
	}
	void set(const mac_addr *a) {
		set(a->m);
	}
};

union phy_addr {
	mac_addr mac;
	uint8_t b[16];
	uint16_t w[8];
};

struct in_addr {
	uint32_t s_addr;
	void set(const uint8_t *a) {
		s_addr = *(const uint32_t*)a;
	}
};

struct sockaddr {
	uint16_t family;
	uint16_t port;
	union {
		in_addr v4;
		uint8_t dat[28];
	};
};

} // namespace net

#endif

