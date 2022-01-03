/* ***
 * Copyright (c) 2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "packet.hpp"
#include "ktext.hpp"
#include "kio.hpp"
#include "memory.hpp"
#include "dev/hwtypes.hpp"
#include "arraylist.hpp"

namespace hw {
	extern NetworkMAC *ethdev;
}
namespace net {
using xiv::printf;

void print_mac(const uint8_t * mac) {
	printf("%02x%02x.%02x%02x.%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
void print_mac(const mac_addr &mac) {
	print_mac(mac.m);
}
void print_mac(const mac_addr *mac) {
	print_mac(mac->m);
}
void print_ip4(const uint8_t * addr) {
	printf("%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
}
void print_ip4(const in_addr &addr) {
	print_ip4((const uint8_t*)&addr);
}
void print_ip6(const uint8_t * addr) {
	printf("%x", (addr[0] << 8) | addr[1]);
	for(size_t i = 1; i < 8; i++) {
		printf(":%x", (addr[i*2] << 8) | addr[i*2 + 1]);
	}
}
void print_data(const void * data, uint16_t len) {
	size_t i = 0;
	const uint8_t *buf = (const uint8_t *)data;
	constexpr size_t col = 32;
	while(i < len) {
		if((i & (col-1)) == 0) printf("%04x:", i);
		printf(" %02x", buf[i]);
		i++;
		if((i & (col-1)) == 0) xiv::putc(10);
	}
	if((i & (col-1))) xiv::putc(10);
}

struct arp_entry {
	in_addr ipa;
	mac_addr lla;
	uint16_t info;
	void set(const in_addr &ip) {
		ipa.s_addr = ip.s_addr;
	}
};

enum ARP_TABLE_INFO : uint16_t {
	ARPT_UNKNOWN = 0x0000,
	ARPT_KNOWN = 0x2000,
	ARPT_PASSIVE = 0x4000,
	ARPT_AFFIRM = 0x6000,
	ARPT_STATIC = 0x8000,
	ARPT_ACTIVE = 0xa000,
	ARPT_CONFLICT = 0xc000,
	ARPT_PROXY = 0xe000,
	ARPT_STATUS = 0xe000,
	ARPT_COUNT = 0x0fff,
	ARPT_PRESENT = 0x1000,
};
struct ARP_layer {
	uint32_t size = 0;
	arp_entry *table;
	uint32_t limit = 0;

	uint32_t add() {
		size_t i = 0;
		for(i = 0; i < size; i++) {
			if(!(table[i].info & ARPT_PRESENT)) {
				goto table_set;
			}
		}
		if(size >= limit) return limit;
		size++;
table_set:
		table[i].info = 0;
		return i;
	}
	uint32_t find_ip(const in_addr &p) const {
		size_t i = 0;
		for(i = 0; i < size; i++) {
			if(!(table[i].info & ARPT_PRESENT)) continue;
			if(table[i].ipa.s_addr == p.s_addr) break;
		}
		return i;
	}
} arp_table;

struct Timer { // TODO move and expand this!
	uint32_t next;
	bool expired() const {
		return ((int)_iv_int_n - (int)next) >= 0;
	}
	void add(uint32_t t) {
		next += t;
	}
	void fromnow(uint32_t t) {
		next = _iv_int_n + t;
	}
};
Timer arp_tc;
Timer net_tc;
Timer dh_tc;
in_addr system_ip;

void assign_ip4(const in_addr &ip, const in_addr &mask) {
	(void)mask; // TODO
	if(ip.s_addr == system_ip.s_addr) return;
	uint32_t arp_index = arp_table.find_ip(ip);
	if(arp_index >= arp_table.size) {
		arp_index = arp_table.add();
	}
	if(arp_index >= arp_table.size) {
		printf("assign_ip: no ARP table space FIX THIS!\n"); // TODO
	}
	arp_entry &arp = arp_table.table[arp_index];
	if((arp.info & ARPT_STATUS) != ARPT_ACTIVE) {
		arp.info = ARPT_PRESENT + ARPT_STATIC + 2;
		hw::ethdev->getmediaaddr(arp.lla.m);
	}
	arp.ipa.s_addr = ip.s_addr;
	system_ip.s_addr = ip.s_addr;
}

struct DHCP_Client {
	uint32_t transaction; // a unique id
	uint32_t runtime; // seconds!
	uint32_t subtime; // some fraction of second!
	uint32_t runstate; // finite state machine~ ^_^
	uint32_t timeout; // how long before we retry
	uint32_t retry; // how many attempts before we give up
	static constexpr size_t max_offers = 4; // why 4? because I feel like it
	// save whole packets, this is rather excessive
	// it allows the client to confirm parameters are the same
	// as well as defer selection of an offer until the end of the offer period
	// ... as if I want to do that, so... maybe FIXME ?
	Ref<Packet> offers[max_offers];
	uint32_t offer_count;
	phy_addr link_addr; // how do we call ourselves?
	in_addr lease_addr; // our lovely leased address
	in_addr next_addr; // the "next" server (booting or access)
	in_addr serv_addr; // where we got the lease from
	uint32_t lease_time; // how long we think is on our lease
	uint32_t lease_t1;
	uint32_t lease_t2;

	static const sockaddr dest_addr; // servers are 67
	// clients are port 68
	DHCP_Client();
	~DHCP_Client() {}
	void start();
	void run();
	void verify();
	void state(uint32_t newstate);
	void recv(Ref<Packet> msg, size_t ofs, size_t len);
	void send_packet(uint8_t phase, const in_addr *reqip, const in_addr *srvip, const in_addr *dstip);
	enum DHCP_TYPE : uint8_t {
		DHCPDISCOVER = 1,
		DHCPOFFER = 2,
		DHCPREQUEST = 3,
		DHCPDECLINE = 4,
		DHCPACK = 5,
		DHCPNAK = 6,
		DHCPRELEASE = 7,
		DHCPINFORM = 8,
	};
	struct OptionAdder {
		size_t where;
		size_t soo;
		Packet &pk;
		OptionAdder(Packet &p, size_t ofs) : where{ofs}, pk{p} {}
		OptionAdder& operator()(uint8_t op) {
			pk[where] = op;
			soo = where + 1;
			where += 2;
			pk[soo] = 0;
			return *this;
		}
		template<class T>
		void v(const T *v) {
			pk[soo] += sizeof(T);
			for(size_t i = 0; i < sizeof(T); i++) {
				pk.buf[where + i] = ((const uint8_t *)v)[i];
			}
			where += sizeof(T);
		}
		template<class T>
		void v(const T v) {
			pk[soo] += sizeof(T);
			for(size_t i = 0; i < sizeof(T); i++) {
				pk.buf[where + i] = ((const uint8_t *)&v)[i];
			}
			where += sizeof(T);
		}
	};
} dhcpc;

const sockaddr DHCP_Client::dest_addr {0, 0x4300, 0xffffffff}; // servers are 67
DHCP_Client::DHCP_Client() :
	runtime{0}, subtime{0},
	runstate{0},
	offer_count{0}, lease_time{0}
{}

void DHCP_Client::start() {
	state(1);
}

void DHCP_Client::state(uint32_t newstate) {
	runstate = newstate;
	bool reset = false;
	switch(runstate) {
	case 0:
		printf("dhcp: shutting down\n");
		reset = true;
		break;
	case 4:
		timeout = 300;
		reset = true;
		// fallthrough
	case 1:
		subtime = 0;
		runtime = 0;
		reset = true;
		lease_addr.s_addr = 0; // full restarts means lease invalid
		assign_ip4( {0},{0} );; // unassign! o.o;;
		break;
	case 2:
		subtime = 0;
		runtime = 0;
		reset = true;
		break;
	case 3:
	case 5:
		timeout = runtime + 5;
		break;
	case 6:
		timeout = runtime + 30;
		break;
	case 8:
		runtime = 0;
		subtime = 0;
		timeout = 1 << retry;
		break;
	default:
		break;
	}
	if(reset) {
		for(size_t i = 0; i < max_offers; i++)
			offers[i].reset();
		offer_count = 0;
	}
}

void DHCP_Client::verify() {
	if(offer_count == 0) {
		if(!lease_addr.s_addr) { // not renewing
			printf("dhcp:verify: no offers\n");
			if(++retry > 6) state(0);
			else state(2); // once more!
			return;
		}
		// try to renew a lease!
		in_addr *target = &serv_addr;
		// renew vs rebind logic
		if(lease_time <= lease_t2) target = nullptr;
		send_packet(DHCPREQUEST, &lease_addr, nullptr, target);
		return;
	}
	Ref<Packet> sel_offer;
	uint32_t selected_val = 0;
	for(size_t i = 0; i < offer_count; i++) {
		uint32_t val = offers[i]->i(4);
		if(val > selected_val) {
			sel_offer = offers[i];
			selected_val = val;
		}
	}
	in_addr raddr { sel_offer->i(sel_offer->w(0) + 16) };
	in_addr sid { sel_offer->i(8) };
	send_packet(DHCPREQUEST, &raddr, &sid, nullptr);
	state(5);
}

void DHCP_Client::run() {
	++subtime;
	if(subtime >= 10) {
		runtime++;
		subtime = 0;
		if(lease_time && (lease_time != 0xffffffff)) {
			lease_time--;
		}
	}
	switch(runstate) {
	case 1:
		if(runtime > 10) {
			state(2);
			transaction = ixa4random();
			retry = 0;
		}
		break;
	case 2:
		send_packet(DHCPDISCOVER, nullptr, nullptr, nullptr);
		state(3);
		break;
	case 3: // wait OFFER
		if(runtime >= timeout) {
			verify();
		}
		break;
	case 4: // wait ERROR
		if(runtime > timeout) {
			state(1);
		}
		break;
	case 5: //  wait ACK
		if(runtime >= timeout) {
			if(++retry > 6) {
				state(1); // restart
				break;
			}
			verify();
		}
		break;
	case 6: // ACK'd verify
		if(runtime >= timeout) {
			state(7);
		}
		// fallthrough
	case 7: // wait lease
		if(lease_time < lease_t1) {
			retry = 0;
			transaction = ixa4random();
			state(8);
		}
		break;
	case 8: // renew/rebind interval
		if(lease_time == 0) { // expired!
			state(1);
			break;
		}
		if(runtime >= timeout) {
			if(++retry > 6) {
				retry = 0;
				transaction = ixa4random();
				state(8);
			}
			verify();
		}
		break;
	default:
		return;
	}
}

void DHCP_Client::recv(Ref<Packet> msg, size_t ofs, size_t len) {
	uint32_t xid = msg->i(ofs + 4);
	if(len < 240) return; // too smol
	if(xid != transaction) return; // ignore requests that aren't for us
	if((msg->i(ofs) & 0xffffff) != 0x00060102) return; // begone nonsense!
	if((msg->nw(ofs + 10) & 0x7fff) != 0) return; // we don't want your weird flags
	mac_addr lla;
	hw::ethdev->getmediaaddr(lla.m);
	if(memeq(&lla, msg->buf + ofs + 28, 6)) return; // not our address? lolwut?
	uint16_t secs = msg->nw(ofs + 8);
	uint8_t mtype = 0;
	if(msg->i(ofs + 236) != _ix_bswapi(0x63825363)) {
		return; // TODO accept BOOTP... maybe...
	}
	uint32_t has_options = 0;
	in_addr sid { 0 };
	enum HAS_OPTIONS {
		HAS_LEASE_TIME = 1,
		HAS_SUBNET = 1<<1, // code 1
		HAS_ROUTER = 1<<2, // code 3
		HAS_DNS = 1<<3, // code 6
		HAS_TIMEOFS = 1<<4, // code 2
		HAS_TIMESRV = 1<<5, // code 4
		HAS_DNAME = 1<<6, // code 15
		HAS_ROUTERDIS = 1<<7, // code 31
		HAS_SID = 1<<8, // code 54
	};
	constexpr uint32_t min_options = HAS_LEASE_TIME | HAS_SUBNET | HAS_ROUTER | HAS_DNS | HAS_SID;
	const size_t eof = len + ofs;
	for(size_t i = ofs + 240; i < eof;) {
		uint8_t oc = msg->buf[i++];
		if(oc == 0) continue;
		if(oc == 0xff) break;
		uint8_t ocl = msg->buf[i++];
		if(i + ocl > eof) break;
		switch(oc) {
		case 1:
			if(ocl != 4) return; // wrong size!
			has_options |= HAS_SUBNET;
			break;
		case 2:
			if(ocl != 4) return; // wrong size!
			has_options |= HAS_TIMEOFS;
			break;
		case 3:
			if(ocl < 4 || (ocl & 3)) return; // not a multiple of 4!
			has_options |= HAS_ROUTER;
			break;
		case 4:
			if(ocl < 4 || (ocl & 3)) return; // not a multiple of 4!
			has_options |= HAS_TIMESRV;
			break;
		case 6:
			if(ocl < 4 || (ocl & 3)) return; // not a multiple of 4!
			has_options |= HAS_DNS;
			break;
		case 15:
			if(ocl < 1) return; // wuteven
			has_options |= HAS_DNAME;
			break;
		case 31:
			if(ocl != 1) return; // yickh
			has_options |= HAS_ROUTERDIS;
			break;
		case 51:
			if(ocl != 4) return; // yeet!
			has_options |= HAS_LEASE_TIME;
			break;
		case 53:
			if(ocl != 1) return; // yeet!
			mtype = msg->buf[i];
			if(mtype < 2 || mtype > 8) return; // not touching that
			break;
		case 54:
			if(ocl != 4) return; // yeet!
			has_options |= HAS_SID;
			sid.s_addr = msg->i(i);
			break;
		}
		i += ocl;
	}
	if((has_options & min_options) != min_options) return;
	if(runstate == 3 && mtype != DHCPOFFER) return;
	if((runstate == 5 || runstate == 8) && mtype == DHCPNAK) { // OOF
		state(1);
		printf("dhcp:NACK restarting\n");
		return;
	}
	if((runstate == 5 || runstate == 8) && mtype != DHCPACK) return;
	//printf("dhcp:IN: %d<>%d.%d %08x", secs, runtime, subtime, has_options);
	//if(mtype > 0) printf(" type:%d", mtype);
	//xiv::print(" ip:");
	in_addr yip { msg->i(ofs + 16) };
	in_addr sip { msg->i(ofs + 20) };
	//print_ip4(yip); // yi
	//xiv::print(" next:");
	//print_ip4(sip); // si
	//xiv::print("\n");
	if(runstate == 3) {
		if(offer_count < max_offers) {
			msg->w(0) = (uint16_t)(ofs);
			msg->w(2) = (uint16_t)(len);
			msg->i(4) = has_options;
			msg->i(8) = sid.s_addr;
			offers[offer_count++] = msg;
		}
	} else if(runstate == 5 || runstate == 8) {
		for(size_t i = 0; i < max_offers; i++)
			offers[i].reset();
		offer_count = 0;
		lease_addr = yip;
		next_addr = sip;
		serv_addr = sid;
		in_addr ymask;
		in_addr router;
		for(size_t i = ofs + 240; i < eof;) {
			uint8_t oc = msg->buf[i++];
			if(oc == 0) continue;
			if(oc == 0xff) break;
			uint8_t ocl = msg->buf[i++];
			if(i + ocl > eof) break;
			switch(oc) {
			case 1: // subnet 4 bytes
				ymask.s_addr = msg->i(i);
				break;
			case 2: // timeoffset 4 bytes
				break;
			case 3: // routers 4n bytes
				router.s_addr = msg->i(i);
				break;
			case 4: // time server 4 bytes
				break;
			case 6: // DNS servers 4n bytes
				break;
			case 15: // DNS domain n bytes
				break;
			case 31: // router discover 1 byte
				break;
			case 51: // lease time
				lease_time = _ix_bswapi(msg->i(i));
				break;
			}
			i += ocl;
		}
		lease_t1 = lease_time >> 1;
		lease_t2 = lease_time >> 3;
		assign_ip4(yip, ymask);
		(void)router; // TODO
		if(runstate == 5) { // only for first assignment
			printf("dhcp: assigned: ");
			print_ip4(yip); // yi
			printf(" lease %d sec\n", lease_time);
		}
		state(6);
	}
}

void DHCP_Client::send_packet(uint8_t phase, const in_addr *reqip, const in_addr *srvip, const in_addr *dstip) {
	Ref<Packet> pkt;
	size_t sop = 42; // FIXME not to have to do this... error prone
	request_packet(pkt);
	if(!pkt) return; // FIXME if we fail...
	Packet &pk = *pkt;
	pk.i(sop) = 0x00060101; // this is a request via ethernet, nothing else matters ;)
	pk.i(sop + 4) = transaction; // this value is opaque, so we don't care about byte order
	pk.w(sop + 8) = _ix_bswapw(runtime);
	pk.w(sop + 10) = dstip ? 0 : 0x0080; // set broadcast flag if not unicast
	pk.i(sop + 12) = (dstip && reqip) ? reqip->s_addr : 0; // fill ci for renewals
	pk.i(sop + 16) = 0; // yi
	pk.i(sop + 20) = 0; // si
	pk.i(sop + 24) = 0; // gi
	hw::ethdev->getmediaaddr(pk.buf + sop + 28); // FIXME
	pk.w(sop + 34) = 0; // hw address padding
	pk.i(sop + 36) = 0;
	pk.i(sop + 40) = 0;
	for(size_t i = 0; i < (128 + 64); i++) { // clear these fields
		pk[sop + 44 + i] = 0;
	}
	constexpr uint16_t dhc_size = 236;
	pk.i(sop + 236) = _ix_bswapi(0x63825363);
	OptionAdder opt(pk, sop + dhc_size + 4);
	opt(53).v(phase);
	if(!dstip) { // if unicasting, we don't add these options
		if(reqip) { // add requested IP from another message
			opt(50).v(*reqip); // requested address
		}
		if(srvip) { // add a server identifier from another message
			opt(54).v(*srvip); // server id
		}
	}
	opt(55).v<uint8_t>(1);
	opt.v<uint8_t>(2);
	opt.v<uint8_t>(3);
	opt.v<uint8_t>(4);
	opt.v<uint8_t>(5);
	opt.v<uint8_t>(6);
	opt.v<uint8_t>(15);
	opt.v<uint8_t>(28);
	opt.v<uint8_t>(31);
	opt.v<uint8_t>(42);
	opt.v<uint8_t>(51);
	opt.v<uint8_t>(54);
	opt(57).v<uint16_t>(_ix_bswapw(1400));
	pk[opt.where++] = 0xff;
	uint16_t v_size = opt.where - sop;
	if(dstip) {
		sockaddr udst_addr { dest_addr.family, dest_addr.port, *dstip };
		if(packet_fill_udp(pkt, 68, &udst_addr, v_size)) return;
	} else {
		if(packet_fill_udp(pkt, 68, &dest_addr, v_size)) return;
	}
	hw::ethdev->transmit(pkt); // TODO better way to send packets
}

void runarp() {
	for(size_t i = 0; i < arp_table.size; i++) {
		arp_entry &arp = arp_table.table[i];
		if(!(arp.info & ARPT_PRESENT)) continue;
		Ref<Packet> myarp;
		switch(arp.info & ARPT_STATUS) {
		case ARPT_UNKNOWN:
		case ARPT_PASSIVE:
			if((arp.info & ARPT_COUNT) > 0) {
				request_packet(myarp);
				if(!myarp) continue;
				packet_send_bc_arp(myarp, &arp.ipa);
			}
			arp.info--;
			break;
		case ARPT_STATIC:
			if((arp.info & ARPT_COUNT) > 0) {
				request_packet(myarp);
				if(!myarp) continue;
				packet_send_bc_arp(myarp, &arp.ipa);
				arp.info--;
			} else {
				arp.info |= ARPT_KNOWN;
			}
			break;
		default:
			continue;
		}
	}
}

void resolve_with_arp(const in_addr *nani) {
	uint32_t q = arp_table.find_ip(*nani);
	if(q >= arp_table.size) { // does not exist yet
		q = arp_table.add();
		if(q >= arp_table.size) {
			printf("arp: can not resolve, no space in table\n");
			return; // resolve failed
		}
		arp_entry &arp = arp_table.table[q];
		arp.info = ARPT_PRESENT + 3; // unknown status
		arp.set(*nani);
		return;
	}
	arp_entry &arp = arp_table.table[q];
	switch(arp.info & ARPT_STATUS) {
	case ARPT_UNKNOWN: // these already trigger packets
	case ARPT_PASSIVE:
	case ARPT_STATIC:
		break;
	case ARPT_PROXY: // these require imply we already resolved them
	case ARPT_ACTIVE:
		return;
	case ARPT_KNOWN:
		arp.info = ARPT_PRESENT + 1; // force re-arp
		return;
	case ARPT_AFFIRM: // attempt to affirm passive learned entry
		arp.info = ARPT_PRESENT | (arp.info & (~ARPT_STATUS));
		break;
	case ARPT_CONFLICT: // retry conflict resolution
		arp.info = (ARPT_PRESENT | ARPT_STATIC) + 5;
		break;
	}
	if((arp.info & ARPT_COUNT) <= 1) {
		arp.info += 3;
	}
}

void debug_arp() {
	for(size_t i = 0; i < arp_table.size; i++) {
		arp_entry &arp = arp_table.table[i];
		if(!(arp.info & ARPT_PRESENT)) continue;
		printf("%s%s%s ",
			(arp.info & ARPT_STATIC) ? "S" : "-",
			(arp.info & ARPT_PASSIVE) ? "P" : "-",
			(arp.info & ARPT_KNOWN) ? "K" : "-");
		print_ip4(arp.ipa);
		printf(" : ");
		print_mac(arp.lla);
		xiv::putc(10);
	}
}

void debug_junk() {
	hw::ethdev->debug_junk();
}

void runsched() { // scheduled network tasks
	if(arp_tc.expired()) {
		runarp();
		arp_tc.add(1000);
	}
	if(net_tc.expired()) {
		net_tc.add(10000);
		debug_junk();
	}
	if(dh_tc.expired()) {
		dh_tc.add(100);
		dhcpc.run();
	}
}

ArrayList<Packet, mem::PAGE_SIZE> pktinfo;

void request_packet(Ref<Packet> &pkt) {
	auto itr = pktinfo.begin();
	size_t i = 0;
	for(; itr; itr++, i++) {
		Packet *p = *itr;
		if(!p->ref_count) { // FIXME threading (someday) will hate this
			pkt.reset(p);
			return;
		}
	}
	pkt.reset(nullptr);
}

void debug_packet() {
	auto itr = pktinfo.begin();
	uint32_t busy, pfree;
	busy = 0; pfree = 0;
	for(size_t i = 0; itr; i++, itr++) {
		if(!(*itr)->ref_count) {
			pfree++;
			continue;
		}
		busy++;
		printf("pkt: %03d %02x(%04d) %08x\n", i,
			(*itr)->ref_count,
			(*itr)->size,
			*(uint32_t*)(*itr)->buf);
	}
	printf("pkt:busy: %d free: %d\n", busy, pfree);
}

void init() {
	net_tc.fromnow(777);
	arp_tc.fromnow(777);
	printf("net: starting network layer\n");
	arp_table.size = 0;
	arp_table.table = (arp_entry*)mem::vmm_request(mem::PAGE_SIZE, nullptr, 0, mem::RQ_RW | mem::RQ_ALLOC);
	arp_table.limit = mem::PAGE_SIZE / sizeof(arp_entry);
	pktinfo.add(mem::vmm_request(mem::PAGE_SIZE, nullptr, 0, mem::RQ_RW | mem::RQ_ALLOC));
	size_t pkt_data_size = pktinfo.blockent;
	if(pkt_data_size & 1) ++pkt_data_size; // FIXME this *may* waste half a page
	pkt_data_size >>= 1;
	uint8_t *pktbuffers = (uint8_t*)mem::vmm_request(pkt_data_size * mem::PAGE_SIZE, nullptr, 0, mem::RQ_RW | mem::RQ_ALLOC);
	auto itr = pktinfo.begin();
	for(size_t i = 0; itr; i++, itr++) {
		(*itr)->ref_count = 0;
		// FIXME directly tied to page size, probably should be a const
		(*itr)->buf = pktbuffers + (2048 * i);
	}
	itr = pktinfo.begin();
	for(size_t i = 0; i < 64 && itr; itr++, i++) {
		// TODO figure out a good way the driver can request packets
		//hw::ethdev->addreceive(*itr);
	}
	dh_tc.fromnow(6000);
	dhcpc.start();
	printf("net: done\n");
}

void packet_fill_arp(Ref<Packet> pkt, uint8_t opc, const in_addr *nani, const mac_addr *lla) {
	size_t sop = 14;
	Packet &pk = *pkt;
	pk[sop - 2] = 0x08;
	pk[sop - 1] = 0x06;
	pk.w(sop) = _ix_bswapw(1);
	pk.w(sop+2) = _ix_bswapw(ETH_IP4);
	pk[sop+4] = 6;
	pk[sop+5] = 4;
	pk[sop+6] = 0;
	pk[sop+7] = opc;
	hw::ethdev->getmediaaddr(pk.buf+sop+8);
	pk.i(sop+14) = system_ip.s_addr;
	if(lla == nullptr) {
		pk.w(sop+18) = 0; pk.w(sop+20) = 0; pk.w(sop+22) = 0;
	} else {
		pk.w(sop+18) = *(uint16_t*)(lla->m+0);
		pk.w(sop+20) = *(uint16_t*)(lla->m+2);
		pk.w(sop+22) = *(uint16_t*)(lla->m+4);
	}
	pk.i(sop+24) = nani->s_addr;
}

void packet_send_resp_arp(Ref<Packet> pkt, const Ref<Packet> &src, const in_addr *nani, const mac_addr *doko) {
	Packet &pk = *pkt;
	hw::ethdev->getmediaaddr(pk.buf+6);
	pk.w(0) = src->w(6);
	pk.w(2) = src->w(8);
	pk.w(4) = src->w(10);
	packet_fill_arp(pkt, 2, nani, doko);
	pk.size = 42;
	hw::ethdev->transmit(pkt);
}

void packet_send_bc_arp(Ref<Packet> pkt, const in_addr *nani) {
	Packet &pk = *pkt;
	hw::ethdev->getmediaaddr(pk.buf+6);
	pk.w(0) = 0xffff;
	pk.w(2) = 0xffff;
	pk.w(4) = 0xffff;
	packet_fill_arp(pkt, 1, nani, nullptr);
	pk.size = 42;
	hw::ethdev->transmit(pkt);
}

// Internet Protocol Checksum, sans invert/NOT operation
uint16_t ipchksum(const void *dat, uint16_t len, uint32_t in_check = 0) {
	uint32_t checkhi = 0;
	const uint8_t *buf = (const uint8_t *)dat;
	for(unsigned q = 0;;) {
		checkhi += buf[q];
		in_check += buf[q + 1];
		q += 2;
		if(q >= len) break;
	}
	if(len & 1) checkhi += buf[len - 1];
	in_check += checkhi << 8;
	while(in_check & 0xffff0000u) {
		in_check = (in_check & 0xfffful) + (in_check >> 16);
	}
	return in_check;
}

bool packet_fill_ip(Ref<Packet> &pkt, const sockaddr *dst, size_t payload_len, uint8_t proto) {
	uint16_t ip_length = 20 + (uint16_t)payload_len;
	if(ip_length < 28) ip_length = 28;
	size_t sop = 14;
	Packet &pk = *pkt;
	if(dst->v4.s_addr == 0xffffffff) {
		pk.w(0) = 0xffff;
		pk.w(2) = 0xffff;
		pk.w(4) = 0xffff;
	} else {
		size_t arp_index = arp_table.find_ip(dst->v4);
		if(arp_index < arp_table.size) {
			arp_entry &arp = arp_table.table[arp_index];
			if(!(arp.info & ARPT_KNOWN)) {
				printf("ip-arp: unknown host: "); print_ip4(dst->v4); xiv::putc(10);
				return true;
			}
			for(size_t i = 0; i < 6; i++) {
				pk.buf[i] = arp.lla.m[i];
			}
		} else {
			resolve_with_arp(&dst->v4);
			printf("ip-arp: resolving: "); print_ip4(dst->v4); xiv::putc(10);
			return true;
		}
	}
	hw::ethdev->getmediaaddr(pk.buf+6); // TODO ethernet FIB? other devices?!
	pk.w(12) = _ix_bswapw(ETH_IP4);
	pk[sop] = 0x45; // no options
	pk[sop+1] = 0x00;
	pk.w(sop+2) = _ix_bswapw(ip_length);
	pk.w(sop+4) = 0x0000; // Ident
	pk.w(sop+6) = 0x0000; // flags + frag
	pk[sop+8] = 0x08; // TTL
	pk[sop+9] = proto; // Protocol Number
	pk.w(sop+10) = 0; // slot for checksum
	pk.i(sop+12) = system_ip.s_addr; // src
	pk.i(sop+16) = dst->v4.s_addr; // dst
	// recalculate the IP checksum
	pk.w(sop+10) = _ix_bswapw(~ipchksum(pk.buf + sop, 20));
	pk.size = 14 + ip_length; // extra 14 for MAC
	return false;
}

bool packet_fill_udp(Ref<Packet> pkt, uint16_t sport, const sockaddr *dst, uint16_t msg_size) {
	if(!hw::ethdev) return true;
	size_t sop = 34;
	Packet &pk = *pkt;
	constexpr uint16_t udp_mtu = 1500 - 20;
	uint32_t udp_length = msg_size + 8;
	if(udp_length > udp_mtu) udp_length = udp_mtu;
	// udp
	pk.w(sop+0) = _ix_bswapw(sport);
	pk.w(sop+2) = dst->port; // port is network order
	pk.w(sop+4) = _ix_bswapw((uint16_t)udp_length);
	pk.w(sop+6) = 0; // checksum field
	// generate the IP packet for psudo header
	if(packet_fill_ip(pkt, dst, udp_length, 17)) return true;
	uint16_t udp_check = ipchksum(pk.buf + sop, udp_length, pk.psudo_check() + udp_length);
	if(udp_check != 0xffff) udp_check = ~udp_check; // invert it, but not to zero
	pk.w(sop+6) = _ix_bswapw(udp_check);
	return false;
}

void send_via_udp(uint16_t sport, const sockaddr *dst, const uint8_t * msg, uint32_t msg_size) {
	if(!hw::ethdev) return;
	size_t sop = 34;
	Ref<Packet> pkt;
	request_packet(pkt);
	if(!pkt) return;
	Packet &pk = *pkt;
	for(size_t q = 0; q < 1472 && q < msg_size; q++) {
		pk[sop + 8 + q] = msg[q];
	}
	if(packet_fill_udp(pkt, sport, dst, msg_size)) return;
	printf("eth: "); print_mac(pkt->buf + 6);
	printf(" -> "); print_mac(pkt->buf + 0); xiv::putc(10);
	hw::ethdev->transmit(pkt);
}

void send_string_udp(uint16_t sport, const sockaddr *dst, const char * msg) {
	uint32_t sz = 0;
	for(;msg[sz]; sz++);
	send_via_udp(sport, dst, (uint8_t const *)msg, sz);
}

void handle_arp(Ref<Packet> pkt) {
	constexpr size_t sop = 14;
	Packet &pk = *pkt;
	uint16_t hwtype = pk.nw(sop);
	uint16_t prtype = pk.nw(sop + 2);
	if(hwtype != 1 || prtype != ETH_IP4) return; // no weird stuff
	if(pk.buf[sop + 4] != 6 || pk.buf[sop + 5] != 4) return; // no really
	uint16_t opc = pk.nw(sop + 6);
	in_addr w_ip;
	in_addr q_ip;
	w_ip.set(pk.buf + sop + 14);
	q_ip.set(pk.buf + sop + 24);
	mac_addr *src_mac = (mac_addr*)(pk.buf + sop + 8);
	mac_addr *tgt_mac = (mac_addr*)(pk.buf + sop + 18);
	if(opc == 1) { // these are queries
		uint32_t w = arp_table.find_ip(w_ip);
		if(w < arp_table.size) {
			// a source we know
		} else {
			// add a station
			w = arp_table.add();
			if(w < arp_table.size) {
				arp_entry &arp = arp_table.table[w];
				arp.info = ARPT_PASSIVE | ARPT_KNOWN | ARPT_PRESENT;
				arp.lla.set(src_mac);
				arp.ipa = w_ip;
				printf("arp: new-sta src: ");
				print_mac(src_mac);
				printf(" pr: ");
				print_ip4(w_ip);
				xiv::putc(10);
			}
		}
		uint32_t q = arp_table.find_ip(q_ip);
		if(q >= arp_table.size) return;
		// a query we know!
		arp_entry &arp = arp_table.table[q];
		if((arp.info & ARPT_STATUS) == ARPT_STATIC) {
			// static configured, but we aren't sure
			printf("arp: static reply\n");
		} else if((arp.info & ARPT_STATUS) == (ARPT_STATIC | ARPT_KNOWN)) {
			// we should answer this one...
			Ref<Packet> myarp;
			request_packet(myarp);
			if(!myarp) return;
			packet_send_resp_arp(myarp, pkt, &w_ip, src_mac);
			//printf("arp: config reply (%d)%08x\n", myarp->ref_count, myarp);
			//printf("arp: src: "); print_mac(src_mac);
			//printf(" pr: "); print_ip4(w_ip);
			//printf(" << dst: "); print_mac(tgt_mac);
			//printf(" pd: "); print_ip4(q_ip);
			//xiv::putc(10);
		}
	} else if(opc == 2) { // these are responses or advertisements
		uint32_t w = arp_table.find_ip(w_ip);
		if(w >= arp_table.size) {
			w = arp_table.add();
		}
		if(w >= arp_table.size) {
			printf("arp: table full, failed to add station\n");
			return;
		}
		arp_entry &arp = arp_table.table[w];
		if(arp.info & ARPT_PRESENT) {
			// we've heard this one before
			uint16_t status = (arp.info & ARPT_STATUS);
			if(status == ARPT_STATIC) {
				printf("arp: static collision\n");
				return;
			} else if(status == ARPT_PASSIVE) {
				printf("arp: station confirmed\n");
			} else if(status == ARPT_KNOWN) {
				printf("arp: station updated\n");
			} else {
				printf("arp: station query completed\n");
			}
		} else {
			arp.set(w_ip);
			printf("arp: station added\n");
		}
		arp.info = (ARPT_KNOWN | ARPT_PRESENT) + 180;
		arp.lla.set(src_mac);
		printf("arp: src: "); print_mac(src_mac);
		printf(" pr: "); print_ip4(w_ip);
		printf(" >> dst: "); print_mac(tgt_mac);
		printf(" pd: "); print_ip4(q_ip);
		xiv::putc(10);
	}
}

enum ICMP_TYPE : uint8_t {
	ICMP_EchoReply = 0,
	ICMP_Unreachable = 3,
	ICMP_SourceQuench = 4,
	ICMP_Redirect = 5,
	ICMP_Echo = 8,
	ICMP_TExpired = 11,
	ICMP_ParamError = 12,
	ICMP_Timestamp = 13,
	ICMP_TimestampReply = 14,
	ICMP_InfoRequest = 15,
	ICMP_InfoReply = 16,
};

bool handle_icmp(Packet &pkt, const sockaddr &src, size_t ofs, uint16_t len) {
	if(ofs + 8 > pkt.size) return false;
	ICMP_TYPE itype = (ICMP_TYPE)pkt[ofs];
	uint8_t icode = pkt[ofs+1];
	switch(itype) {
	case ICMP_EchoReply:
		return true;
	case ICMP_Unreachable:
		return true;
	case ICMP_Echo:
		if(icode != 0) return true;
	{
		size_t sop = 34;
		Ref<Packet> msg;
		request_packet(msg);
		if(!msg) return false;
		Packet &resp = *msg;
		for(size_t q = 0; q < 1472 && q < len; q++) {
			resp[sop + q] = pkt[ofs + q];
		}
		resp[sop] = ICMP_EchoReply;
		resp.w(sop+2) = 0;
		resp.w(sop+2) = _ix_bswapw(~ipchksum(resp.buf + sop, len));
		if(packet_fill_ip(msg, &src, len, 1)) return true;
		hw::ethdev->transmit(msg);
		return false;
	}
	case ICMP_TExpired:
		return true;
	default:
		return false;
	}
	//uint32_t iext = pkt.i(ofs+4);
}

void show_icmp(Packet &pkt, size_t ofs) {
	ICMP_TYPE itype = (ICMP_TYPE)pkt[ofs];
	uint8_t icode = pkt[ofs+1];
	uint32_t iext = pkt.i(ofs+4);
	printf("icmp: type:%02d code:%d x:%04x\n", itype, icode, iext);
}

bool handle_udp(Ref<Packet> pkt, size_t ofs, uint16_t len) {
	if(len < 8) return false;
	uint16_t src = pkt->nw(ofs);
	uint16_t dst = pkt->nw(ofs+2);
	uint16_t length = pkt->nw(ofs+4);
	//uint16_t check = pkt->nw(ofs+6);
	if(length < 8) return false;
	if(src == 67 && dst == 68) {
		dhcpc.recv(pkt, ofs + 8, length - 8);
		return false;
	}
	if(dst == 67) return false;
	if(dst > 1023) return false;
	if(dst == 137) return false;
	if(dst == 138) return false;
	return true;
}

void show_udp(Packet &pkt, size_t ofs) {
	uint16_t src = pkt.nw(ofs);
	uint16_t dst = pkt.nw(ofs+2);
	uint16_t length = pkt.nw(ofs+4);
	uint16_t check = pkt.nw(ofs+6);
	printf("udp: %d -> %d len:%d cks:%04x\n", src, dst, length, check);
}

void handle_ip6(Ref<Packet> pkt) {
	constexpr unsigned sop = 14;
	uint8_t ver = pkt->buf[sop] & 0xf0;
	if(ver != 0x60) {
		printf("ip6: invalid version\n");
		return;
	}
	if(pkt->buf[sop + 24] == 0xfe || pkt->buf[sop + 24] == 0xff) {
		return; // TODO handle these
	}
	uint32_t fltc = _ix_bswapi(pkt->i(sop)) & 0x0fffffff;
	uint16_t length = pkt->nw(sop + 4);
	uint8_t nh = pkt->buf[sop + 6];
	uint8_t hl = pkt->buf[sop + 7];
	printf("ip6: len:%05d nh:%03d hl:%03d tc:%04x ", length, nh, hl, fltc);
	printf("src: "); print_ip6(pkt->buf + sop + 8);
	printf(" dst: "); print_ip6(pkt->buf + sop + 24);
	xiv::putc(10);
}

void handle_ip4(Ref<Packet> pkt) {
	constexpr unsigned sop = 14;
	uint8_t ipvihl = pkt->buf[sop];
	if((ipvihl & 0xf0) != 0x40) {
		printf("ip4: unsupported version: %02x\n", ipvihl);
		return;
	}
	ipvihl = (ipvihl & 0xf) << 2;
	if(ipvihl < 20) return;
	uint16_t iplen = pkt->nw(sop+2);
	if(iplen < ipvihl) return;
	if(iplen + sop > pkt->size) return;
	iplen -= ipvihl;
	uint8_t tos = pkt->buf[sop+1];
	uint16_t ident = pkt->nw(sop+4);
	uint16_t fragflags = pkt->nw(sop+6);
	uint8_t ttl = pkt->buf[sop+8];
	uint8_t proto = pkt->buf[sop+9];
	if(pkt->buf[sop+16] >= 224 && pkt->buf[sop+16] < 255) return; // FIXME multicast filter, until we care
	sockaddr source = { 1, 0, pkt->i(sop + 12) };
	bool show = true;
	if(proto == 6) { // TCP
		show = false;
	} else if(proto == 17) { // UDP
		show = handle_udp(pkt, sop + ipvihl, iplen);
	} else if(proto == 1) { // ICMP
		show = handle_icmp(*pkt, source, sop + ipvihl, iplen);
	} else { // other IP protocols
	}
	if(show) {
		printf("ip4: tos:%02x len:%04x ident:%04x ff:%04x ttl:%03d ", tos, iplen, ident, fragflags, ttl);
		printf("src: "); print_ip4(pkt->buf+sop+12);
		printf(" dst: "); print_ip4(pkt->buf+sop+16);
		printf(" nextproto: %d\n", proto);
		if(proto == 6) {
			//show_tcp(*pkt, sop + ipvihl);
		} else if(proto == 17) {
			show_udp(*pkt, sop + ipvihl);
		} else if(proto == 1) {
			show_icmp(*pkt, sop + ipvihl);
		}
	}
}

void enqueue(Ref<Packet> pkt) {
	if(pkt->buf[0] & 1) { // multicast traffic
		//if(pkt->buf[0] != 0xff) return; // FIXME: lazy multicast filter
	}
	ETHERTYPE netproto = (ETHERTYPE)pkt->nw(12);
	if(netproto == ETH_IP6) {
		handle_ip6(pkt);
	} else if(netproto == ETH_IP4) {
		handle_ip4(pkt);
	} else if(netproto == ETH_ARP) {
		handle_arp(pkt);
	} else {
		printf("eth: type: %04x\n", netproto);
		print_data(pkt->buf, pkt->size);
	}
}

} // namespace net

