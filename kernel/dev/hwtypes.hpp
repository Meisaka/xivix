/* ***
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */
#ifndef HWTYPES_HAI
#define HWTYPES_HAI

#include "ktypes.hpp"
#include "net/packet.hpp"

namespace hw {

enum class HWTYPE : uint32_t {
};

struct Hardware {
	uint32_t int_line;
	virtual bool init() = 0;
	virtual void remove() = 0;
};
struct Port {
	virtual uint32_t port_query() = 0;
	virtual void port_send(uint8_t c) = 0;
};
struct NetworkMAC : public Hardware {
	virtual void transmit(Ref<net::Packet>) = 0;
	virtual void addreceive(Ref<net::Packet>) = 0;
	virtual void getmediaaddr(uint8_t *) const = 0;
	virtual void processqueues() {};
	virtual void debug_junk() {};
	virtual void debug_cmd(void *, uint32_t) {};
};

struct MultiPortClient;
struct MultiPortService {
	virtual void client_send(uint32_t d, uint32_t u) = 0;
	virtual uint32_t client_req(uint32_t u) = 0;
	virtual void add_client(MultiPortClient*, uint32_t u) = 0;
	virtual void remove_client(MultiPortClient*, uint32_t u) = 0;
};
class MultiPortClient {
protected:
public:
	uint32_t mp_port;
	MultiPortService *mp_serv;
	MultiPortClient() : mp_serv(nullptr) {}
	virtual ~MultiPortClient() {
		if(mp_serv) {
			mp_serv->remove_client(this, mp_port);
		}
	}
	void clear_client_callback() {
		mp_serv = nullptr;
	}
	void set_client_callback(MultiPortService *mp, uint32_t u) {
		if(mp_serv) {
			mp_serv->remove_client(this, mp_port);
		}
		mp_serv = mp;
		mp_port = u;
	}
	virtual void port_data(uint8_t) = 0;
	void send_data(uint8_t v) {
		if(!mp_serv) return;
		mp_serv->client_send(v, mp_port);
	}
};
struct MultiPort {
	virtual uint32_t port_query(uint32_t u) = 0;
	virtual void port_send(uint8_t c, uint32_t u) = 0;
};

} // hw

#endif

