/* ***
 * vterm.hpp - Header for virtual terminal
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

#ifndef VTERM_HAI
#define VTERM_HAI

#include "ktypes.hpp"
#include "textio.hpp"

namespace xiv {

struct VTCell {
	uint32_t code;
	uint32_t attr;
};

constexpr uint32_t ATTR_UPDATE = 0x80000000;

class VirtTerm final : public TextIO {
private:
	uint16_t col;
	uint16_t row;
public:
	VTCell *buffer;
	uint16_t height;
	uint16_t width;
	uint32_t cattr;
public:
	VirtTerm() {}
	VirtTerm(uint16_t w, uint16_t h);
	~VirtTerm();
	void reset() override;
	void setto(uint16_t x, uint16_t y) override;
	uint16_t getrow() const override;
	uint16_t getcol() const override;
	void putc(char c) override;
	void putat(uint16_t x, uint16_t y, char v) override;
	void nextline() override;
	void setattr(uint32_t s);
};

}

#endif
