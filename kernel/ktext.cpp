/* ***
 * ktext.cpp - text output functions
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "ktext.hpp"
#include "dev/vgatext.hpp"
#include "dev/fbtext.hpp"
#include "kio.hpp"

namespace xiv {

TextIO *txtout = nullptr;
VirtTerm *txtvc = nullptr;
FramebufferText *txtfb = nullptr;
bool use_comm = true;

void togglecomm() {
	use_comm = !use_comm;
}
void putc(char c) {
	if(use_comm) ixcom_putc(c);
	if(txtout == nullptr) return;
	switch(c) {
	case 10:
		txtout->nextline();
		if(txtfb && txtout == txtvc) {
			txtfb->render_vc(*txtvc);
		}
		break;
	default:
		txtout->putc(c);
	}
}
void iputc(TextIO *out, char c) {
	if(out == txtout) { // special case
		putc(c);
		return;
	}
	if(c == 10) {
		out->nextline();
	} else {
		out->putc(c);
	}
}

void printattr(uint32_t x) {
	if(txtvc) {
		txtvc->setattr(x);
	}
}
void printn(const char* s, size_t n) {
	while(n--) { putc(*(s++)); }
}
void iprintn(TextIO *out, const char* s, size_t n) {
	while(n--) { iputc(out, *(s++)); }
}

void print(const char* s) {
	while(*s != 0) { putc(*(s++)); }
}
void iprint(TextIO *out, const char* s) {
	while(*s != 0) { iputc(out, *(s++)); }
}

void iprintdec(TextIO *out, uint64_t d, uint32_t s, int sf) {
	char num[20];
	unsigned l = 0;
	if(!d) {
		num[19] = '0';
		l = 1;
	} else while(d) {
		num[19-l] = (d % 10) + '0';
		d = d / 10;
		l++;
	}
	while(l < s) {
		iputc(out, sf);
		s--;
	}
	iprintn(out, num+(20-l), l);
}

void iprinthex(TextIO *out, uint32_t v) {
	int i;
	for(i = 32; i > 4 && ((v >> (i-4)) & 0x0f) == 0; i-=4); // get digit length
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		iputc(out, c);
	} while(i > 0);
}

void iprinthexs(TextIO *out, uint32_t v) {
	int i;
	for(i = 32; i > 4 && ((v >> (i-4)) & 0x0f) == 0; i-=4) iputc(out, ' '); // get digit length
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		iputc(out, c);
	} while(i > 0);
}

void iprinthex(TextIO *out, uint64_t v) {
	int i;
	for(i = 64; i > 4 && ((v >> (i-4)) & 0x0f) == 0; i-=4); // get digit length
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		iputc(out, c);
	} while(i > 0);
}
void iprinthexs(TextIO *out, uint64_t v, uint32_t s, int sf) {
	int i;
	for(i = s * 4; i > 4 && ((v >> (i-4)) & 0x0f) == 0; i-=4) iputc(out, sf); // get digit length
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		iputc(out, c);
	} while(i > 0);
}
void iprinthex(TextIO *out, uint32_t v, uint32_t bits) {
	if(bits > 32) bits = 32;
	int i = bits ^ (bits & 0x3);
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		iputc(out, c);
	} while(i > 0);
}
void iprinthexx(TextIO *out, uint64_t v, uint32_t bits) {
	if(bits > 64) bits = 64;
	int i = bits ^ (bits & 0x3);
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		iputc(out, c);
	} while(i > 0);
}

#define PMODE_LONG 3
void iprintf(TextIO *out, const char *ftr, ...) {
	va_list v;
	va_start(v, ftr);
	ivprintf(out, ftr, v);
	va_end(v);
}
void printf(const char *ftr, ...) {
	va_list v;
	va_start(v, ftr);
	ivprintf(txtout, ftr, v);
	va_end(v);
}
void ivprintf(TextIO *out, const char *ftr, va_list v) {
	unsigned rs = 0;
	int pad = 0;
	int padl = 0;
	uint32_t num;
	uint64_t numx;

	while(*ftr) {
		switch(rs) {
		case 0:
			if(*ftr == '%') {
				rs = 1;
				pad = 0;
			} else {
				iputc(out, *ftr);
			}
			break;
		case 1:
			rs = 0;
			switch(*ftr) {
			case '0':
				pad = 1;
				rs = 1;
				padl = 8;
				break;
			case ' ':
				pad = 2;
				rs = 1;
				padl = 8;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				rs = 2;
				padl = *ftr - '0';
				break;
			case 'l':
				rs = PMODE_LONG;
				break;
			case '%':
				iputc(out, *ftr);
				break;
			case 'c':
				iputc(out, va_arg(v, int));
				break;
			case 'd':
				num = va_arg(v, uint32_t);
				iprintdec(out, num, padl, pad == 2 ? ' ':'0');
				break;
			case 's':
				iprint(out, va_arg(v, const char *));
				break;
			case 'x':
				num = va_arg(v, uint32_t);
				if(pad) iprinthexs(out, num, padl, pad == 2 ? ' ':'0');
				else iprinthex(out, num);
				break;
			default:
				iputc(out, '%');
				iputc(out, *ftr);
			}
			break;
		case 2:
			rs = 0;
			switch(*ftr) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				padl = padl * 10 + (*ftr - '0');
				rs = 2;
				break;
			case 'l':
				rs = PMODE_LONG;
				break;
			case 'd':
				num = va_arg(v, uint32_t);
				iprintdec(out, num, padl, pad == 2 ? ' ':'0');
				break;
			case 's':
				iprint(out, va_arg(v, const char *));
				break;
			case 'x':
				num = va_arg(v, uint32_t);
				if(pad) iprinthexs(out, num, padl, pad == 2 ? ' ':'0');
				else iprinthex(out, num);
				break;
			default:
				iputc(out, '%');
				iputc(out, *ftr);
			}
			break;
		case PMODE_LONG:
			rs = 0;
			switch(*ftr) {
			case '0':
				pad = 1;
				rs = 2;
				break;
			case ' ':
				pad = 2;
				rs = 2;
				break;
			case 'd':
				numx = va_arg(v, uint64_t);
				iprintdec(out, numx, padl, pad == 2 ? ' ':'0');
				break;
			case 'x':
				numx = va_arg(v, uint64_t);
				if(pad) iprinthexs(out, numx, padl, pad == 2 ? ' ':'0');
				else iprinthex(out, numx);
				break;
			default:
				iputc(out, '%');
				iputc(out, *ftr);
			}
			break;
		}
		ftr++;
	}
}

}
