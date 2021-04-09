/* ***
 * ktext.cpp - text output functions
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
 * or visit: http://www.gnu.org/licenses/gpl-2.0.txt
 */
#include "ktext.hpp"
#include "dev/vgatext.hpp"
#include "dev/fbtext.hpp"
#include <stdarg.h>

namespace xiv {

TextIO *txtout = nullptr;
VirtTerm *txtvc = nullptr;
FramebufferText *txtfb = nullptr;

void putc(char c) {
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
void printattr(uint32_t x) {
	if(txtvc) {
		txtvc->setattr(x);
	}
}
void printn(const char* s, size_t n) {
	while(n--) { putc(*(s++)); }
}

void print(const char* s) {
	while(*s != 0) { putc(*(s++)); }
}

void printdec(uint64_t d, uint32_t s, int sf) {
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
		putc(sf);
		s--;
	}
	printn(num+(20-l), l);
}

void printhex(uint32_t v) {
	int i;
	for(i = 32; i > 4 && ((v >> (i-4)) & 0x0f) == 0; i-=4); // get digit length
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		putc(c);
	} while(i > 0);
}
void printhexs(uint32_t v) {
	int i;
	for(i = 32; i > 4 && ((v >> (i-4)) & 0x0f) == 0; i-=4) putc(' '); // get digit length
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		putc(c);
	} while(i > 0);
}
void printhex(uint64_t v) {
	int i;
	for(i = 64; i > 4 && ((v >> (i-4)) & 0x0f) == 0; i-=4); // get digit length
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		putc(c);
	} while(i > 0);
}
void printhexs(uint64_t v, uint32_t s, int sf) {
	int i;
	for(i = s * 4; i > 4 && ((v >> (i-4)) & 0x0f) == 0; i-=4) putc(sf); // get digit length
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		putc(c);
	} while(i > 0);
}
void printhex(uint32_t v, uint32_t bits) {
	if(bits > 32) bits = 32;
	int i = bits ^ (bits & 0x3);
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		putc(c);
	} while(i > 0);
}
void printhexx(uint64_t v, uint32_t bits) {
	if(bits > 64) bits = 64;
	int i = bits ^ (bits & 0x3);
	do {
		i -= 4;
		char c = (v >> i) & 0x0f;
		if(c > 9) c += 7;
		c += '0';
		putc(c);
	} while(i > 0);
}

#define PMODE_LONG 3
void printf(const char *ftr, ...) {
	va_list v;
	unsigned rs = 0;
	int pad = 0;
	int padl = 0;
	uint32_t num;
	uint64_t numx;

	va_start(v, ftr);

	while(*ftr) {
		switch(rs) {
		case 0:
			if(*ftr == '%') {
				rs = 1;
				pad = 0;
			} else {
				putc(*ftr);
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
				putc(*ftr);
				break;
			case 'd':
				num = va_arg(v, uint32_t);
				printdec(num, padl, pad == 2 ? ' ':'0');
				break;
			case 's':
				print(va_arg(v, const char *));
				break;
			case 'x':
				num = va_arg(v, uint32_t);
				if(pad) printhexs(num, padl, pad == 2 ? ' ':'0');
				else printhex(num);
				break;
			default:
				putc('%');
				putc(*ftr);
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
				printdec(num, padl, pad == 2 ? ' ':'0');
				break;
			case 's':
				print(va_arg(v, const char *));
				break;
			case 'x':
				num = va_arg(v, uint32_t);
				if(pad) printhexs(num, padl, pad == 2 ? ' ':'0');
				else printhex(num);
				break;
			default:
				putc('%');
				putc(*ftr);
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
				printdec(numx, padl, pad == 2 ? ' ':'0');
				break;
			case 'x':
				numx = va_arg(v, uint64_t);
				if(pad) printhexs(numx, padl, pad == 2 ? ' ':'0');
				else printhex(numx);
				break;
			default:
				putc('%');
				putc(*ftr);
			}
			break;
		}
		ftr++;
	}
	va_end(v);
}

}

