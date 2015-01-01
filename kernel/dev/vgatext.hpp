#ifndef VGATEXT_HAI
#define VGATEXT_HAI

#include "ktypes.hpp"

class VGAText final {
private:
	constexpr static uint16_t *vgabase = (uint16_t*)0xB8000;
	uint16_t col;
	uint16_t row;
	uint16_t height;
	uint16_t width;
public:
	static VGAText dev;
	VGAText();
	~VGAText();
	void setto(uint16_t c, uint16_t r);
	void putc(char c);
	void putat(uint16_t c, uint16_t r, char v);
	void putstr(const char* s);
	void puthex32(uint32_t v);
	void puthex16(uint32_t v);
	void puthex8(uint32_t v);
};

#endif

