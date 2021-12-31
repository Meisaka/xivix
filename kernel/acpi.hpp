/* ***
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */
#ifndef ACPI_HAI
#define ACPI_HAI
#include "memory.hpp"

namespace acpi {

struct ACPITableHeader {
	union {
		char sig[4];
		uint32_t sig_v;
	};
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	char asl_compiler_id[4];
	uint32_t asl_compiler_revision;
} __attribute__((packed));

void load_acpi();
void debug_acpi(const char *, int);
}

#endif

