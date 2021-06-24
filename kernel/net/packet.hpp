/* ***
 * Copyright (c) 2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */
#ifndef NET_PACKET_HAI
#define NET_PACKET_HAI

#include "ktypes.hpp"
#include "inet.hpp"
#include "kio.hpp"

namespace net {

struct Packet : RefCountable {
	uint8_t *buf = 0;
	uint32_t size;
	uint8_t & operator[](size_t index) {
		return buf[index];
	}
	uint8_t operator[](size_t index) const {
		return buf[index];
	}
	uint16_t w(size_t index) const {
		return *(uint16_t*)(buf + index);
	}
	uint16_t nw(size_t index) const {
		return _ix_ldnw(buf + index);
	}
	uint16_t & w(size_t index) {
		return *(uint16_t*)(buf + index);
	}
	uint32_t i(size_t index) const {
		return (*(uint16_t*)(buf + index)) | ((*(uint16_t*)(buf + index + 2)) << 16);
	}
	uint32_t & i(size_t index) {
		return *(uint32_t*)(buf + index);
	}
	uint32_t psudo_check() {
		constexpr uint32_t ip_start = 14;
		uint32_t ip_precheck = buf[ip_start+9];
		uint32_t checkhi = 0;
		checkhi += buf[ip_start+12]; ip_precheck += buf[ip_start+13];
		checkhi += buf[ip_start+14]; ip_precheck += buf[ip_start+15];
		checkhi += buf[ip_start+16]; ip_precheck += buf[ip_start+17];
		checkhi += buf[ip_start+18]; ip_precheck += buf[ip_start+19];
		ip_precheck += checkhi << 8;
		return ip_precheck;
	}
};

void debug_arp();
void debug_packet();
void debug_junk();
void request_packet(Ref<Packet> &pkt);
void init();
void runsched();
void enqueue(Ref<Packet> pkt);
void resolve_with_arp(const in_addr *nani);
void packet_send_bc_arp(Ref<Packet> pkt, const in_addr *nani);
bool packet_fill_ip(Ref<Packet> &pkt, const sockaddr *dst, size_t payload_len, uint8_t proto);
bool packet_fill_udp(Ref<Packet> pkt, uint16_t sport, const sockaddr *dst, uint16_t msg_size);
void send_via_udp(uint16_t sport, const sockaddr *dst, const uint8_t * msg, uint32_t msg_size);
void send_string_udp(uint16_t sport, const sockaddr *dst, const char * msg);

} // namespace net

#endif

