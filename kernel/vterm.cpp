/* ***
 * vterm.cpp - Virtual Terminal
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

#include "vterm.hpp"
#include "memory.hpp"

namespace xiv {

VirtTerm::VirtTerm(uint16_t w, uint16_t h) {
	uint32_t sz = w * h;
	height = h;
	width = w;
	col = 0;
	row = 0;
	buffer = new VTCell[sz];
	for(uint32_t t = 0; t < sz; t++) {
		buffer[t].code = 32;
		buffer[t].attr = 0;
	}
}
VirtTerm::~VirtTerm() {
	delete buffer;
}
void VirtTerm::setto(uint16_t x, uint16_t y) {
	if(x >= width) x = width - 1;
	if(y >= height) y = height - 1;
	col = x;
	row = y;
}
uint16_t VirtTerm::getrow() const {
	return row;
}
uint16_t VirtTerm::getcol() const {
	return col;
}
void VirtTerm::putc(char c) {
	buffer[col + (width * row)].code = c;
	col++;
	if(col >= width) {
		col = 0;
		nextline();
	}
}
void VirtTerm::putat(uint16_t x, uint16_t y, char v) {
	if(x >= width) x = width - 1;
	if(y >= height) y = height - 1;
	buffer[x + (width * y)].code = v;
}
void VirtTerm::nextline() {
	col = 0;
	row++;
	if(row >= height) {
		row = height - 1;
		VTCell *pr = buffer;
		VTCell *cr = buffer+width;
		for(int y = 1; y < height; y++) {
			for(int x = 0; x < width; x++) {
				*pr = *cr;
				pr++; cr++;
			}
		}
		for(int x = 0; x < width; x++) {
			pr->code = 32;
			pr++;
		}
	}
}

}

