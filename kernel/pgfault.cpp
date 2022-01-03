/* ***
 * pgfault.cpp - Page fault handler
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "kio.hpp"
#include "ktext.hpp"
#include "ktypes.hpp"
#include "interrupt.hpp"

namespace xiv {

void pfhandle(void *, uint32_t, IntCtx *ctx) {
	printattr(1);
	printf("*** Page fault! from %0x ***\n", ctx->ih.r_eip);
	printf("CS: %x FLAGS: %0x  \n", ctx->ih.r_cs, ctx->ih.r_eflag);
	printf("code: %x - ", ctx->ecode);
	print(ctx->ecode & 1 ? "PRESENT " : "NONPRESENT ");
	print(ctx->ecode & 4 ? "USER " : "KERNEL ");
	print(ctx->ecode & 2 ? "WRITE" : "READ");
	if(ctx->ecode & 8) print(" RESERVE-BIT");
	if(ctx->ecode & 16) print(" INST-FETCH");
	if(ctx->ecode & 0x8000) print(" <<SGX>>");
	printf(" At Address %0x  \n", ctx->extra);
	printf("EAX:%0x  EBX:%0x\nECX:%0x  EDX:%0x  \n"
		"ESI:%0x  EDI:%0x\nEBP:%0x  ESP:%0x  \n",
		ctx->ir.r_eax, ctx->ir.r_ebx,
		ctx->ir.r_ecx, ctx->ir.r_edx,
		ctx->ir.r_esi, ctx->ir.r_edi,
		ctx->ir.r_ebp, ctx->ir.r_esp_hdl);
	printf("kernel stack:  \n");
	uint32_t *fcs = (uint32_t*)ctx->ir.r_esp_hdl;
	fcs += 4; // skip the IH stuff
	for(int y = 0; y < 8; y++) {
		printf("%0x:", fcs);
		for(int x = 0; x < 4; x++) {
			printf(" %0x", fcs[x]);
		}
		printf("  \n");
		fcs += 4;
	}
	printf("kernel calls:  \n");
	fcs = (uint32_t*)ctx->ir.r_ebp;
	for(int y = 0; (y < 8) && ((uint32_t)fcs > 0xc0000000); y++) {
		printf("%0x: %0x  \n", y, fcs[1]);
		fcs = (uint32_t*)(fcs[0]);
	}
	printf("*** End PF ***\n");
	printattr(0);
}

void pfinit() {
	iv_except[0xe] = {pfhandle, nullptr};
}

}
