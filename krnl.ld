
ENTRY(_ix_entry)
SECTIONS {
	_kernel_load = 1M ;
	_kernel_start = 0xc0000000 + 1M ;
	. = _kernel_load;
	.text.init : AT( 1M ) {
		*( .ixboot )
	}
	. = _kernel_start + SIZEOF( .text.init ) ;
	.text ALIGN(4K) : AT( ALIGN(1M + SIZEOF(.text.init), 4K) ) {
		*(.text)
	}
	.rodata ALIGN(4K) : {
		*(.rodata)
	}
	.data : ALIGN(4K) {
		*(.data)
	}
	.ctors : ALIGN(4K) {
		_ctor_start = . ;
		*(.ctors.begin)
		*(.ctors)
		*(.ctors.end)
	}
	.bss (NOLOAD) : ALIGN(4K) {
		_bss_start = . ;
		*(COMMON)
		*(.bss)
		*(.initstack)
	}
	_kernel_end = ALIGN(4K) ;
	_kernel_phy_end = ( _kernel_end - _kernel_start ) + _kernel_load ;
}

