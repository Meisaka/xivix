/* ***
 * vgatext.hpp - class decl for text mode console driver
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
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

