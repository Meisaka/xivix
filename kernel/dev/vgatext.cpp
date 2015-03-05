
#include "vgatext.hpp"

constexpr static uint16_t *vgabase = (uint16_t*)0xc00B8000;

VGAText::VGAText() {
	col = 0;
	row = 0;
	height = 25;
	width = 80;
	attrib = 0x0c00;
}
VGAText::~VGAText() {}

void VGAText::putc(char c) {
	vgabase[col + (width*row)] = attrib | c;
	col++;
	if(col >= width) {
		nextline();
	}
}
void VGAText::setfg(uint8_t clr) {
	attrib = (attrib & 0xf000) | ((clr & 0xf) << 8);
}
void VGAText::setbg(uint8_t clr) {
	attrib = (attrib & 0xf00) | ((clr & 0xf) << 12);
}
void VGAText::setattrib(uint8_t clr) {
	attrib = (clr & 0x0ff) << 8;
}
void VGAText::nextline() {
	col = 0;
	row++;
	if(row >= height) {
		row = 0;
	}
}
void VGAText::setto(uint16_t c, uint16_t r) {
	if(r > height) r = height;
	if(c > width) c = width;
	row = r;
	col = c;
}
void VGAText::putat(uint16_t c, uint16_t r, char v) {
	vgabase[c + (width * r)] = attrib | v;
}

