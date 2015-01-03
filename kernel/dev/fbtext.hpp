#ifndef FBTEXT_HAI
#define FBTEXT_HAI

#include "ktypes.hpp"
#include "textio.hpp"

class FramebufferText final : public xiv::TextIO {
private:
	uint8_t *fbb;
	uint32_t pitch;
	uint32_t bitmode;
	uint32_t col;
	uint32_t row;
	uint32_t hlim;
	uint32_t vlim;
public:
	FramebufferText(void *vm, uint32_t p, uint8_t bits);
	~FramebufferText();

	void dispchar32(uint8_t c, uint32_t x, uint32_t y);

	void setto(uint16_t c, uint16_t r) override;
	uint16_t getrow() const override;
	uint16_t getcol() const override;
	void putc(char c) override;
	void putat(uint16_t c, uint16_t r, char v) override;
	void nextline() override;
};

#endif

