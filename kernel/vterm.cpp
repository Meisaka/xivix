/* ***
 * vterm.cpp - Virtual Terminal
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "vterm.hpp"
#include "memory.hpp"

namespace xiv {

VirtTerm::VirtTerm(uint16_t w, uint16_t h) {
	uint32_t sz = w * h;
	height = h;
	width = w;
	buffer = new VTCell[sz];
	reset();
}
VirtTerm::~VirtTerm() {
	delete buffer;
}
void VirtTerm::reset() {
	col = 0;
	row = 0;
	cattr = 0;
	uint32_t sz = width * height;
	for(uint32_t t = 0; t < sz; t++) {
		buffer[t] = {.code = 32, .attr = ATTR_UPDATE};
	}
}
void VirtTerm::setto(uint16_t x, uint16_t y) {
	if(x >= width) x = width - 1;
	if(y >= height) y = height - 1;
	col = x;
	row = y;
}
void VirtTerm::setattr(uint32_t a) {
	cattr = a & 0x00ffffff;
}
uint16_t VirtTerm::getrow() const {
	return row;
}
uint16_t VirtTerm::getcol() const {
	return col;
}
void VirtTerm::putc(char c) {
	buffer[col + (width * (uint32_t)row)] = {.code = (uint8_t)c, .attr = cattr | ATTR_UPDATE};
	col++;
	if(col >= width) {
		col = 0;
		nextline();
	}
}
void VirtTerm::putat(uint16_t x, uint16_t y, char v) {
	if(x >= width) x = width - 1;
	if(y >= height) y = height - 1;
	buffer[x + (width * (uint32_t)y)] = {.code = (uint8_t)v, .attr = cattr | ATTR_UPDATE};
}
void VirtTerm::nextline() {
	col = 0;
	row++;
	if(row >= height) {
		row = height - 1;
		VTCell *pr = buffer;
		VTCell *cr = buffer+(uint32_t)width;
		for(int y = 1; y < height; y++) {
			for(int x = 0; x < width; x++) {
				if(pr->code != cr->code || ((pr->attr ^ cr->attr) & 0xffffff)) {
					cr->attr |= ATTR_UPDATE;
				}
				*pr = *cr;
				pr++; cr++;
			}
		}
		for(int x = 0; x < width; x++) {
			pr->code = 32;
			pr->attr = cattr | ATTR_UPDATE;
			pr++;
		}
	}
}

}
