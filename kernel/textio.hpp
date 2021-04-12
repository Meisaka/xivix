/* ***
 * textio.hpp - interface for console like devices
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

#ifndef TEXTIO_HAI
#define TEXTIO_HAI

#include "ktypes.hpp"

namespace xiv {
struct TextIO {
	virtual void reset() {};
	virtual void setto(uint16_t c, uint16_t r) = 0;
	virtual uint16_t getrow() const = 0;
	virtual uint16_t getcol() const = 0;
	virtual void putc(char c) = 0;
	virtual void putat(uint16_t c, uint16_t r, char v) = 0;
	virtual void nextline() = 0;
};

} // ns xiv

#endif
