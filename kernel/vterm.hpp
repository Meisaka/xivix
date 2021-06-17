/* ***
 * vterm.hpp - Header for virtual terminal
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
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
