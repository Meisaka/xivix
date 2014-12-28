ENTRY(_start)
SECTIONS
{
	. = 0x500 ;
	.bss (NOLOAD) : 
	{
		*(.bss)
	}
	. = 0x7c00 ;
	.text :
	{
		*(.text)
	}
}
