
#include "ps2.hpp"
#include "vgatext.hpp"
#include "kio.hpp"

extern "C" {

extern void (*_ivix_irq1_fn)(void);
extern void (*_ivix_irq12_fn)(void);

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
	for(unsigned i = 0; i < 4; i++) {
		port[i] = {
			PS2ST::INIT,
			PS2TYPE::QUERY,
			_ivix_int_n + 20,
			0
		};
	}
}
PS2::~PS2() {}
void PS2::irq1_handle() {
	uint8_t sb = _ix_inb(0x64);
	if(sb & 0x20) {
		interupted |= 2;
		istatus[1] = _ix_inb(0x64);
		icode[1] = _ix_inb(0x60);
		push_keycode(icode[1]);
	} else {
		interupted |= 1;
		istatus[0] = _ix_inb(0x64);
		icode[0] = _ix_inb(0x60);
		push_keycode(icode[0]);
	}
}
void PS2::irq12_handle() {
	uint8_t sb = _ix_inb(0x64);
	if(sb & 0x20) {
		interupted |= 2;
		istatus[1] = _ix_inb(0x64);
		icode[1] = _ix_inb(0x60);
		push_keycode(icode[1]);
	} else {
		interupted |= 1;
		istatus[0] = _ix_inb(0x64);
		icode[0] = _ix_inb(0x60);
		push_keycode(icode[0]);
	}
}
void PS2::push_keycode(uint16_t v) {
	for(int i = 8; i > 0; i--) {
		keycode[i] = keycode[i-1];
	}
	keycode[0] = v;
	VGAText::dev.setto(1,17);
	for(int x = 0; x < 12; x++) {
		VGAText::dev.puthex16(keycode[x]);
		VGAText::dev.putc(' ');
	}
}
uint8_t PS2::ps2_get_read(bool force) {
	uint32_t d = _ivix_int_n + 2000;
	uint8_t s=_ix_inb(0x64);
	while((force || (d < _ivix_int_n)) && !((s=_ix_inb(0x64)) & 1)) {
		if(interupted) {
			interupted = 0;
			return icode[0];
		}
	}
	if(interupted) {
		interupted = 0;
		return icode[0];
	}
	if( (_ix_inb(0x64) & 1) == 0 ) return 0;
	uint8_t r = _ix_inb(0x60);
	push_keycode(r);
	return r;
}
static void ps2_wait_fwrite() {
	uint8_t sb;
	do { sb = _ix_inb(0x64); }
	while(
		(sb & 2) && (~sb & 0xc0)
	);
}
void PS2::send_data(uint8_t v) {
	ps2_wait_fwrite();
	_ix_outb(0x60, v);
}
void PS2::send_cmd(uint8_t v) {
	ps2_wait_fwrite();
	_ix_outb(0x64, v);
}

static bool kbd_cmd(uint32_t u, uint8_t cc, uint32_t p, unsigned n) {
	int i = 6;
	while(i > 0) {
		i--;
		PS2::dev.port_send(cc, u);
		for(unsigned x = 0; x < n; x++) {
		PS2::dev.port_send(cast<uint8_t>(p), u);
		}
		ps2_wait_fwrite();
		uint32_t rc;
		rc = PS2::dev.port_query(u);
		if(rc == ~0u) continue;
		if(rc != 0x0FE) {
			if(rc == 0xFA) return true;
		}
	}
	return false;
}
static bool kbd_cmd_ex(uint32_t u, uint8_t cc, uint32_t ex) {
	int i = 6;
	while(i > 0) {
		i--;
		PS2::dev.port_send(cc, u);
		ps2_wait_fwrite();
		uint32_t rc;
		rc = PS2::dev.port_query(u);
		if(rc == ~0u) continue;
		if(rc != 0x0FE) {
			if(rc == ex) return true;
		}
	}
	return false;
}
void PS2::system_reset() {
	send_cmd(0xfe); // toggle reset for the system
}
void PS2::init() {
	uint8_t c, t;
	err_n &= 0xF777;
	if(!(err_n & 0x4)) {
		send_cmd(0xAD); // port 1 dis
		send_cmd(0xA7); // port 2 dis
		send_cmd(0xAA); // test
		ps2_wait_fwrite();
		t = ps2_get_read(true);
		push_keycode(t | 0xAA00);
		if(t != 0x55) {
			err_n |= 0x8;
			return;
		}
		err_n |= 0x4;
	}

	send_cmd(0x20); // read r0
	ps2_wait_fwrite();
	c = ps2_get_read(true);
	push_keycode(c | 0x2000);
	c &= 0x34;
	send_cmd(0x60); // write r0
	send_data(c); // config

	send_cmd(0xAB); // port 1 test
	ps2_wait_fwrite();
	t = ps2_get_read(true);
	push_keycode(t | 0xAB00);
	if(t != 0) {
		err_n |= 0x80;
		return;
	}

	send_cmd(0xA9); // port 2 test
	ps2_wait_fwrite();
	t = ps2_get_read(true);
	push_keycode(t | 0xA900);
	if(t != 0) {
		err_n |= 0x800;
	}
	if(err_n & 0x088) return;

	c |= 0x03;
	keycode[11] = c;
	send_cmd(0x60); // write r0
	send_data(c);

	send_cmd(0x20); // read r0
	ps2_wait_fwrite();
	c = ps2_get_read(true);
	push_keycode(c | 0x2200);
	if(c & 0x1) err_n |= 0x10;
	if(c & 0x2) err_n |= 0x100;
	if((c & 0x3) == 0) {
		err_n ^= 0x4;
		return;
	}

	send_cmd(0xAE); // port 1 ena
	send_cmd(0xA8); // port 2 ena
	cinit = true;
}
void PS2::init_kbd(uint32_t p) {
	uint32_t c;
	uint32_t es = 0;
	if(!kbd_cmd_ex(p, 0xff, 0xaa)) es++;
	if(!kbd_cmd(p, 0xed, 0x07, 1)) es++; // setled ...
	if(!kbd_cmd(p, 0xf0, 0x02, 1)) es++; // set code set ...
	if(!kbd_cmd(p, 0xf0, 0x00, 1)) es++; // get code set ...
	if(es > 3) {
		port[p].status = PS2ST::LOST;
		return;
	}
	c = port_query(p);
	keycode[11] = cast<uint8_t>(c);
	if(!kbd_cmd(p, 0xf3, 0x02, 1)) es++; // set rate ...
	if(!kbd_cmd(p, 0xf4, 0, 0)) es++; // scan
	if(!kbd_cmd(p, 0xed, 0x03, 1)) es++; // setled ...
	port[p].status = PS2ST::IDLE;
}


bool PS2::waiting() {
	return (_ivix_int_n > port[0].nextcheck)
		|| (_ivix_int_n > port[1].nextcheck)
		|| (interupted != 0);
}
void PS2::handle() {
	uint32_t c;
	if(!cinit) {
		init();
		return;
	}
	keycode[10] = _ix_inb(0x64);
	for(unsigned i = 0; i < 2; i++) {
		uint32_t f = 1 << i;
		if(_ivix_int_n > port[i].nextcheck) {
			if( (interupted&f) ) interupted &= ~f;
			port[i].nextcheck = _ivix_int_n + 25;
			switch(port[i].status) {
			case PS2ST::IDLE:
				port[i].status = PS2ST::ECHO;
				if(port[i].type == PS2TYPE::KEYBOARD) {
					port_send(0xEE, i); // echo
				} else {
					port_send(0xF2, i); // ident
				}
				break;
			case PS2ST::LOST:
				port[i].nextcheck = _ivix_int_n + 5;
				port_send(0xF2, i); // ident
				port[i].status = PS2ST::IDENT;
				break;
			case PS2ST::IDENT:
				port[i].nextcheck = _ivix_int_n + 50;
				port[i].status = PS2ST::LOST;
				break;
			case PS2ST::IDENT1:
				port[i].status = PS2ST::INIT;
				port[i].type = PS2TYPE::KEYBOARD;
				break;
			case PS2ST::IDENT2:
				port[i].status = PS2ST::INIT;
				port[i].type = PS2TYPE::KEYBOARD;
				break;
			default:
				break;
			}
		}
		switch(port[i].status) {
		case PS2ST::LOST:
			if( !(interupted&f) ) break;
			c = port_query(i);
			if(c == 0xEE) {
				port[i].err_c = 0;
				port[i].status = PS2ST::INIT;
				port[i].type = PS2TYPE::KEYBOARD;
			} else if(c == 0xAA) {
				port[i].err_c = 0;
				port[i].status = PS2ST::INIT;
			} else if(c != 0xFE && c != ~0u) {
				port[i].err_c = 0;
				port[i].status = PS2ST::INIT;
			}
			break;
		case PS2ST::INIT:
			switch(port[i].type) {
			case PS2TYPE::MOUSE3:
			case PS2TYPE::MOUSE4:
			case PS2TYPE::MOUSE5:
				port[i].status = PS2ST::IDLE;
				break;
			default:
				init_kbd(i);
				port[i].type = PS2TYPE::KEYBOARD;
				break;
			}
			break;
		case PS2ST::ECHO:
			c = port_query(i);
			if(c == 0xFE) {
				if(istatus[i] & 0xC0) port[i].err_c++;
				port_send(0xEE, i); // echo
				if(port[i].err_c > 3) {
					port[i].status = PS2ST::LOST;
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
			if(!(interupted&f)) break;
			keycode[9] = port_query(i);
			break;
		case PS2ST::IDENT:
			if(!(interupted&f)) break;
			c = port_query(i);
			if(c == 0xFE) {
				port[i].nextcheck = _ivix_int_n + 50;
				port[i].status = PS2ST::LOST;
			} else if(c == 0xFA) {
				port[i].status = PS2ST::IDENT1;
				port[i].nextcheck = _ivix_int_n + 25;
			}
			break;
		case PS2ST::IDENT1:
			if(!(interupted&f)) break;
			c = port_query(i);
			port[i].nextcheck = _ivix_int_n + 25;
			if(c == 0x00) {
				port[i].status = PS2ST::INIT;
				port[i].type = PS2TYPE::MOUSE3;
			} else if(c == 0x03) {
				port[i].status = PS2ST::INIT;
				port[i].type = PS2TYPE::MOUSE4;
			} else if(c == 0x04) {
				port[i].status = PS2ST::INIT;
				port[i].type = PS2TYPE::MOUSE5;
			} else if(c == 0xAB) {
				port[i].status = PS2ST::IDENT2;
			} else {
				port[i].status = PS2ST::INIT;
				port[i].type = PS2TYPE::KEYBOARD;
			}
			err_n &= 0xffff;
			err_n |= c << 16;
			break;
		case PS2ST::IDENT2:
			if(!(interupted&f)) break;
			port[i].nextcheck = _ivix_int_n + 25;
			c = port_query(i);
			port[i].status = PS2ST::INIT;
			port[i].type = PS2TYPE::KEYBOARD;
			err_n &= 0xffff;
			err_n |= (0xAB00 | c) << 16;
			break;
		}
	}
}
uint32_t PS2::port_query(uint32_t p) {
	uint32_t f = 1 << p;
	while(_ivix_int_n < port[p].nextcheck && !(interupted&f));
	if(!(interupted&f)) {
		return ~0;
	} else {
		interupted &= ~f;
		return icode[p];
	}
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
