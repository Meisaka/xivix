/* ***
 * vgatext.hpp - class decl for text mode console driver
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

#ifndef VGATEXT_HAI
#define VGATEXT_HAI

#include "ktypes.hpp"
#include "textio.hpp"

class VGAText final : public xiv::TextIO {
private:
	uint16_t col;
	uint16_t row;
	uint16_t height;
	uint16_t width;
	uint16_t attrib;
public:
	VGAText();
	~VGAText();
	void setfg(uint8_t clr);
	void setbg(uint8_t clr);
	void setattrib(uint8_t clr);
	void setto(uint16_t c, uint16_t r);
	uint16_t getrow() const { return row; }
	uint16_t getcol() const { return col; }
	void putc(char c);
	void putat(uint16_t c, uint16_t r, char v);
	void nextline();
};

#endif

