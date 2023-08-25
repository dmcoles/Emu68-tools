#include <exec/devices.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/devicetree.h>
#include <proto/dos.h>

#include "wifipi.h"
#include "sdio.h"
#include "mbox.h"

#define D(x) x

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
            file = Open("S:brcmfmac43455-sdio.bin", MODE_OLDFILE);
            Seek(file, 0, OFFSET_END);
            size = Seek(file, 0, OFFSET_BEGINING);

            D(bug("[WiFi] Firmware file size: %ld bytes\n", size));

            WiFiBase->w_FirmwareBase = AllocMem(size, MEMF_ANY);
            if (Read(file, WiFiBase->w_FirmwareBase, size) != size)
            {
                D(bug("[WiFi] Something went wrong when reading WiFi firmware\n"));
            }
            Close(file);

            WiFiBase->w_FirmwareSize = (ULONG)size;



            file = Open("S:brcmfmac43455-sdio.txt", MODE_OLDFILE);
            Seek(file, 0, OFFSET_END);
            size = Seek(file, 0, OFFSET_BEGINING);

            D(bug("[WiFi] Config file size: %ld bytes\n", size));

            ULONG src_pos = 0;
            ULONG dst_pos = 0;
            src_conf = AllocMem(size, MEMF_ANY);
            dst_conf = AllocMem(size, MEMF_ANY);
            if (Read(file, src_conf, size) != size)
            {
                D(bug("[WiFi] Something went wrong when reading WiFi config7\n"));
            }
            Close(file);

            ULONG parsed_size = 0;
            D(bug("[WiFi] Removing comments and whitespace from config file\n"));

            do
            {
                // Remove whitespace and newlines at beginning of the line
                while(src_pos < (ULONG)size && (src_conf[src_pos] == ' ' || src_conf[src_pos] == '\t' || src_conf[src_pos] == '\n'))
                {
                    src_pos++;
                    continue;
                }
                
                // If line begins with '#' then it is a comment, remove until end of line
                if (src_conf[src_pos] == '#')
                {
                    while(src_conf[src_pos] != 10 && src_pos < (ULONG)size) {
                        src_pos++;
                    }

                    // Skip new line
                    src_pos++;
                    continue;
                }
                
                // Now there is a token, copy it until newline character
                while(src_pos < (ULONG)size && src_conf[src_pos] != '\n')
                    dst_conf[dst_pos++] = src_conf[src_pos++];
                
                // Skip new line
                src_pos++;
                
                // Go back to remove trailing whitespace
                while(dst_conf[--dst_pos] == ' ');

                dst_pos++;

                // Apply 0 at the end of the entry
                dst_conf[dst_pos++] = 0;

            } while(src_pos < (ULONG)size);

            // and the end apply the end of config marker
            dst_conf[dst_pos++] = 0x00;
            dst_conf[dst_pos++] = 0x00;
            dst_conf[dst_pos++] = 0x00;
            dst_conf[dst_pos++] = 0x00;
            dst_conf[dst_pos++] = 0xaa;
            dst_conf[dst_pos++] = 0x00;
            dst_conf[dst_pos++] = 0x55;
            dst_conf[dst_pos++] = 0xff;

            D(bug("[WiFi] Stripped config length: %ld bytes\n", dst_pos));

            WiFiBase->w_ConfigBase = AllocMem(dst_pos, MEMF_ANY);
            WiFiBase->w_ConfigSize = dst_pos;
            CopyMem(dst_conf, WiFiBase->w_ConfigBase, dst_pos);

            FreeMem(src_conf, size);
            FreeMem(dst_conf, size);
        }
        else
            D(bug("[WiFi] I'm a task\n"));

        D(bug("[WiFi] Setting GPIO41 to 1\n"));
        wr32(WiFiBase->w_GPIO, 0x20, 1 << (41 - 32));

        sdio_init(WiFiBase);
    }

    D(bug("[WiFi] WiFi_Init done\n"));

    return WiFiBase;
}
