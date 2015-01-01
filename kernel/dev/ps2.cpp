
#include "ps2.hpp"
#include "vgatext.hpp"
#include "kio.hpp"

extern "C" {

extern void (*_ivix_irq1_fn)(void);
extern void (*_ivix_irq12_fn)(void);

}
namespace hw {

PS2 PS2::dev;

static bool kbd_cmd(uint32_t u, uint8_t cc, uint32_t p, unsigned n) {
	int i = 6;
	while(i > 0) {
		i--;
		PS2::dev.port_send(cc, u);
		for(unsigned x = 0; x < n; x++) {
		PS2::dev.port_send(cast<uint8_t>(p), u);
		}
		//ps2_wait_fwrite();
		uint32_t rc;
		rc = PS2::dev.port_query(u);
		if(rc == ~0u) continue;
		rc &= 0xff;
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
		//ps2_wait_fwrite();
		uint32_t rc;
		rc = PS2::dev.port_query(u);
		if(rc == ~0u) continue;
		rc &= 0xff;
		if(rc != 0x0FE) {
			if(rc == ex) return true;
			return false;
		}
	}
	return false;
}
Keyboard::Keyboard() {
	state = 0;
}
Keyboard::~Keyboard() {
}

bool Keyboard::init() {
	if(!mp_serv) return false;
	uint32_t c;
	uint32_t es = 0;
	if(!kbd_cmd_ex(mp_port, 0xff, 0xaa)) es++;
	if(!kbd_cmd(mp_port, 0xed, 0x07, 1)) es++; // setled ...
	if(!kbd_cmd(mp_port, 0xf0, 0x02, 1)) es++; // set code set ...
	if(!kbd_cmd(mp_port, 0xf0, 0x00, 1)) es++; // get code set ...
	if(es > 3) {
		return false;
	}
	//c = port_query(mp_port);
	if(!kbd_cmd(mp_port, 0xf3, 0x02, 1)) es++; // set rate ...
	if(!kbd_cmd(mp_port, 0xf4, 0, 0)) es++; // scan
	if(!kbd_cmd(mp_port, 0xed, 0x03, 1)) es++; // setled ...
	return true;
}

void Keyboard::key_down() {
	lastkey1 = keycode;
}

void Keyboard::key_up() {
	lastkey2 = lastkey1;
	lastkey1 = keycode;
}

void Keyboard::remove() {
}
void Keyboard::port_data(uint8_t c) {
	lastscan = c;
	switch(state) {
	case 0:
		keycode = 0;
		if(c == 0xE0) {
			state = 2;
			keycode |= 0x20000;
			return;
		} else if(c == 0xE1) {
			state = 4;
			keycode |= 0x40000;
			return;
		} else if(c == 0xF0) {
			state = 1;
			keycode |= 0x10000;
			return;
		}
		keycode |= c;
		// keydown
		key_down();
		return;
	case 1:
		keycode |= c;
		state = 0;
		// key up
		key_up();
		return;
	case 2:
		if(c == 0xF0) {
			state = 3;
			keycode |= 0x10000;
			break;
		}
		keycode |= c;
		state = 0;
		// keydown
		key_down();
		return;
	case 3:
		keycode |= c;
		state = 0;
		// key up
		key_up();
		return;
	case 4:
		if(c == 0xF0) {
			state = 6;
			keycode |= 0x10000;
			break;
		}
		keycode |= c;
		state = 5;
		return;
	case 5:
		keycode |= cast<uint32_t>(c) << 8;
		state = 0;
		// keydown
		key_down();
		return;
	case 6:
		state = 7;
		keycode |= c;
		return;
	case 7:
		if(c == 0xF0) {
			state = 8;
			break;
		}
		keycode |= cast<uint32_t>(c) << 8;
		state = 0;
		// key toggle
		key_down();
		return;
	case 8:
		keycode |= cast<uint32_t>(c) << 8;
		state = 0;
		// key up
		key_up();
		return;
	}
}
uint32_t Keyboard::port_query() {
	return ~0u;
}
void Keyboard::port_send(uint8_t c) {
}

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
	VGAText::dev.setto(1,17);
	for(int x = 0; x < 12; x++) {
		VGAText::dev.puthex16(keycode[x]);
		VGAText::dev.putc(' ');
	}
	for(uint32_t k = 0; k < 12; k++) {
		VGAText::dev.setto(2+ (k*5), 18);
		if(k == 11 && keycount > 12) {
			VGAText::dev.putc('>');
		} else if(k == keycount - 1) {
			VGAText::dev.putc('^');
		} else {
			VGAText::dev.putc(' ');
		}
	}
	VGAText::dev.setto( (12*5), 18);
	VGAText::dev.puthex16(keycount);
}
static void kb_diag(const char *m, uint32_t u) {
	VGAText::dev.setto(2, 16);
	VGAText::dev.putstr(m);
	VGAText::dev.puthex32(u);
}
static void kb_diag2(const char *m, uint32_t u, uint32_t l) {
	VGAText::dev.setto(2, 15 - l);
	VGAText::dev.putstr(m);
	VGAText::dev.puthex32(u);
}

uint8_t PS2::ps2_get_read(bool force) {
	uint32_t d = _ivix_int_n + 2000;
	uint8_t s=_ix_inb(0x64);
	while((force || (d < _ivix_int_n)) && !((s=_ix_inb(0x64)) & 1)) {
		if(interupted) {
			interupted = 0;
			keycount = 0;
			return icode[0];
		}
	}
	if(interupted) {
		interupted = 0;
		keycount = 0;
		return icode[0];
	}
	if( (_ix_inb(0x64) & 1) == 0 ) return 0;
	uint8_t r = _ix_inb(0x60);
	return r;
}
static void ps2_wait_fwrite() {
	uint8_t sb;
	do { sb = _ix_inb(0x64);
	//kb_diag("fwritew  ", sb);
	}
	while(
		(sb & 2) && (~sb & 0xc0)
	);
}
static void ps2_wait_write() {
	uint8_t sb;
	uint32_t kw = _ivix_int_n + 5;
	do { sb = _ix_inb(0x64);
	kb_diag(" writew  ", sb);
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
	send_cmd(0xfe); // toggle reset for the system
}
bool PS2::init() {
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
			return false;
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
		return false;
	}

	send_cmd(0xA9); // port 2 test
	ps2_wait_fwrite();
	t = ps2_get_read(true);
	push_keycode(t | 0xA900);
	if(t != 0) {
		err_n |= 0x800;
	}
	if(err_n & 0x088) return false;

	c |= 0x03;
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
		return false;
	}

	send_cmd(0xAE); // port 1 ena
	send_cmd(0xA8); // port 2 ena

	send_cmd(0xD4);
	send_data(0xff);

	{
	uint32_t d = _ivix_int_n + 20;
	while( !(interupted & 2) && (d > _ivix_int_n) ) {
	}
	if(interupted & 2) {
		err_n |= 0x200;
	} else {
		port[1].status = PS2ST::DISABLE;
	}
	}

	cinit = true;
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
	//c->set_client_callback(this, u);
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
	return (_ivix_int_n > port[0].nextcheck)
		|| (_ivix_int_n > port[1].nextcheck)
		|| (keycount > 0)
		|| (interupted != 0);
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
		c = port_query(0);
		i = (c >> 8) & 0xff;
		kb_diag("KEY CON ", i | (c << 16));
		c &= 0xff;
		VGAText::dev.puthex32(cast<uint32_t>(port[i].status));
		if(i > 3) {
			kb_diag2("KEY RNG ", i, 1+i);
			break;
		}
		switch(port[i].status) {
		case PS2ST::INIT:
			kb_diag2("INIT HL ", i, 1+i);
			break;
		case PS2ST::LOST:
			kb_diag2("LOST HL ", i, 1+i);
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
			kb_diag2("ECHO HL ", i, 1+i);
			if(c == 0xFE) {
				if(istatus[i] & 0xC0) port[i].err_c++;
				//port_senda(0xEE, i); // echo
				if(port[i].err_c > 3) {
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
			kb_diag2("IDLE HL ", i, 1+i);
			port[i].status = PS2ST::BUSY;
			port[i].nextcheck = _ivix_int_n + 50;
			if(port[i].cl_ptr) {
				port[i].cl_ptr->port_data(cast<uint8_t>(c));
			}
			break;
		case PS2ST::BUSY:
			kb_diag2("BUSY HL ", i, 1+i);
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
			kb_diag2("IDNT HL ", i | (c << 16), 4+i);
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
				nextst = PS2ST::LOST;
			} else if(c == 0xFAu) {
				nextst = idst;
			} else if(c == 0xFCu) {
				nextst = idst;
			} else {
				//nextst = PS2ST::LOST;
				nextst = PS2ST::IDENT2;
			}
			if(dettype != port[i].type) {
				port[i].type = dettype;
				port[i].status = PS2ST::INIT;
			} else {
				port[i].status = nextst;
			}
			}
			break;
		case PS2ST::IDENT1:
		case PS2ST::IDENTE1:
			kb_diag2("ID1 HL  ", i | (c << 16), 4+i);
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
			kb_diag2("ID2 HL  ", i | (c << 16), 4+i);
			break;
		}
	}
	for(unsigned i = 0; i < 2; i++) {
		uint32_t f = 1 << i;
		if(port[i].status == PS2ST::DISABLE) {
			continue;
		}
		if(_ivix_int_n > port[i].nextcheck) {
			if( (interupted&f) ) interupted &= ~f;
			port[i].nextcheck = _ivix_int_n + 75;
			switch(port[i].status) {
			case PS2ST::IDLE:
				if(!ps2_can_write()) break;
				//port[i].status = PS2ST::ECHO;
				port[i].status = PS2ST::IDENTE;
				kb_diag("TO ECHO ", i);
				if(port[i].type == PS2TYPE::KEYBOARD) {
					//port_senda(0xEE, i); // echo
					port_senda(0xF2, i); // ident
				} else {
					port_senda(0xF2, i); // ident
				}
				break;
			case PS2ST::BUSY:
				port[i].nextcheck = _ivix_int_n + 50;
				port[i].status = PS2ST::IDLE;
				break;
			case PS2ST::LOST:
				port[i].nextcheck = _ivix_int_n + 50;
				port[i].type = PS2TYPE::QUERY;
				if(!ps2_can_write()) break;
				kb_diag("LOST ID ", i);
				port_senda(0xF2, i); // ident
				kb_diag("LOST id ", i);
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
				port[i].nextcheck = _ivix_int_n + 100;
				port[i].status = PS2ST::LOST;
				break;
			default:
				break;
			}
		}
		if(port[i].status == PS2ST::INIT) {
			switch(port[i].type) {
			case PS2TYPE::MOUSE3:
			case PS2TYPE::MOUSE4:
			case PS2TYPE::MOUSE5:
				port[i].status = PS2ST::IDLE;
				break;
			case PS2TYPE::KEYBOARD:
				init_kbd(i);
				port[i].type = PS2TYPE::KEYBOARD;
				break;
			default:
				port[i].type = PS2TYPE::QUERY;
				port[i].status = PS2ST::IDENT;
				port_senda(0xF2, i); // ident
				break;
			}
		}
	}
}
void PS2::port_enable(uint32_t u, bool e) {
	if(u > 1) return;
	if(e) {
		if(port[u].status == PS2ST::DISABLE)
			port[u].status = PS2ST::LOST;
	} else {
		port[u].status = PS2ST::DISABLE;
	}
}
uint32_t PS2::port_query(uint32_t p) {
	uint32_t f = 1 << p;
	if(keycount > 0) {
		interupted &= ~f;
		keycount --;
		if(keycount > 12) return keycode[11];
		return keycode[keycount];
	}
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
void PS2::port_senda(uint8_t km, uint32_t ex) {
	if(ex == 0) {
		send_adata(km);
	} else if(ex == 1) {
		send_cmd(0xD4);
		send_adata(km);
	}
}

} // hw

