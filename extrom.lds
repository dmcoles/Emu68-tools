MEMORY
{
    rom (rx) : org = 0x00000000, l = 16M
    ram (rw) : org = 0xdeaddead, l = 0M
}

ENTRY(_rom_start)

SECTIONS
{
    .text 0x00000000 : 
    {
        . = 0x00f00000;

        _rom_start = .;

        /* ROM marker */
        SHORT(0x1114)

        /* Immediately jump to the rom at 0xf80000 address */
        SHORT(0x4ef9)
        LONG(0x00f80002)

        . = 0x00f000d0;

        /* Pull all sections now. If user provided writable then it's his fault */
        
        *(.text .text.* .rodata .rodata.*)
        *(.eh_frame)
       
        . = ALIGN(524288);
    } >rom = 0xff

    .data : {
        *(.data .data.*)
    } >ram

    .bss : {
        *(.bss)
    } >ram
}
