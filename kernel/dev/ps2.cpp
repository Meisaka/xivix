
#include "ps2.hpp"
#include "kio.hpp"
#include "ktext.hpp"

extern "C" {

extern void (*_ivix_irq1_fn)(void);
extern void (*_ivix_irq12_fn)(void);

void _ix_totalhalt();

}
namespace hw {

PS2 PS2::dev;


void PS2::irq1_signal() {
	PS2::dev.irq1_handle();
}
void PS2::irq12_signal() {
	PS2::dev.irq12_handle();
}
PS2::PS2() {
	kmd_l = 0;
	err_n = 0;
	_ivix_irq1_fn = PS2::irq1_signal;
	_ivix_irq12_fn = PS2::irq12_signal;
	kb_drv = nullptr;
	for(unsigned i = 0; i < 4; i++) {
		port[i] = {
			PS2ST::INIT,
			PS2TYPE::QUERY,
			_ivix_int_n + 20,
			0,
			nullptr
		};
	}
}
PS2::~PS2() {}
void PS2::irq1_handle() {
	if(!cinit) return;
	uint8_t sb = _ix_inb(0x64);
	if(sb & 0x20) {
		interupted |= 2;
		istatus[1] = _ix_inb(0x64);
		icode[1] = _ix_inb(0x60);
		push_keycode(icode[1] | 0x100);
		keycount++;
	} else {
		interupted |= 1;
		istatus[0] = _ix_inb(0x64);
		icode[0] = _ix_inb(0x60);
		push_keycode(icode[0]);
		keycount++;
	}
}
void PS2::irq12_handle() {
	if(!cinit) return;
	uint8_t sb = _ix_inb(0x64);
	if(sb & 0x20) {
		interupted |= 2;
		istatus[1] = _ix_inb(0x64);
		icode[1] = _ix_inb(0x60);
		push_keycode(icode[1] | 0x100);
		keycount++;
	} else {
		interupted |= 1;
		istatus[0] = _ix_inb(0x64);
		icode[0] = _ix_inb(0x60);
		push_keycode(icode[0]);
		keycount++;
	}
}
void PS2::push_keycode(uint16_t v) {
	for(int i = 11; i > 0; i--) {
		keycode[i] = keycode[i-1];
	}
	keycode[0] = v;
}

uint8_t PS2::ps2_get_read() {
	while( !(_ix_inb(0x64) & 1) ) {
	}
	uint8_t r = _ix_inb(0x60);
	return r;
}
static void ps2_wait_fwrite() {
	uint8_t sb;
	do { sb = _ix_inb(0x64);
	}
	while(
		(sb & 2) && (~sb & 0xc0)
	);
}
static void ps2_wait_write() {
	uint8_t sb;
	uint32_t kw = _ivix_int_n + 5;
	do { sb = _ix_inb(0x64);
	}
	while(
		(_ivix_int_n < kw) &&
		(sb & 2) && (~sb & 0xc0)
	);
}
static bool ps2_can_write() {
	uint8_t sb;
	sb = _ix_inb(0x64);
	return !( (sb & 2) && (~sb & 0xc0) );
}
void PS2::send_data(uint8_t v) {
	ps2_wait_fwrite();
	_ix_outb(0x60, v);
}
void PS2::send_adata(uint8_t v) {
	ps2_wait_write();
	_ix_outb(0x60, v);
}
void PS2::send_cmd(uint8_t v) {
	ps2_wait_fwrite();
	_ix_outb(0x64, v);
}

void PS2::system_reset() {
	uint8_t sb;
	do {
		sb = _ix_inb(0x64);
		if(sb & 1) {
			_ix_inb(0x60);
		}
	} while( (sb & 2) );
	_ix_outb(0x64, 0xfe);
	// toggle reset for the system
	_ix_totalhalt();
}
bool PS2::init() {
	uint8_t c, t;
	err_n &= 0xF777;
	cinit = false;
	xiv::print("PS2: init\n");
	if(!(err_n & 0x4)) {
		send_cmd(0xAD); // port 1 dis
		send_cmd(0xA7); // port 2 dis
		send_cmd(0xAA); // test
		ps2_wait_fwrite();
		t = ps2_get_read();
		if(t != 0x55) {
			err_n |= 0x8;
			xiv::print("PS2: controllor test fail\n");
			return false;
		}
		err_n |= 0x4;
	}

	send_cmd(0x20); // read r0
	ps2_wait_fwrite();
	c = ps2_get_read();
	c &= 0x34;
	send_cmd(0x60); // write r0
	send_data(c); // config

	send_cmd(0xAB); // port 1 test
	ps2_wait_fwrite();
	t = ps2_get_read();
	if(t != 0) {
		err_n |= 0x80;
		xiv::print("PS2: port 1 test fail\n");
		return false;
	}

	send_cmd(0xA9); // port 2 test
	ps2_wait_fwrite();
	t = ps2_get_read();
	if(t != 0) {
		xiv::print("PS2: port 2 test fail\n");
		err_n |= 0x800;
	}
	if(err_n & 0x088) return false;

	c |= 0x03;
	send_cmd(0x60); // write r0
	send_data(c);

	send_cmd(0x20); // read r0
	ps2_wait_fwrite();
	c = ps2_get_read();
	if(c & 0x1) err_n |= 0x10;
	if(c & 0x2) err_n |= 0x100;
	if((c & 0x3) == 0) {
		err_n ^= 0x4;
		return false;
	}

	send_cmd(0xAE); // port 1 ena
	send_cmd(0xA8); // port 2 ena

	cinit = true;

	send_cmd(0xD4);
	send_data(0xff);

	{
	uint32_t d = _ivix_int_n + 20;
	while( !(interupted & 2) && (d > _ivix_int_n) ) {
	}
	if(interupted & 2) {
		err_n |= 0x200;
	} else {
		xiv::print("PS2: port 2 interupts fail - disabling\n");
		port[1].status = PS2ST::DISABLE;
	}
	}

	return true;
}
void PS2::add_kbd(Keyboard *k) {
	kb_drv = k;
}

void PS2::init_kbd(uint32_t p) {
	if(kb_drv) {
		add_client(kb_drv, p);
		if(kb_drv->init()) {
			port[p].status = PS2ST::IDLE;
		} else {
			port[p].status = PS2ST::LOST;
		}
	} else {
		port[p].status = PS2ST::LOST;
	}
}

void PS2::add_client(MultiPortClient *c, uint32_t u) {
	if(u > 3) return;
	if(c == nullptr) return;
	port[u].cl_ptr = c;
	c->mp_serv = this;
	c->mp_port = u;
}
void PS2::remove_client(MultiPortClient *c, uint32_t u) {
	if(u > 3) return;
	if(port[u].cl_ptr != c) {
		if(port[u].cl_ptr != nullptr) {
			port[u].cl_ptr->clear_client_callback();
		}
		port[u].cl_ptr = nullptr;
		return;
	}
	port[u].cl_ptr = nullptr;
	if(c != nullptr) {
		c->clear_client_callback();
	}
}

bool PS2::waiting() {
	if( keycount || interupted ) return true;
	return (_ivix_int_n > port[0].nextcheck)
		|| (_ivix_int_n > port[1].nextcheck);
}
void PS2::signal_loss(uint32_t u) {
	remove_client(port[u].cl_ptr, u);
	if(port[u].type == PS2TYPE::KEYBOARD) {
		if(kb_drv) kb_drv->remove();
	}
	port[u].status = PS2ST::LOST;
}

void PS2::handle() {
	uint32_t c;
	if(!cinit) {
		init();
		return;
	}
	while(keycount > 0) {
		uint32_t i;
		c = port_query(~0);
		i = (c >> 8) & 0xff;
		c &= 0xff;
		if(i > 3) {
			break;
		}
		switch(port[i].status) {
		case PS2ST::DISABLE:
			break;
		case PS2ST::INIT:
			break;
		case PS2ST::LOST:
			port[i].nextcheck = _ivix_int_n + 50;
			if(c == 0xEE) {
				port[i].err_c = 0;
				port[i].status = PS2ST::IDENT;
			} else if(c == 0xAA) {
				port[i].err_c = 0;
				port[i].status = PS2ST::IDENT;
				port[i].type = PS2TYPE::QUERY;
			} else if(c == 0xFE) {
			} else if(c == 0xFC) {
				port[i].status = PS2ST::IDENT;
			} else {
				port[i].err_c = 0;
				port[i].status = PS2ST::IDENT;
			}
			break;
		case PS2ST::ECHO:
			if(c == 0xFE) {
				if(istatus[i] & 0xC0) port[i].err_c++;
				if(port[i].err_c > 3) {
					xiv::printf("PS2: port %d echo lost\n", i+1);
					port[i].status = PS2ST::LOST;
					signal_loss(i);
					port[i].type = PS2TYPE::QUERY;
				}
				break;
			} else if(c != 0xEE) {
				port[i].err_c = 0;
				err_n &= 0xffff;
				err_n |= c << 16;
			} else {
				port[i].err_c = 0;
				err_n &= 0xefff;
			}
			port[i].status = PS2ST::IDLE;
			break;
		case PS2ST::IDLE:
			port[i].status = PS2ST::BUSY;
			port[i].nextcheck = _ivix_int_n + 50;
			if(port[i].cl_ptr) {
				port[i].cl_ptr->port_data(cast<uint8_t>(c));
			}
			break;
		case PS2ST::BUSY:
			port[i].nextcheck = _ivix_int_n + 50;
			if(port[i].cl_ptr) {
				port[i].cl_ptr->port_data(cast<uint8_t>(c));
			}
			break;
		case PS2ST::IDENT:
		case PS2ST::IDENTE:
			{
				port[i].nextcheck = _ivix_int_n + 25;
			PS2TYPE dettype = port[i].type;
			PS2ST nextst, idst, id1st;
			if(port[i].status == PS2ST::IDENTE) {
				nextst = PS2ST::IDLE;
				idst = PS2ST::IDENTE;
				id1st = PS2ST::IDENTE1;
				port[i].nextcheck = _ivix_int_n + 200;
			} else {
				nextst = PS2ST::INIT;
				idst = PS2ST::IDENT;
				id1st = PS2ST::IDENT1;
			}
			if(c == 0x00) {
				dettype = PS2TYPE::MOUSE3;
			} else if(c == 0x03u) {
				dettype = PS2TYPE::MOUSE4;
			} else if(c == 0x04u) {
				dettype = PS2TYPE::MOUSE5;
			} else if(c == 0xABu) {
				nextst = id1st;
			} else if(c == 0xFEu) {
				dettype = PS2TYPE::QUERY;
				nextst = PS2ST::LOST;
			} else if(c == 0xFAu) {
				nextst = idst;
			} else if(c == 0xFCu) {
				nextst = idst;
			} else {
				nextst = PS2ST::IDENT2;
			}
			if(dettype != port[i].type) {
				port[i].type = dettype;
				if(nextst == PS2ST::LOST) {
					xiv::printf("PS2: port %d ident err - lost\n", i+1);
				}
			}
			port[i].status = nextst;
			}
			break;
		case PS2ST::IDENT1:
		case PS2ST::IDENTE1:
			port[i].nextcheck = _ivix_int_n + 25;
			if(PS2TYPE::KEYBOARD != port[i].type) {
				port[i].status = PS2ST::INIT;
				port[i].type = PS2TYPE::KEYBOARD;
			} else {
				if(port[i].status == PS2ST::IDENTE1) {
					port[i].status = PS2ST::IDLE;
				} else {
					port[i].status = PS2ST::LOST;
				}
			}
			err_n &= 0xffff;
			err_n |= (0xAB00 | c) << 16;
			break;
		case PS2ST::IDENT2:
			break;
		}
	}
	for(unsigned i = 0; i < 2; i++) {
		uint32_t f = 1 << i;
		if(port[i].status == PS2ST::DISABLE) {
			port[i].nextcheck = _ivix_int_n + 1000;
			continue;
		} else if(port[i].status == PS2ST::INIT) {
			switch(port[i].type) {
			case PS2TYPE::MOUSE3:
			case PS2TYPE::MOUSE4:
			case PS2TYPE::MOUSE5:
				xiv::printf("PS2: port %d init as mouse\n", i+1);
				port[i].status = PS2ST::IDLE;
				break;
			case PS2TYPE::KEYBOARD:
				xiv::printf("PS2: port %d init as keyboard\n", i+1);
				init_kbd(i);
				port[i].type = PS2TYPE::KEYBOARD;
				break;
			default:
				xiv::printf("PS2: port %d init unknown - querying\n", i+1);
				port[i].type = PS2TYPE::QUERY;
				port[i].status = PS2ST::IDENT;
				if(ps2_can_write()) {
				port_send(0xF2, i); // ident
				}
				break;
			}
		}
		if(_ivix_int_n > port[i].nextcheck) {
			if( (interupted&f) ) interupted &= ~f;
			port[i].nextcheck = _ivix_int_n + 75;
			switch(port[i].status) {
			case PS2ST::IDLE:
				if(!ps2_can_write()) break;
				port[i].status = PS2ST::IDENTE;
				if(port[i].type == PS2TYPE::KEYBOARD) {
					port_send(0xF2, i); // ident
				} else {
					port_send(0xF2, i); // ident
				}
				break;
			case PS2ST::BUSY:
				port[i].nextcheck = _ivix_int_n + 50;
				port[i].status = PS2ST::IDLE;
				break;
			case PS2ST::LOST:
				if(!ps2_can_write()) break;
				port[i].nextcheck = _ivix_int_n + 50;
				port[i].type = PS2TYPE::QUERY;
				port_send(0xF2, i); // ident
				port[i].status = PS2ST::IDENT;
				break;
			case PS2ST::IDENT:
				port[i].nextcheck = _ivix_int_n + 100;
				port[i].status = PS2ST::LOST;
				break;
			case PS2ST::IDENT1:
				port[i].status = PS2ST::INIT;
				port[i].type = PS2TYPE::KEYBOARD;
				break;
			case PS2ST::IDENT2:
				xiv::printf("PS2: port %d IDENT2 timeout\n", i+1);
				port[i].nextcheck = _ivix_int_n + 100;
				port[i].status = PS2ST::LOST;
				break;
			default:
				break;
			}
		}
	}
}
void PS2::port_enable(uint32_t u, bool e) {
	if(u > 1) return;
	if(e) {
		if(port[u].status == PS2ST::DISABLE) {
			xiv::printf("PS2: port %d enabled\n", u+1);		
			port[u].status = PS2ST::LOST;
		}
	} else {
		port[u].status = PS2ST::DISABLE;
		xiv::printf("PS2: port %d disabled\n", u+1);		
	}
}
uint32_t PS2::port_query(uint32_t p) {
	if(p == ~0u) {
		if(keycount > 0) {
			uint32_t k;
			keycount --;
			if(keycount > 12) k = keycode[11]; else k = keycode[keycount];
			uint32_t f = 1 << ((k >> 8) & 0xff);
			interupted ^= interupted & f;
			return k;
		}
		return ~0u;
	} else {
		for(int i = keycount>12?12:keycount; i > 0; ) {
			i--;
			if(cast<uint32_t>(keycode[i] >> 8) == p) {
				uint32_t k = keycode[i];
				for(; i < keycount - 1; i++) {
					keycode[i] = keycode[i+1];
				}
				keycount--;
				interupted ^= interupted & (1 << p);
				return k;
			}
		}
	}
	return ~0u;
}
void PS2::port_send(uint8_t km, uint32_t ex) {
	if(ex == 0) {
		send_data(km);
	} else if(ex == 1) {
		send_cmd(0xD4);
		send_data(km);
	}
}

} // hw

