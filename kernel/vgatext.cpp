
#include "vgatext.hpp"

VGAText::VGAText() {
	col = 0;
	row = 0;
	height = 25;
	width = 80;
}

void VGAText::putc(char c) {
	vgabase[col + (width*row)] = 0x1200 | c;
	col++;
	if(col >= width) {
		col = 0;
		row++;
		if(row >= height) {
			row = 0;
		}
	}
}
void VGAText::putstr(const char* s) {
	while(*s != 0) { putc(*(s++)); }
}
void VGAText::puthex32(uint32_t v) {
	int i = 32;
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		putc(c);
	} while(i > 0);
}
void VGAText::setto(uint16_t c, uint16_t r) {
	row = r;
	col = c;
}
void VGAText::putat(uint16_t c, uint16_t r, char v) {
	vgabase[c + (width * r)] = 0x1200 | v;
}

