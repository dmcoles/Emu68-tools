MEMORY
{
    rom (rx) : org = 0x00000000, l = 64K
    ram (rw) : org = 0xdeaddead, l = 0M
}

ENTRY(_rom_start)

SECTIONS
{
    /* Board parameters which will be subsequently used to generate proper expansion rom structures */
    _type   = 0x92;         /* ERT_ZORROIII | ERTF_DIAGVALID | size = 2 (128K) */
    _pID    = 0x21;
    _flags  = 0xd0;         /* Memtype, ZorroIII flag, cannot shutoff */
    _mID    = 0x6d73;
    _serial = 0x04f6403d;
    _off    = _diag_start - _rom_start;
    _int    = 0x00;

    .text 0x0 : 
    {
        _rom_start = .;

        /* ROM image will start with ExpansionRom structure */

        LONG(((_type << 24) | (_type << 12)) & 0xf000f000)      /* er_Type */
        LONG(~((_pID << 24) | (_pID << 12)) & 0xf000f000)       /* er_Product */
        LONG(~((_flags << 24) | (_flags << 12)) & 0xf000f000)   /* er_Flags */
        LONG(0xf000f000)                                        /* er_Reserved03 */

        _mIDh = (_mID >> 8) & 0xff; _mIDl = (_mID) & 0xff;
        LONG(~((_mIDh << 24) | (_mIDh << 12)) & 0xf000f000)     /* er_Manufacturer */
        LONG(~((_mIDl << 24) | (_mIDl << 12)) & 0xf000f000)

        _ser0 = (_serial >> 24) & 0xff; _ser1 = (_serial >> 16) & 0xff;
        _ser2 = (_serial >> 8) & 0xff; _ser3 = (_serial) & 0xff;
        LONG(~((_ser0 << 24) | (_ser0 << 12)) & 0xf000f000)     /* er_SerialNumber */
        LONG(~((_ser1 << 24) | (_ser1 << 12)) & 0xf000f000)
        LONG(~((_ser2 << 24) | (_ser2 << 12)) & 0xf000f000)
        LONG(~((_ser3 << 24) | (_ser3 << 12)) & 0xf000f000)

        _offh = (_off >> 8) & 0xff; _offl = (_off) & 0xff;
        LONG(~((_offh << 24) | (_offh << 12)) & 0xf000f000)     /* er_InitDiagVec */
        LONG(~((_offl << 24) | (_offl << 12)) & 0xf000f000)

        LONG(0xf000f000) LONG(0xf000f000)                       /* er_Reserved0c, er_Reserved0d */
        LONG(0xf000f000) LONG(0xf000f000)                       /* er_Reserved0e, er_Reserved0f */

        /* ExpansionControl follows ExpansionRom structure immediately */

        LONG(((_int << 24) | (_int << 12)) & 0xf000f000)        /* ec_Interrupt */
        LONG(0xf000f000)                                        /* ec_Z3_HighBase */
        LONG(0xf000f000)                                        /* ec_BaseAddress */
        LONG(0xf000f000)                                        /* ec_Shutup */
        
        LONG(0xf000f000) LONG(0xf000f000)                       /* ec_Reserved14, ec_Reserved15 */
        LONG(0xf000f000) LONG(0xf000f000)                       /* ec_Reserved16, ec_Reserved17 */
        LONG(0xf000f000) LONG(0xf000f000)                       /* ec_Reserved18, ec_Reserved19 */
        LONG(0xf000f000) LONG(0xf000f000)                       /* ec_Reserved1a, ec_Reserved1b */
        LONG(0xf000f000) LONG(0xf000f000)                       /* ec_Reserved1c, ec_Reserved1d */
        LONG(0xf000f000) LONG(0xf000f000)                       /* ec_Reserved1e, ec_Reserved1f */

        _diag_start = .;

        BYTE(0x90)                          /* DAC_WORDWIDE | DAC_CONFIGTIME */
        BYTE(0x00)                          /* Flags - no flags */
        SHORT(_ramcopy_end - _diag_start)   /* da_Size */
        SHORT(_DiagPoint - _diag_start)     /* DiagPoint offset */
        SHORT(_BootPoint - _diag_start)     /* BootPoint offset */
        SHORT(_deviceName - _diag_start)    /* Offset to device name */
        LONG(0x00000000)                    /* Reserved, must be 0 */

        _code_start = .;

        /* Everything from main.c will be copied to ram together with DiagArea */
        *main.c.o(.text .text.* .rodata .rodata.*)

        _ramcopy_end = .;

        /* This constant gives offset of DiagArea from beginning of the ROM */
        _diag_offset = _diag_start - _rom_start;

        *(.text .text.* .rodata .rodata.*)
        *(.eh_frame)

        . = ALIGN(4096);
    } >rom = 0xff

    _rom_end = .;

    .data : {
        *(.data .data.*)
    } >ram

    .bss : {
        *(.bss)
    } >ram
}