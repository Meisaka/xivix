
ENTRY(_ix_entry)
SECTIONS {
	. = 1M;
	.text : {
		*(.ixboot)
		*(.text)
	}
	.rodata : ALIGN(4K) {
		*(.rodata)
	}
	.data : ALIGN(4K) {
		*(.data)
	}
	.bss (NOLOAD) : ALIGN(4K) {
		*(COMMON)
		*(.bss)
		*(.initstack)
	}
}

