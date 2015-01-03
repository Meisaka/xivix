#ifndef KTEXT_HAI
#define KTEXT_HAI

#include "ktypes.hpp"

namespace xiv {
void printn(const char *, size_t);
void print(const char *);
void printdec(uint32_t d);
void printhex(uint32_t v);
void printhex(uint32_t v, uint32_t bits);
void printf(const char *ftr, ...);
} // ns xiv
#endif

