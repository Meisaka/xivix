/* ***
 * fbtext.hpp - Class decl for the simple graphical console
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

#ifndef FBTEXT_HAI
#define FBTEXT_HAI

#include "ktypes.hpp"
#include "vector2.hpp"
#include "vterm.hpp"

class FramebufferText final : public xiv::TextIO {
private:
	uint8_t *fbb;
	uint32_t pitch;
	uint32_t bitmode;
	uint32_t col;
	uint32_t row;
	uint32_t hlim;
	uint32_t vlim;
	xiv::vector2<uint32_t> origin;
public:
	FramebufferText(void *vm, uint32_t p, uint8_t bits);
	~FramebufferText();

	void setoffset(uint32_t x, uint32_t y); // top/left of "window"
	void dispchar32(uint8_t c, uint32_t x, uint32_t y);
	void render_vc(xiv::VirtTerm &);

	void setto(uint16_t c, uint16_t r) override;
	uint16_t getrow() const override;
	uint16_t getcol() const override;
	void putc(char c) override;
	void putat(uint16_t c, uint16_t r, char v) override;
	void nextline() override;
};

#endif

