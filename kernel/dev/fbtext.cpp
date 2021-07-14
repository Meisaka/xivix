/* ***
 * fbtext.cpp - The graphical console driver, includes bitmap font
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "fbtext.hpp"

static const uint32_t ec1242[] = {
#include "ec-font.inc"
};

static const uint32_t ec_font_w = 6;
static const uint32_t ec_font_h = 8;

static const uint32_t ufgcolors[16] = {
	0x00888888,
	0x00ee4444,
};
static const uint32_t ubgcolors[16] = {
	0x00000022,
};
static inline uint32_t roll(uint32_t v) {
	return (v << 1) | (v >> 31);
}
static inline uint32_t roll8(uint32_t v) {
	return (v << 8) | (v >> 24);
}
static inline uint32_t rolr8(uint32_t v) {
	return (v << 24) | (v >> 8);
}

void FramebufferText::dispchar32(uint8_t c, uint8_t *lp, uint32_t scanline) {
	uint8_t *ecp = (uint8_t*)(ec1242+(c*2));
	uint32_t ev = ecp[scanline] >> 1;
	uint32_t *cp = reinterpret_cast<uint32_t*>(lp);
	for(uint32_t ix = ec_font_w; ix-- ;cp++) {
		*cp = (ev & 1) ? fgcolor:bgcolor;
		ev >>= 1;
	}
}
void FramebufferText::resetpointer(void *vm) {
	fbb = reinterpret_cast<uint8_t*>(vm);
}
void FramebufferText::setoffset(uint32_t x, uint32_t y) {
	origin.x = x;
	origin.y = y;
}
void FramebufferText::render_vc(xiv::VirtTerm &vc) {
	uint32_t bytesperpix = 4;
	uint32_t xco = origin.x * bytesperpix;
	uint32_t ro = 0;
	uint32_t roline = 0;
	uint8_t scanline = 0;
	uint32_t fheight = vc.height * ec_font_h;
	uint32_t xadvance = bytesperpix * ec_font_w;
	uint8_t *lp = fbb + (pitch * origin.y);
	uint32_t lastatt = 0x100;
	for(uint32_t y = 0; y < fheight; y++) {
		uint8_t *lcp = lp + xco;
		ro = roline;
		for(uint32_t x = 0; x < vc.width; x++) {
			uint32_t att = vc.buffer[ro].attr;
			if(att & xiv::ATTR_UPDATE) {
				if((att & 0xff) != lastatt) {
					fgcolor = ufgcolors[att & 0xf];
					bgcolor = ubgcolors[(att >> 4) & 0xf];
					lastatt = att & 0xff;
				}
				dispchar32(cast<uint8_t>(vc.buffer[ro].code), lcp, scanline);
			}
			lcp += xadvance;
			ro++;
		}
		lp += pitch;
		if(++scanline == ec_font_h) {
			ro = roline;
			for(uint32_t x = 0; x < vc.width; x++) {
				vc.buffer[ro].attr &= ~xiv::ATTR_UPDATE;
				ro++;
			}
			scanline = 0;
			roline = ro;
		}
	}
}

FramebufferText::FramebufferText(void *vm, uint32_t p, uint8_t bits) {
	fbb = reinterpret_cast<uint8_t*>(vm);
	pitch = p;
	switch(bits) {
	case 32:
		bitmode = 0;
		break;
	case 24:
		bitmode = 1;
		break;
	case 16:
		bitmode = 2;
		break;
	case 15:
		bitmode = 3;
		break;
	case 8:
		bitmode = 4;
		break;
	}
	col = 0;
	row = 0;
	hlim = 90;
	vlim = 90;
	fgcolor = ufgcolors[0];
	bgcolor = ubgcolors[0];
}
FramebufferText::~FramebufferText() {
}

void FramebufferText::setto(uint16_t c, uint16_t r) {
	if(r > vlim) row = vlim; else row = r;
	if(c > hlim) col = hlim; else col = c;
}
uint16_t FramebufferText::getrow() const {
	return row;
}
uint16_t FramebufferText::getcol() const {
	return col;
}
void FramebufferText::putc(char c) {
	putat(col, row, c);
	col++;
	if(col >= hlim) {
		nextline();
	}
}
void FramebufferText::putat(uint16_t c, uint16_t r, char v) {
	uint8_t *lp = fbb + (pitch * (origin.y + (r * ec_font_h)));
	uint32_t bytesperpix = 4;
	uint32_t xco = origin.x * bytesperpix;
	uint32_t xadvance = bytesperpix * ec_font_w;
	lp += xco + xadvance * c;
	for(uint32_t y = 0; y < ec_font_h; y++) {
		dispchar32(v, lp, y);
		lp += pitch;
	}
}
void FramebufferText::nextline() {
	col = 0;
	row++;
	if(row >= vlim) {
		row = 0;
	}
}
