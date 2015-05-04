/* ***
 * pgfault.cpp - Page fault handler
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
#include "kio.hpp"
#include "ktext.hpp"
#include "ktypes.hpp"
#include "interrupt.hpp"

namespace xiv {

void pfhandle(void *, uint32_t, ixexptctx *ctx) {
	printattr(1);
	printf("Page fault! from %0x\n", ctx->ih->r_eip);
	printf("CS: %x FLAGS: %0x\n", ctx->ih->r_cs, ctx->ih->r_eflag);
	printf("code: %x - ", ctx->ec_1);
	print(ctx->ec_1 & 1 ? "PROTECT " : "NONPRESENT ");
	print(ctx->ec_1 & 4 ? "USER " : "KERNEL ");
	print(ctx->ec_1 & 2 ? "WRITE" : "READ");
	if(ctx->ec_1 & 8) print(" RESRVD-SET");
	if(ctx->ec_1 & 16) print(" INS-FETCH");
	printf(" At Address %0x\n", ctx->ec_2);
	printf("EAX:%0x  EBX:%0x\nECX:%0x  EDX:%0x\n"
		"ESI:%0x  EDI:%0x\nEBP:%0x  ESP:%0x\n",
		ctx->ir->r_eax, ctx->ir->r_ebx,
		ctx->ir->r_ecx, ctx->ir->r_edx,
		ctx->ir->r_esi, ctx->ir->r_edi,
		ctx->ir->r_ebp, ctx->ir->r_esp_hdl);
	printattr(0);
}

void pfinit() {
	ivix_except[0xe] = {pfhandle, nullptr};
}

}

