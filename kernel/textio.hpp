/* ***
 * textio.hpp - interface for console like devices
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
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
