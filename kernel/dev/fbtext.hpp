/* ***
 * fbtext.hpp - Class decl for the simple graphical console
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
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
	uint32_t fgcolor;
	uint32_t bgcolor;
	xiv::vector2<uint32_t> origin;
	void dispchar32(uint8_t c, uint8_t *lp, uint32_t scanline);
public:
	FramebufferText(void *vm, uint32_t p, uint8_t bits);
	~FramebufferText();
	void resetpointer(void*);

	void setoffset(uint32_t x, uint32_t y); // top/left of "window"
	void render_vc(xiv::VirtTerm &);

	void setto(uint16_t c, uint16_t r) override;
	uint16_t getrow() const override;
	uint16_t getcol() const override;
	void putc(char c) override;
	void putat(uint16_t c, uint16_t r, char v) override;
	void nextline() override;
};

#endif

