/* ***
 * fbtext.cpp - The graphical console driver, includes bitmap font
 * Copyright (C) 2014-2015  Meisaka Yukara
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 *     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "fbtext.hpp"

static const uint32_t ec1242[] = {
#include "ec-font.inc"
};

static const uint32_t ec_font_w = 6;
static const uint32_t ec_font_h = 8;

static const uint32_t ufgcolors[16] = {
	0x00cccccc,
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

void FramebufferText::dispchar32(uint8_t c, uint32_t x, uint32_t y) {
	uint8_t *ecp = (uint8_t*)(ec1242+(c*2));
	uint32_t ev;
	uint8_t *lp;
	uint32_t *cp;
	lp = fbb + (pitch * y) + (x * 4);
	for(uint32_t il = 0; il < ec_font_h; il++) {
		cp = reinterpret_cast<uint32_t*>(lp);
		lp += pitch;
		ev = *(ecp++);
		for(uint32_t ix = ec_font_w; ix-- ;cp++) {
			*cp = (ev & (0x40 >> ix)) ? fgcolor:bgcolor;
		}
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
	uint32_t xco = origin.x;
	uint32_t yco = origin.y;
	uint32_t ro = 0;
	for(uint32_t y = 0; y < vc.height; y++) {
		xco = origin.x;
		for(uint32_t x = 0; x < vc.width; x++) {
			uint32_t att = vc.buffer[ro].attr;
			if(att & xiv::ATTR_UPDATE) {
				vc.buffer[ro].attr ^= xiv::ATTR_UPDATE;
				fgcolor = ufgcolors[att & 0xf];
				bgcolor = ubgcolors[(att >> 4) & 0xf];
				dispchar32(cast<uint8_t>(vc.buffer[ro].code), xco, yco);
			}
			xco += ec_font_w;
			ro++;
		}
		//ro += vc.width;
		yco += ec_font_h;
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
	dispchar32(cast<uint8_t>(v), origin.x + (c * ec_font_w), origin.y + (r * ec_font_h));
}
void FramebufferText::nextline() {
	col = 0;
	row++;
	if(row >= vlim) {
		row = 0;
	}
}

