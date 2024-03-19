#include <exec/devices.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/devicetree.h>
#include <proto/dos.h>

#include "wifipi.h"
#include "sdio.h"
#include "mbox.h"
#include "brcm.h"

#define D(x) x

struct FirmwareDesc {
    UWORD chipID;
    UWORD chipREVMask;
    CONST_STRPTR binFile;
    CONST_STRPTR clmFile;
    CONST_STRPTR txtFile;
};

struct ModelDesc {
    CONST_STRPTR modelID;
    const struct FirmwareDesc *firmwareTable;
};

const struct FirmwareDesc zero2Desc[] = {
    /* 43430   */ { 0x0000, 0, "brcmfmac43436s-sdio.bin", NULL, "brcmfmac43436s-sdio.txt" },
    /* 43430b0 */ { 0x0000, 0, "brcmfmac43436-sdio.bin", "brcmfmac43436-sdio.clm_blob", "brcmfmac43436-sdio.txt" },
    /* 43436   */ { 0x0000, 0, "brcmfmac43436-sdio.bin", "brcmfmac43436-sdio.clm_blob", "brcmfmac43436-sdio.txt" },
    /* 43436s  */ { 0x0000, 0, "brcmfmac43436s-sdio.bin", NULL, "brcmfmac43436s-sdio.txt" },
                  { 0x0000, 0, NULL, NULL, NULL }
};

const struct FirmwareDesc model3bDesc[] = {
    /* 43430   */ {  43430, 0x0002, "cyfmac43430-sdio.bin", "cyfmac43430-sdio.clm_blob", "brcmfmac43430-sdio.txt" },
                  { 0x0000, 0, NULL, NULL, NULL }
};

const struct FirmwareDesc model3aplusDesc[] = {
    /* 43455   */ { 0x4345, 0x0040, "cyfmac43455-sdio.bin", "cyfmac43455-sdio.clm_blob", "brcmfmac43455-sdio.txt" },
                  { 0x0000, 0, NULL, NULL, NULL }
};

const struct FirmwareDesc model3bplusDesc[] = {
    /* 43455   */ { 0x4345, 0x0040, "cyfmac43455-sdio.bin", "cyfmac43455-sdio.clm_blob", "brcmfmac43455-sdio.txt" },
                  { 0x0000, 0, NULL, NULL, NULL }
};

const struct FirmwareDesc model4bDesc[] = {
    /* 43455   */ { 0x4345, 0x0040, "cyfmac43455-sdio.bin", "cyfmac43455-sdio.clm_blob", "brcmfmac43455-sdio.txt" },
                  { 0x0000, 0, NULL, NULL, NULL }
};

const struct FirmwareDesc modelCM4Desc[] = {
    /* 43455   */ { 0x4345, 0x0040, "cyfmac43455-sdio.bin", "cyfmac43455-sdio.clm_blob", "brcmfmac43455-sdio.txt" },
    /* 43456   */ { 0x4345, 0xffb0, "brcmfmac43456-sdio.bin", "brcmfmac43456-sdio.clm_blob", "brcmfmac43456-sdio.txt" },
                  { 0x00000000, NULL, NULL, NULL }
};

const struct ModelDesc FirmwareMatrix[] = 
{
    { "raspberrypi,3-model-zero-2-w", zero2Desc },
    { "raspberrypi,3-model-b", model3bDesc },
    { "raspberrypi,3-model-a-plus", model3aplusDesc },
    { "raspberrypi,3-model-b-plus", model3bplusDesc },
    { "raspberrypi,4-model-b", model4bDesc },
    { "raspberrypi,4-compute-module", modelCM4Desc },
    { NULL, NULL }
};

void _bzero(APTR ptr, ULONG sz)
{
    char *p = ptr;
    if (p)
        while(sz--)
            *p++ = 0;
}

APTR _memcpy(APTR dst, CONST_APTR src, ULONG sz)
{
    UBYTE *d = dst;
    const UBYTE *s = src;

    while(sz--) *d++ = *s++;

    return dst;
}

static inline ULONG _strlen(CONST_STRPTR c)
{
    ULONG result = 0;
    while (*c++)
        result++;

    return result;
}

STRPTR _strncpy(STRPTR dst, CONST_STRPTR src, ULONG len)
{
    ULONG slen = _strlen(src);
    if (slen > len)
        slen = len;
    _bzero(dst, len);
    _memcpy(dst, src, slen);
    return dst;
}

STRPTR _strcpy(STRPTR dst, CONST_STRPTR src)
{
    _memcpy(dst, src, _strlen(src) + 1);
    return dst;
}

int _strcmp(CONST_STRPTR s1, CONST_STRPTR s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

int _strncmp(CONST_STRPTR s1, CONST_STRPTR s2, ULONG n)
{
    if (n == 0) {
        return 0;
    }
	while (*s1 == *s2++) {
        if (--n == 0)
            return 0;
		if (*s1++ == '\0')
			return 0;
    }
	return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

BOOL LoadFirmware(struct Chip *chip)
{
    struct WiFiBase *WiFiBase = chip->c_WiFiBase;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    APTR DeviceTreeBase = WiFiBase->w_DeviceTreeBase;
    struct Library *DOSBase = WiFiBase->w_DosBase;
    APTR buffer = NULL;

    /* Firmware name shall never exceed total size of 256 bytes */
    STRPTR path = AllocVec(256, MEMF_CLEAR);
    
    D(bug("[WiFi] Trying to match firmware files for chip ID %04lx rev %lx\n", chip->c_ChipID, chip->c_ChipREV));

    /* Proceed if memory allocated */
    if (path != NULL)
    {
        const struct ModelDesc *matrix = FirmwareMatrix;
        CONST_STRPTR model = DT_GetPropValue(DT_FindProperty(DT_OpenKey("/"), "compatible"));

        /* Go through table of Pi models and find the matching one */
        while (matrix->modelID != NULL)
        {
            /* Check if "compatible" property of root node matches the model */
            if (_strcmp(matrix->modelID, model) == 0)
            {
                /* Yes, break the loop */
                D(bug("[WiFi] Raspberry model match: %s\n", (ULONG)matrix->modelID));
                break;
            }
            else
            {
                /* No, go for next model */
                matrix++;
            }
        }

        if (matrix->firmwareTable != NULL)
        {
            const struct FirmwareDesc *fw = matrix->firmwareTable;

            while(fw->binFile != NULL)
            {
                if (fw->chipID == chip->c_ChipID && (fw->chipREVMask & (1 << chip->c_ChipREV)) != 0)
                {
                    /* We have match. Begin with .bin file as this is the largest one */
                    BPTR file; 
                    LONG size;
                    UBYTE *buffer;
                    ULONG allocSize;
                    ULONG dst_pos, src_pos;

                    D(bug("[WiFi] ChipID match\n"));

                    /* Reset path */
                    AddPart(path, "DEVS:Firmware", 255);
                    /* Add bin file to the path */
                    AddPart(path, fw->binFile, 255);

                    file = Open(path, MODE_OLDFILE);
                    if (file == 0)
                    {
                        D(bug("[WiFi] Error opening firmware BIN file\n"));
                        return FALSE;
                    }
                    Seek(file, 0, OFFSET_END);
                    size = Seek(file, 0, OFFSET_BEGINING);

                    D(bug("[WiFi] Firmware %s file size: %ld bytes\n", (ULONG)fw->binFile, size));
                    allocSize = size;
                    buffer = AllocMem(allocSize, MEMF_ANY);
                    if (buffer == NULL)
                    {
                        Close(file);
                        D(bug("[WiFi] Error allocating memory\n"));
                        return FALSE;
                    }
                    if (Read(file, buffer, size) != size)
                    {
                        D(bug("[WiFi] Something went wrong when reading WiFi firmware\n"));
                        Close(file);
                        FreeMem(buffer, size);
                        return FALSE;
                    }
                    Close(file);

                    /* Upload firmware to the WiFi module */

                    chip->c_FirmwareBase = AllocMem(size, MEMF_PUBLIC);
                    CopyMem(buffer, chip->c_FirmwareBase, size);
                    chip->c_FirmwareSize = size;

                    /* If clm_blob file exists, load it */
                    if (fw->clmFile != NULL)
                    {
                        /* Reset path */
                        AddPart(path, "DEVS:Firmware", 255);
                        /* Add bin file to the path */
                        AddPart(path, fw->clmFile, 255);

                        file = Open(path, MODE_OLDFILE);
                        if (file == 0)
                        {
                            D(bug("[WiFi] Error opening firmware CLM file\n"));
                            return FALSE;
                        }
                        Seek(file, 0, OFFSET_END);
                        size = Seek(file, 0, OFFSET_BEGINING);

                        D(bug("[WiFi] Firmware %s file size: %ld bytes\n", (ULONG)fw->clmFile, size));
                        if ((ULONG)size > allocSize)
                        {
                            FreeMem(buffer, allocSize);
                            allocSize = size;
                            buffer = AllocMem(size, MEMF_ANY);
                        }
                        
                        if (buffer == NULL)
                        {
                            Close(file);
                            D(bug("[WiFi] Error allocating memory\n"));
                            return FALSE;
                        }
                        if (Read(file, buffer, size) != size)
                        {
                            D(bug("[WiFi] Something went wrong when reading WiFi firmware\n"));
                            Close(file);
                            FreeMem(buffer, size);
                            return FALSE;
                        }
                        Close(file);

                        /* Upload firmware to the WiFi module */

                        chip->c_CLMBase = AllocMem(size, MEMF_PUBLIC);
                        CopyMem(buffer, chip->c_CLMBase, size);
                        chip->c_CLMSize = size;
                    }

                    /* Load NVRAM file */
                    /* Reset path */
                    AddPart(path, "DEVS:Firmware", 255);
                    /* Add bin file to the path */
                    AddPart(path, fw->txtFile, 255);

                    file = Open(path, MODE_OLDFILE);
                    if (file == 0)
                    {
                        D(bug("[WiFi] Error opening firmware TXT file\n"));
                        return FALSE;
                    }
                    Seek(file, 0, OFFSET_END);
                    size = Seek(file, 0, OFFSET_BEGINING);

                    D(bug("[WiFi] Firmware %s file size: %ld bytes\n", (ULONG)fw->txtFile, size));
                    if ((ULONG)size > allocSize)
                    {
                        FreeMem(buffer, allocSize);
                        allocSize = size;
                        buffer = AllocMem(size, MEMF_ANY);
                    }
                    
                    if (buffer == NULL)
                    {
                        Close(file);
                        D(bug("[WiFi] Error allocating memory\n"));
                        return FALSE;
                    }
                    if (Read(file, buffer, size) != size)
                    {
                        D(bug("[WiFi] Something went wrong when reading WiFi firmware\n"));
                        Close(file);
                        FreeMem(buffer, size);
                        return FALSE;
                    }
                    Close(file);

                    /* 
                        Parse and reformat NVRAM file. It can be done in place since the resulting NVRAM
                        will be shorter (by comments) or same size (if no commends or whitespace were used)
                    */
                    src_pos = dst_pos = 0;

                    do
                    {
                        // Remove whitespace and newlines at beginning of the line
                        while(src_pos < (ULONG)size && (buffer[src_pos] == ' ' || buffer[src_pos] == '\t' || buffer[src_pos] == '\n'))
                        {
                            src_pos++;
                            continue;
                        }
                        
                        // If line begins with '#' then it is a comment, remove until end of line
                        if (buffer[src_pos] == '#')
                        {
                            while(buffer[src_pos] != 10 && src_pos < (ULONG)size) {
                                src_pos++;
                            }

                            // Skip new line
                            src_pos++;
                            continue;
                        }
                        
                        // Now there is a token, copy it until newline character
                        while(src_pos < (ULONG)size && buffer[src_pos] != '\n')
                            buffer[dst_pos++] = buffer[src_pos++];
                        
                        // Skip new line
                        src_pos++;
                        
                        // Go back to remove trailing whitespace
                        while(buffer[--dst_pos] == ' ');

                        dst_pos++;

                        // Apply 0 at the end of the entry
                        buffer[dst_pos++] = 0;

                    } while(src_pos < (ULONG)size);

                    // and the end apply the end of config marker
                    buffer[dst_pos++] = 0x00;
                    buffer[dst_pos++] = 0x00;
                    buffer[dst_pos++] = 0x00;
                    buffer[dst_pos++] = 0x00;
                    buffer[dst_pos++] = 0xaa;
                    buffer[dst_pos++] = 0x00;
                    buffer[dst_pos++] = 0x55;
                    buffer[dst_pos++] = 0xff;

                    /* Upload NVRAM to WiFi module */
                    
                    chip->c_ConfigBase = AllocMem(dst_pos, MEMF_PUBLIC);
                    CopyMem(buffer, chip->c_ConfigBase, dst_pos);
                    chip->c_ConfigSize = dst_pos;

                    /* Get rid of temporary buffer */
                    FreeMem(buffer, size);
                    FreeVec(path);
                    
                    return TRUE;
                }
                else
                {
                    fw++;
                }
            }
        }
    }

    return FALSE;
}

/*
    Some properties, like e.g. #size-cells, are not always available in a key, but in that case the properties
    should be searched for in the parent. The process repeats recursively until either root key is found
    or the property is found, whichever occurs first
*/
CONST_APTR GetPropValueRecursive(APTR key, CONST_STRPTR property, APTR DeviceTreeBase)
{
    do {
        /* Find the property first */
        APTR prop = DT_FindProperty(key, property);

        if (prop)
        {
            /* If property is found, get its value and exit */
            return DT_GetPropValue(prop);
        }
        
        /* Property was not found, go to the parent and repeat */
        key = DT_GetParent(key);
    } while (key);

    return NULL;
}

struct WiFiBase * WiFi_Init(struct WiFiBase *base asm("d0"), BPTR seglist asm("a0"), struct ExecBase *SysBase asm("a6"))
{
    APTR DeviceTreeBase;
    struct WiFiBase *WiFiBase = base;

    D(bug("[WiFi] WiFi_Init(%08lx, %08lx, %08lx)\n", (ULONG)base, seglist, (ULONG)SysBase));

    WiFiBase->w_SegList = seglist;
    WiFiBase->w_SysBase = SysBase;
    WiFiBase->w_UtilityBase = OpenLibrary("utility.library", 0);
    WiFiBase->w_Device.dd_Library.lib_Revision = WIFIPI_REVISION;
    
    WiFiBase->w_RequestOrig = AllocMem(512, MEMF_CLEAR);
    WiFiBase->w_Request = (APTR)(((ULONG)WiFiBase->w_RequestOrig + 31) & ~31);

    WiFiBase->w_DeviceTreeBase = DeviceTreeBase = OpenResource("devicetree.resource");

    //NewMinList(&WiFiBase->w_Cores);

    if (DeviceTreeBase)
    {
        APTR key;

        /* Get VC4 physical address of mailbox interface. Subsequently it will be translated to m68k physical address */
        key = DT_OpenKey("/aliases");
        if (key)
        {
            CONST_STRPTR mbox_alias = DT_GetPropValue(DT_FindProperty(key, "mailbox"));

            DT_CloseKey(key);
            
            if (mbox_alias != NULL)
            {
                key = DT_OpenKey(mbox_alias);

                if (key)
                {
                    int size_cells = 1;
                    int address_cells = 1;

                    const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
                    const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                    if (siz != NULL)
                        size_cells = *siz;
                    
                    if (addr != NULL)
                        address_cells = *addr;

                    const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));

                    WiFiBase->w_MailBox = (APTR)reg[address_cells - 1];

                    DT_CloseKey(key);
                }
            }
            DT_CloseKey(key);
        }

        /* Open /aliases and find out the "link" to the emmc */
        key = DT_OpenKey("/aliases");
        if (key)
        {
            CONST_STRPTR mmc_alias = DT_GetPropValue(DT_FindProperty(key, "mmc"));

            DT_CloseKey(key);
               
            if (mmc_alias != NULL)
            {
                /* Open the alias and find out the MMIO VC4 physical base address */
                key = DT_OpenKey(mmc_alias);
                if (key) {
                    int size_cells = 1;
                    int address_cells = 1;

                    const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
                    const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                    if (siz != NULL)
                        size_cells = *siz;
                        
                    if (addr != NULL)
                        address_cells = *addr;

                    const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));
                    WiFiBase->w_SDIO = (APTR)reg[address_cells - 1];
                    DT_CloseKey(key);
                }
            }               
            DT_CloseKey(key);
        }

        /* Open /aliases and find out the "link" to the emmc */
        key = DT_OpenKey("/aliases");
        if (key)
        {
            CONST_STRPTR gpio_alias = DT_GetPropValue(DT_FindProperty(key, "gpio"));

            DT_CloseKey(key);
               
            if (gpio_alias != NULL)
            {
                /* Open the alias and find out the MMIO VC4 physical base address */
                key = DT_OpenKey(gpio_alias);
                if (key) {
                    int size_cells = 1;
                    int address_cells = 1;

                    const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
                    const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                    if (siz != NULL)
                        size_cells = *siz;
                        
                    if (addr != NULL)
                        address_cells = *addr;

                    const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));
                    WiFiBase->w_GPIO = (APTR)reg[address_cells - 1];
                    DT_CloseKey(key);
                }
            }               
            DT_CloseKey(key);
        }

        /* Open /soc key and learn about VC4 to CPU mapping. Use it to adjust the addresses obtained above */
        key = DT_OpenKey("/soc");
        if (key)
        {
            int size_cells = 1;
            int address_cells = 1;
            int cpu_address_cells = 1;

            const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
            const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);
            const ULONG * cpu_addr = DT_GetPropValue(DT_FindProperty(DT_OpenKey("/"), "#address-cells"));

            if (siz != NULL)
                size_cells = *siz;
            
            if (addr != NULL)
                address_cells = *addr;

            if (cpu_addr != NULL)
                cpu_address_cells = *cpu_addr;

            const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "ranges"));

                ULONG phys_vc4 = reg[address_cells - 1];
                ULONG phys_cpu = reg[address_cells + cpu_address_cells - 1];

            WiFiBase->w_MailBox = (APTR)((ULONG)WiFiBase->w_MailBox - phys_vc4 + phys_cpu);
            WiFiBase->w_SDIO = (APTR)((ULONG)WiFiBase->w_SDIO - phys_vc4 + phys_cpu);
            WiFiBase->w_GPIO = (APTR)((ULONG)WiFiBase->w_GPIO - phys_vc4 + phys_cpu);

            D(bug("[WiFi]   Mailbox at %08lx\n", (ULONG)WiFiBase->w_MailBox));
            D(bug("[WiFi]   SDIO regs at %08lx\n", (ULONG)WiFiBase->w_SDIO));
            D(bug("[WiFi]   GPIO regs at %08lx\n", (ULONG)WiFiBase->w_GPIO));

            DT_CloseKey(key);
        }

        D(bug("[WiFi] Configuring GPIO alternate functions\n"));

        ULONG tmp = rd32(WiFiBase->w_GPIO, 0x0c);
        tmp &= 0xfff;       // Leave data for GPIO 30..33 intact
        tmp |= 0x3ffff000;  // GPIO 34..39 are ALT3 now
        wr32(WiFiBase->w_GPIO, 0x0c, tmp);

        D(bug("[WiFi] Enabling pull-ups \n"));

        tmp = rd32(WiFiBase->w_GPIO, 0xec);
        tmp &= 0xffff000f;  // Clear PU/PD setting for GPIO 34..39
        tmp |= 0x00005540;  // 01 in 35..59 == pull-up
        wr32(WiFiBase->w_GPIO, 0xec, tmp);
#if 0
        D(bug("[WiFi] Enable GPCLK2, 32kHz on GPIO43 and output on GPIO41\n"));

        tmp = rd32(WiFiBase->w_GPIO, 0x10);
        tmp &= ~(7 << 9);   // Clear ALT-config for GPIO43
        tmp |= 4 << 9;      // GPIO43 to ALT0 == low speed clock
        tmp &= ~(7 << 3);   // Clear ALT-config for GPIO41
        tmp |= 1 << 3;      // Set GPIO41 as output
        wr32(WiFiBase->w_GPIO, 0x10, tmp);

        D(bug("[WiFi] GP2CTL = %08lx\n", rd32((void*)0xf2101000, 0x80)));
        D(bug("[WiFi] GP2DIV = %08lx\n", rd32((void*)0xf2101000, 0x84)));

        D(bug("[WiFi] Setting GPCLK2 to 32kHz\n"));

        D(bug("[WiFi] Stopping clock...\n"));
        wr32((void*)0xf2101000, 0x80, 0x5a000000 | (rd32((void*)0xf2101000, 0x80) & ~0x10));

        while(rd32((void*)0xf2101000, 0x80) & 0x80);

        D(bug("[WiFi] Clock stopped, GP2CTL = %08lx...\n", rd32((void*)0xf2101000, 0x80)));

        /* Clock source is oscillator, divier for 32kHz */
        wr32((void*)0xf2101000, 0x80, 0x5a000001);
        wr32((void*)0xf2101000, 0x84, 0x5a249000);

        D(bug("[WiFi] Starting clock...\n"));
        wr32((void*)0xf2101000, 0x80, 0x5a000000 | (rd32((void*)0xf2101000, 0x80) | 0x10));

        while(0 == (rd32((void*)0xf2101000, 0x80) & 0x80));

        D(bug("[WiFi] Clock is up...\n"));

        D(bug("[WiFi] GP2CTL = %08lx\n", rd32((void*)0xf2101000, 0x80)));
        D(bug("[WiFi] GP2DIV = %08lx\n", rd32((void*)0xf2101000, 0x84)));
#endif
        D(bug("[WiFi] Enabling EMMC clock\n"));
        ULONG clk = get_clock_state(1, WiFiBase);
        D(bug("[WiFi] Old clock state: %lx\n", clk));
        set_clock_state(1, 1, WiFiBase);
        clk = get_clock_state(1, WiFiBase);
        D(bug("[WiFi] New clock state: %lx\n", clk));
        clk = get_clock_rate(1, WiFiBase);
        D(bug("[WiFi] Clock speed: %ld MHz\n", clk / 1000000));
        WiFiBase->w_SDIOClock = clk;

        if (FindTask(NULL)->tc_Node.ln_Type == NT_PROCESS)
        {
            UBYTE *src_conf;
            UBYTE *dst_conf;
            BPTR file;
            LONG size;
            WiFiBase->w_DosBase = OpenLibrary("dos.library", 0);
            struct Library *DOSBase = (struct Library *)WiFiBase->w_DosBase;

            D(bug("[WiFi] I'm a process, DosBase=%08lx\n", (ULONG)WiFiBase->w_DosBase));
        }
        else
            D(bug("[WiFi] I'm a task\n"));

        //D(bug("[WiFi] Setting GPIO41 to 1\n"));
        //wr32(WiFiBase->w_GPIO, 0x20, 1 << (41 - 32));

        struct SDIO * sdio = sdio_init(WiFiBase);
        if (sdio)
        {
            chip_init(sdio);
        }
    }

    D(bug("[WiFi] WiFi_Init done\n"));

    return WiFiBase;
}
