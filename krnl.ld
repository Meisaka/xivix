
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
	.ctors : ALIGN(4K) {
		*(.ctors.begin)
		*(.ctors)
		*(.ctors.end)
	}
	.bss (NOLOAD) : ALIGN(4K) {
		*(COMMON)
		*(.bss)
		*(.initstack)
	}
}

