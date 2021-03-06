ENTRY(__entry)

base_address = 0x80000000;
start_address = 0x8000;
framebuffer_size = 0x400000;

PHDRS
{
  init   PT_LOAD FLAGS(5); /* read-only, executable*/
  text   PT_LOAD FLAGS(5); /* read-only, executable*/
}

SECTIONS
{
	/*bootloader will be compiled with phy address base at 0x8000*/
	.init start_address : AT(start_address) ALIGN(4096)
  {
    KEEP(*(.init .init.*))
		../../basic/arch/sys/arm/v6/start.o(.text .text.*)
		../../basic/arch/sys/arm/v6/start.o(.rodata .rodata.*)
		../../basic/arch/sys/arm/v6/start.o(.data .data.*)
		../../basic/arch/sys/arm/v6/start.o(.bss .bss.*)
  } : init
  _init_end = ALIGN(4096);

	/*kernel will be compiled with virtual address base at 2GB*/
	PROVIDE(_kernel_start = base_address + _init_end);

	.text base_address + _init_end : AT(_init_end) ALIGN(4096)
	{
		KEEP(*(.text))
		*(.text)
	} : text
	. = ALIGN(4096);

	.data : { /** data segment */
		*(.data*)
	}
	. = ALIGN(4096);

	_bss_start = .;
	.bss : 
	{ 
		*(.bss*)
	}
	. = ALIGN(4096);
	_bss_end = .;

	. = . + 0x10000;
	_irq_stack = .;

	. = . + 0x10000;
	_abt_stack = .;

	. = . + 0x10000;
	_svc_stack = .;

	. = ALIGN(4096);
	PROVIDE(_framebuffer_base_raw = .); 
	. += framebuffer_size;
	PROVIDE(_framebuffer_end_raw = .); 

	. = ALIGN(4096);
	PROVIDE(_kernel_end = .);
}
