/* ***
 * memory.cpp - implementation of memory management functions
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "memory.hpp"
#include "ktext.hpp"
#include "kio.hpp"

extern "C" {
extern char _kernel_start;
extern char _kernel_end;
extern char _kernel_load;
}

size_t const phyptr = ((&_kernel_start) - (&_kernel_load));

namespace acpi {

struct RSDPDescriptor1 {
	char sig[8];
	uint8_t checksum;
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_phyaddr;
} __attribute__((packed));

struct RSDPDescriptor12 {
	RSDPDescriptor1 v1;
	uint32_t length;
	uint64_t xsdt_phyaddr;
	uint8_t checksum;
	uint8_t resv[3];
} __attribute__((packed));

struct ACPITableHeader {
	char sig[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	char asl_compiler_id[4];
	uint32_t asl_compiler_revision;
} __attribute__((packed));

struct MADT {
	ACPITableHeader header;
	uint32_t local_apic_addr;
	uint32_t flags;
} __attribute__((packed));

struct IOAPIC {
	volatile uint32_t *ioapic_base;
	void init(uint64_t base_phy) {
		ioapic_base = (uint32_t*)mem::vmm_request(0x1000, nullptr, base_phy, mem::RQ_RW | mem::RQ_RCD | mem::RQ_WCD);
	}
} io_apic;

struct LocalAPIC {
	volatile uint32_t *apic_base;
	void init(uint64_t base_phy) {
		apic_base = (uint32_t*)mem::vmm_request(0x1000, nullptr, base_phy, mem::RQ_RW | mem::RQ_RCD | mem::RQ_WCD);
		uint32_t version = apic_base[0x30 / 4];
		uint32_t count = apic_base[0x390 / 4];
		xiv::printf("LAPIC:version: %x, timer:%x\n", version, count);
	}
} local_apic;

struct LocalPageBlock {
	struct PageMap {
		const uint8_t *v;
		uint64_t p;
	} pages[8];
	void reset() {
		for(int e = 0; e < 8; e++) this->pages[e] = {nullptr, 0};
	}
	const uint8_t * get_mapping(uint64_t phy) {
		uint64_t phy_page = phy & ~0xfff;
		if(phy_page == 0) return nullptr;
		int freee = -1;
		for(int e = 0; e < 8; e++) {
			if(this->pages[e].p == 0) {
				if(freee == -1) freee = e;
			} else if(this->pages[e].p == phy_page) {
				return this->pages[e].v + (phy & 0xfff);
			}
		}
		if(freee == -1) return nullptr; // no more room
		const uint8_t *v_page = (const uint8_t*)mem::vmm_request(0x1000, nullptr, phy_page, 0);
		if(v_page == nullptr) return nullptr;
		this->pages[freee].p = phy_page;
		this->pages[freee].v = v_page;
		return v_page + (phy & 0xfff);
	}
};

uint8_t acpi_checksum(const void *where, size_t length) {
	const uint8_t *ckb = (const uint8_t *)where;
	uint32_t check = 0;
	for(size_t i = 0; i < length; i++) {
		check += ckb[i];
	}
	return check & 0xff; // invalid if nonzero
}

void load_madt(const MADT *madt_base) {
	using namespace xiv;
	if(acpi_checksum(madt_base, madt_base->header.length)) {
		printf("ACPI-MADT: bad checksum\n");
		return;
	}
	uint64_t local_apic_phy = madt_base->local_apic_addr;
	uint64_t ioapic_phy = 0;
	printf("ACPI-MADT: APIC address %lx\n", local_apic_phy);
	const uint8_t *entries_base = ((const uint8_t*)madt_base) + sizeof(MADT);
	uint32_t remain = madt_base->header.length - sizeof(MADT);
	const uint8_t *e_ptr = entries_base;
	const uint8_t *entries_end = entries_base + remain;
	while(e_ptr < entries_end) {
		uint8_t entry_type = e_ptr[0];
		uint8_t entry_len = e_ptr[1];
		//printf("ACPI-MADT: entry: %x len:%x\n", entry_type, entry_len);
		uint32_t lapic_flags;
		switch(entry_type) {
		case 0: // processor local APIC (one per core/CPU)
			lapic_flags = *(const uint32_t*)(e_ptr + 4);
			printf("ACPI-MADT:PLAPIC: CPUID: %x APICID:%x flags:%x\n",
				e_ptr[2], e_ptr[3], lapic_flags);
			break;
		case 1: // I/O APIC
		{
			uint32_t address = *(const uint32_t*)(e_ptr + 4);
			uint32_t gsint_base = *(const uint32_t*)(e_ptr + 8);
			ioapic_phy = address;
			printf("ACPI-MADT:IOAPIC ID:%x addr:%x INTbase:%x\n",
				e_ptr[2], address, gsint_base);
			break;
		}
		case 2: // IO/APIC Interupt Source Override
			printf("ACPI-MADT:IOAPIC INT source override\n");
			break;
		case 3: // IO/APIC Non-maskable interrupt source
			printf("ACPI-MADT:IOAPIC NMI source\n");
			break;
		case 4: // Local APIC non-maskable interrupts
			printf("ACPI-MADT:LAPIC NMI\n");
			break;
		case 5: // Local APIC address override
			local_apic_phy = *(const uint64_t*)(e_ptr + 4);
			printf("ACPI-MADT:LAPIC addr override %lx\n", local_apic_phy);
			break;
		case 9: // processor local x2APIC
			printf("ACPI-MADT:x2APIC \n");
			break;
		}
		e_ptr += entry_len;
	}
	if(ioapic_phy != 0) {
		io_apic.init(ioapic_phy);
	}
	if(local_apic_phy != 0) {
		local_apic.init(local_apic_phy);
	}
}

const RSDPDescriptor12 *rsdpd = nullptr;

void rsdp_validate(const uint32_t * where) {
	using namespace acpi;
	const RSDPDescriptor12 *d = (const RSDPDescriptor12 *)where;
	if(acpi_checksum(where, 20)) return;
	if(d->v1.revision == 0) { // version 1.0!
		if(!rsdpd) {
			rsdpd = d; // idk, maybe accept it
		}
	} else if(d->v1.revision == 2) {
		if(acpi_checksum(where + (20/4), 16)) return; // invalid v2
		rsdpd = d; // always accept
	}
}
acpi::LocalPageBlock apci_pages;
void load_acpi() {
	using namespace xiv;
	using namespace acpi;
	union {
		char entry_sig[5];
		uint32_t entry_sig_v;
	};
	entry_sig[4] = 0;
	apci_pages.reset();
	print("loading tables...\n");
	const uint8_t *sysbase = (const uint8_t*)phyptr;
	// magic segment pointer should be at 0x40e, convert it and start there
	const uint32_t *ebda = (const uint32_t*)(sysbase + ((*(const uint16_t*)(sysbase + 0x40e)) << 4));
	printf("ACPI: seg:%x\n", ebda);
	const uint32_t rsd_ptr_[] = { 0x20445352, 0x20525450 }; // text to look for
	for(size_t i = 0; i < 64; i++) { // scan first 1KB of that
		if(ebda[0] == rsd_ptr_[0] && ebda[1] == rsd_ptr_[1]) {
			printf("ACPI: hinted RSDP: %x\n", ebda);
			rsdp_validate(ebda);
		}
		ebda += 4; // skip ahead every 16 bytes (the enforced alignment)
	}
	ebda = (const uint32_t*)(sysbase + 0xe0000);
	for(size_t i = 0; i < 0x2000; i++) { // next scan 0xE0000 - 0xFFFFF
		if(ebda[0] == rsd_ptr_[0] && ebda[1] == rsd_ptr_[1]) {
			printf("ACPI: possible RSDP: %x\n", ebda);
			rsdp_validate(ebda);
		}
		ebda += 4; // again, every 16 bytes
	}
	if(rsdpd == nullptr) {
		print("ACPI: no valid tables found\n");
		return;
	}
	if(!rsdpd->v1.revision) {
		print("ACPI: version 1 tables found, but currently unsupported!\n");
		return;
		//uint64_t phy = rsdpd->v1.rsdt_phyaddr;
	}
	uint64_t xsdt_phy = rsdpd->xsdt_phyaddr;
	uint64_t xsdt_phy_page = xsdt_phy & ~0xfff;
	printf("ACPI: version %x table at v_%08x -> p_%lx\n",
			rsdpd->v1.revision, rsdpd, xsdt_phy);
	const uint8_t *xsdt_base = apci_pages.get_mapping(xsdt_phy);
	const ACPITableHeader *xsdt = (const ACPITableHeader*)xsdt_base;
	if(!xsdt_base) { printf("ACPI: failed to map page\n"); return; }
	entry_sig_v = *(const uint32_t*)(xsdt->sig);
	if(xsdt->sig[0] != 'X'
		|| xsdt->sig[1] != 'S'
		|| xsdt->sig[2] != 'D'
		|| xsdt->sig[3] != 'T') {
		printf("ACPI: table XSDT at %x invalid sig: %s\n", xsdt_base, entry_sig);
		return;
	}
	printf("ACPI: table XSDT: len:%x\n", xsdt->length);
	if(acpi_checksum(xsdt_base, xsdt->length)) {
		printf("ACPI: table XSDT at %x invalid checksum\n", xsdt_base);
		return;
	}
	uint32_t entries = (xsdt->length - sizeof(ACPITableHeader)) / 8;
	const uint64_t *xsdt_ptrs = (const uint64_t*)(xsdt_base + sizeof(ACPITableHeader));
	for(uint32_t entry = 0; entry < entries; entry++) {
		const uint8_t *entry_base = apci_pages.get_mapping(xsdt_ptrs[entry]);
		const ACPITableHeader *header = (const ACPITableHeader*)entry_base;
		if(header != nullptr) {
			entry_sig[0] = header->sig[0];
			entry_sig[1] = header->sig[1];
			entry_sig[2] = header->sig[2];
			entry_sig[3] = header->sig[3];
			printf("ACPI-table: p_%lx > v_%x: %s (%x) len: %x\n", xsdt_ptrs[entry], header, entry_sig, entry_sig_v, header->length);
			if(entry_sig_v == 0x43495041) { // APIC
				load_madt((const MADT*)header);
			}
		} else {
			printf("ACPI-XSDT: p_%lx\n", xsdt_ptrs[entry]);
		}
	}
}

} // namespace acpi

