SECTIONS {
    . = 0x30000000;
    
	.text : {
			start.o
			clock.o
			sdram.o
			nand.o
			mmu.o
			* (.text)
	}

	. = ALIGN(4);
	.rodata : {
			* (.rodata)
	}

	. = ALIGN(4);
	.data : {
			* (.data)
	}

	. = ALIGN(4);
    bss_start = .;
    .bss : {
			*(.bss)  *(COMMON) 
	}
    bss_end = .;
}
