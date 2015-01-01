
#include "ps2.hpp"
#include "vgatext.hpp"
#include "kio.hpp"

extern "C" {

extern void (*_ivix_irq1_fn)(void);

}
namespace hw {

PS2 PS2::dev;

void PS2::irq1_signal() {
	PS2::dev.irq1_handle();
}
PS2::PS2() {
	kmd_l = 0;
	err_n = 0;
	_ivix_irq1_fn = PS2::irq1_signal;
	nextcheck = _ivix_int_n + 20;
	cstatus = PS2ST::INIT;
}
PS2::~PS2() {}
void PS2::irq1_handle() {
	interupted++;
	istatus = _ix_inb(0x64);
	icode = _ix_inb(0x60);
	push_keycode(icode);
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
			interupted--;
			return icode;
		}
	}
	if(interupted) {
		interupted = 0;
		return icode;
	}
	if( (_ix_inb(0x64) & 1) == 0 ) return 0;
	uint8_t r = _ix_inb(0x60);
	push_keycode(r);
	return r;
}
static void ps2_wait_write() {
	uint32_t d = _ivix_int_n + 50;
	while((d < _ivix_int_n) && (_ix_inb(0x64) & 2));
}
static void ps2_wait_fwrite() {
	while(_ix_inb(0x64) & 2);
}
static bool kbd_cmd(uint8_t cc, uint32_t p, unsigned n) {
	int i = 6;
	while(i > 0) {
		i--;
		ps2_wait_fwrite();
		_ix_outb(0x60, cc);
		for(unsigned x = 0; x < n; x++) {
		ps2_wait_fwrite();
		_ix_outb(0x60, (uint8_t)p);
		}
		ps2_wait_fwrite();
		uint32_t rc;
		rc = PS2::dev.port_query();
		if(rc == ~0u) continue;
		if(rc != 0x0FE) {
			if(rc == 0xFA) return true;
		}
	}
	return false;
}
static bool kbd_cmd_ex(uint8_t cc, uint32_t ex) {
	int i = 6;
	while(i > 0) {
		i--;
		ps2_wait_fwrite();
		_ix_outb(0x60, cc);
		ps2_wait_fwrite();
		uint32_t rc;
		rc = PS2::dev.port_query();
		if(rc == ~0u) continue;
		if(rc != 0x0FE) {
			if(rc == ex) return true;
		}
	}
	return false;
}
void PS2::system_reset() {
	ps2_wait_write();
	_ix_outb(0x64, 0xfe); // toggle reset for the system
}
void PS2::init() {
	uint8_t c, t;
	err_n &= 0xF777;
	if(!(err_n & 0x4)) {
	ps2_wait_fwrite(); _ix_outb(0x64, 0xAD); // port 1 dis
	ps2_wait_fwrite(); _ix_outb(0x64, 0xA7); // port 2 dis
	ps2_wait_fwrite(); _ix_outb(0x64, 0xAA); // test
	ps2_wait_fwrite();
	t = ps2_get_read(true);
	push_keycode(t | 0xAA00);
	if(t != 0x55) { err_n |= 0x8; return; }
	err_n |= 0x4;
	}
	ps2_wait_fwrite(); _ix_outb(0x64, 0x20); // read r0
	ps2_wait_fwrite();
	c = ps2_get_read(true);
	push_keycode(c | 0x2000);
	c &= 0x34;
	ps2_wait_fwrite(); _ix_outb(0x64, 0x60); // write r0
	ps2_wait_fwrite(); _ix_outb(0x60, c); // config
	ps2_wait_fwrite(); _ix_outb(0x64, 0xAB); // port 1 test
	ps2_wait_fwrite();
	t = ps2_get_read(true);
	push_keycode(t | 0xAB00);
	if(t != 0) { err_n |= 0x80; return; }
	ps2_wait_fwrite(); _ix_outb(0x64, 0xA9); // port 2 test
	ps2_wait_fwrite();
	t = ps2_get_read(true);
	push_keycode(t | 0xA900);
	if(t != 0) { err_n |= 0x800; }
	if(err_n & 0x088) return;
	c |= 0x03;
	keycode[11] = c;
	ps2_wait_fwrite();
	_ix_outb(0x64, 0x60); // write r0
	ps2_wait_fwrite();
	push_keycode(_ix_inb(0x64) | 0x6100);
	_ix_outb(0x60, c); // config
	ps2_wait_fwrite();
	push_keycode(_ix_inb(0x64) | 0x6200);
	_ix_outb(0x64, 0x20); // read r0
	ps2_wait_fwrite();
	c = ps2_get_read(true);
	push_keycode(c | 0x2200);
	if(c & 0x1) err_n |= 0x10;
	if(c & 0x2) err_n |= 0x100;
	if((c & 0x3) == 0) {
		err_n ^= 0x4;
		return;
	}
	ps2_wait_fwrite(); _ix_outb(0x64, 0xAE); // port 1 ena
	ps2_wait_fwrite(); _ix_outb(0x64, 0xA8); // port 2 ena
	cstatus = PS2ST::INIT_KB;
}
void PS2::init_kbd() {
	uint32_t c;
	/*	_ix_outb(0x60, 0xff); // reset
	if(cstatus == PS2ST::INIT_KB) {
		do {
			c = port_query();
			if(c == ~0u) {
				ps2_wait_write();
				_ix_outb(0x60, 0xff); // reset
				return;
			}
			err_n &= 0xffff;
			err_n |= static_cast<uint32_t>(c) << 24;
		} while(c == ~0u || c == 0xFE);
		if(c != 0xAA) {
			return;
		}
	} */
	kbd_cmd_ex(0xff, 0xaa);
	kbd_cmd(0xed, 0x07, 1); // setled ...
	kbd_cmd(0xf0, 0x02, 1); // set code set ...
	kbd_cmd(0xf0, 0x00, 1); // get code set ...
	c = port_query();
	keycode[11] = cast<uint8_t>(c);
	kbd_cmd(0xf3, 0x02, 1); // set rate ...
	kbd_cmd(0xf4, 0, 0); // scan
	kbd_cmd(0xed, 0x03, 1); // setled ...
	cstatus = PS2ST::IDLE;
	//ps2_wait_write(); _ix_outb(0x60, 0xFF);
	//ps2_wait_write(); _ix_outb(0x60, 0xED);
	//ps2_wait_write(); _ix_outb(0x60, 0x03);
}


bool PS2::waiting() {
	return (_ivix_int_n > nextcheck) || (interupted != 0);
}
void PS2::handle() {
	if(_ivix_int_n > nextcheck) {
		nextcheck = _ivix_int_n + 20;
		if(interupted) interupted = 0;
		if(cstatus == PS2ST::IDLE) {
			ps2_wait_write();
			cstatus = PS2ST::ECHO;
			_ix_outb(0x60, 0xEE); // echo
		} else if(cstatus == PS2ST::LOST) {
			nextcheck = _ivix_int_n + 50;
			ps2_wait_fwrite();
			_ix_outb(0x60, 0xEE); // try echo
		}
	}
	uint32_t c;
	keycode[10] = _ix_inb(0x64);
	switch(cstatus) {
	case PS2ST::INIT:
		init();
		break;
	case PS2ST::INIT_KB:
		init_kbd();
		break;
	case PS2ST::LOST:
		c = port_query();
		if(c == 0xEE || c == 0xAA) {
			err_c = 0;
			init_kbd();
		}
		break;
	case PS2ST::ECHO:
		c = port_query();
		if(c == 0xFE) {
			if(istatus & 0xC0) err_c++;
			ps2_wait_write();
			_ix_outb(0x60, 0xEE); // echo
			if(err_c > 1) {
				cstatus = PS2ST::LOST;
			}
			break;
		} else if(c != 0xEE) {
			err_c = 0;
			err_n &= 0xffff;
			err_n |= c << 16;
		} else {
			err_c = 0;
			err_n &= 0xefff;
		}
		cstatus = PS2ST::IDLE;
		break;
	case PS2ST::IDLE:
		if(!interupted) break;
		keycode[9] = port_query();
		break;
	}
}
uint32_t PS2::port_query() {
	while(_ivix_int_n < nextcheck && !interupted);
	if(!interupted) {
		return ~0;
	} else {
		interupted = 0;
		return icode;
	}
}
void PS2::port_send(uint8_t km, uint32_t ex) {
	if(kmd_l < 16) {
		kmd_q[kmd_l++] = km;
		kmd_q[kmd_l++] = ex;
	}
}

} // hw
