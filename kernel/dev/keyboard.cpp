
#include "vgatext.hpp"
#include "ps2.hpp"
#include "kio.hpp"

namespace hw {

Keyboard::Keyboard() {
	state = 0;
	istate = 0;
}

Keyboard::~Keyboard() {
}

void Keyboard::push_key(uint32_t k) {
	for(int i = 15; i > 0; i--) {
		keybuf[i] = keybuf[i-1];
	}
	keybuf[0] = k;
	if(keyev < 16) keyev++;
}
uint32_t Keyboard::pop_key() {
	if(keyev > 0) {
		return keybuf[keyev--];
	} else {
		return 0;
	}
}
bool Keyboard::has_key() {
	return keyev > 0;
}

void Keyboard::init_proc() {
}

uint8_t Keyboard::kbd_cmd(uint8_t v) {
	int i = 6;
	if(!mp_serv) return 0xFE;
	while(i > 0) {
		i--;
		mp_serv->client_send(v, mp_port);
		uint32_t rc = mp_serv->client_req(mp_port);
		if(rc == ~0u) continue;
		rc &= 0xff;
		if(rc != 0x0FE) {
			return rc;
		}
	}
	return 0xFE;
}
void Keyboard::kbd_data(uint8_t v) {
	if(!mp_serv) return;
	mp_serv->client_send(v, mp_port);
}

bool Keyboard::init() {
	mods = 0;
	state = 0;
	keyev = 0;
	if(!mp_serv) return false;
	uint32_t es = 0;
	if(kbd_cmd(0xff) != 0xAA) es++;
	kbd_data(0xed);// set code set ...
	if(kbd_cmd(0x07) != 0xFA) es++; // setled ...
	kbd_data(0xf0);// get code set ...
	if(kbd_cmd(0x02) != 0xFA) es++;
	kbd_data(0xf0);
	if(kbd_cmd(0x00) != 0xFA) es++;
	if(es > 3) {
		return false;
	}
	mp_serv->client_req(mp_port);
	kbd_data(0xf3);// set rate ...
	if(kbd_cmd(0x02) != 0xFA) es++;
	if(kbd_cmd(0xf4) != 0xFA) es++;// scan
	kbd_data(0xed);// setled ...
	if(kbd_cmd(0x03) != 0xFA) es++;
	return true;
}

void Keyboard::key_down() {
	switch(keycode & 0x2ffff) {
	case 0x12:
		mods |= 0x01;
		break;
	case 0x59:
		mods |= 0x02;
		break;
	case 0x14:
		mods |= 0x04;
		break;
	case 0x20014:
		mods |= 0x08;
		break;
	case 0x11:
		mods |= 0x10;
		break;
	case 0x20011:
		mods |= 0x20;
		break;
	case 0x2001f:
		mods |= 0x40;
		break;
	case 0x20027:
		mods |= 0x80;
		break;
	case 0x2002f:
		mods |= 0x100;
		break;
	}
	if( ((mods & 0x30) != 0) && ((mods & 0xC) != 0) ) {
		VGAText::dev.setto(1,17);
		VGAText::dev.putstr("SYSTEM REQ ");
		VGAText::dev.puthex32(keycode);
	}
	push_key(keycode);
	lastkey1 = keycode;
}

void Keyboard::key_up() {
	switch(keycode & 0x2ffff) {
	case 0x12:
		mods ^= mods & 0x01;
		break;
	case 0x59:
		mods ^= mods & 0x02;
		break;
	case 0x14:
		mods ^= mods & 0x04;
		break;
	case 0x20014:
		mods ^= mods & 0x08;
		break;
	case 0x11:
		mods ^= mods & 0x10;
		break;
	case 0x20011:
		mods ^= mods & 0x20;
		break;
	case 0x2001f:
		mods ^= mods & 0x40;
		break;
	case 0x20027:
		mods ^= mods & 0x80;
		break;
	case 0x2002f:
		mods ^= mods & 0x100;
		break;
	}
	if( ((mods & 0x30) != 0) && ((mods & 0xC) != 0) ) {
		VGAText::dev.setto(1,17);
		VGAText::dev.putstr("SYSTEM REQ ");
		VGAText::dev.puthex32(keycode);
		if(keycode == 0x30071) {
			PS2::system_reset();
		}
	}
	push_key(keycode);
	lastkey2 = lastkey1;
	lastkey1 = keycode;
}

void Keyboard::remove() {
	mods = 0;
	state = 0;
	keyev = 0;
}
void Keyboard::port_data(uint8_t c) {
	lastscan = c;
	if(c > 0xF0) return; // got control code
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
	case 4: // 0xE1 codes
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

} // hw

