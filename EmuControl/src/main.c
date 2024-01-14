#include <exec/types.h>
#include <exec/execbase.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/classes.h>
#include <workbench/startup.h>
#include <graphics/gfxbase.h>
#include <graphics/gfx.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/devicetree.h>
#include <proto/asl.h>
#include <clib/muimaster_protos.h>
#include <clib/alib_protos.h>
#include <utility/tagitem.h>
#include <stdarg.h>
#include <stdint.h>

#include <common/compiler.h>

#include "mbox.h"
#include "presets.h"

int main(int);

/* Startup code including workbench message support */
int _start()
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    struct Process *p = NULL;
    struct WBStartup *wbmsg = NULL;
    int ret = 0;

    p = (struct Process *)SysBase->ThisTask;

    if (p->pr_CLI == 0)
    {
        WaitPort(&p->pr_MsgPort);
        wbmsg = (struct WBStartup *)GetMsg(&p->pr_MsgPort);
    }

    ret = main(wbmsg ? 1 : 0);

    if (wbmsg)
    {
        Forbid();
        ReplyMsg((struct Message *)wbmsg);
    }

    return ret;
}

struct ExecBase *       SysBase;
struct IntuitionBase *  IntuitionBase;
struct GfxBase *        GfxBase;
struct Library *        GadToolsBase;
struct DosLibrary *     DOSBase;
struct Library *        MUIMasterBase;
APTR                    MailBox;

#define APPNAME "EmuControl"

static const char version[] __attribute__((used)) = "$VER: " VERSION_STRING;

Object *app;
Object *MainWindow, *INSNDepth, *InlineRange, *LoopCount, *SoftFlush, *CacheFlush, *FastCache, *SlowCHIP, *SlowDBF, *BlitWait;
Object *MainArea, *MIPS_M68k, *MIPS_ARM, *JITUsage, *Effectiveness, *CacheMiss, *SoftThresh;
Object *JITCount, *EnableDebug, *EnableDisasm, *DebugMin, *DebugMax, *CoreTemp, *CoreVolt, *CCRDepth;
Object *MenuOpen, *MenuSaveAs, *MenuQuit, *MenuDefaults;

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

void InitMailBox()
{
    APTR key;
    APTR DeviceTreeBase = OpenResource("devicetree.resource");

    if (DeviceTreeBase)
    {
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

                    MailBox = (APTR)reg[address_cells - 1];

                    DT_CloseKey(key);
                }
            }
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

            MailBox = (APTR)((ULONG)MailBox - phys_vc4 + phys_cpu);

            DT_CloseKey(key);
        }
    }
}

unsigned long long getARMCount()
{
    union {
        unsigned long long u64;
        ULONG u32[2];
    } u;
    ULONG tmp;

    do {
        asm volatile("movec #0xe6, %0":"=r"(u.u32[0]));
        asm volatile("movec #0xe5, %0":"=r"(u.u32[1]));
        asm volatile("movec #0xe6, %0":"=r"(tmp));
    } while(tmp != u.u32[0]);
    
    return u.u64;
}

unsigned long long getCounter()
{
    union {
        unsigned long long u64;
        ULONG u32[2];
    } u;
    ULONG tmp;

    do {
        asm volatile("movec #0xe2, %0":"=r"(u.u32[0]));
        asm volatile("movec #0xe1, %0":"=r"(u.u32[1]));
        asm volatile("movec #0xe2, %0":"=r"(tmp));
    } while(tmp != u.u32[0]);
    
    return u.u64;
}


static inline unsigned long long getM68kCount()
{
    union {
        unsigned long long u64;
        ULONG u32[2];
    } u;
    ULONG tmp;

    do {
        asm volatile("movec #0xe4, %0":"=r"(u.u32[0]));
        asm volatile("movec #0xe3, %0":"=r"(u.u32[1]));
        asm volatile("movec #0xe4, %0":"=r"(tmp));
    } while(tmp != u.u32[0]);
    
    return u.u64;
}

static inline ULONG getJITSize()
{
    ULONG res;

    asm volatile("movec #0xe7, %0":"=r"(res));
    
    return res;
}

static inline ULONG getJITFree()
{
    ULONG res;

    asm volatile("movec #0xe8, %0":"=r"(res));
    
    return res;
}

static inline ULONG getCMiss()
{
    ULONG res;

    asm volatile("movec #0xec, %0":"=r"(res));
    
    return res;
}

static inline ULONG getCounterSpeed()
{
    ULONG res;

    asm volatile("movec #0xe0, %0":"=r"(res));
    
    return res;
}

static inline ULONG getINSN_DEPTH()
{
    ULONG res;

    asm volatile("movec #0xeb, %0":"=r"(res));

    res = (res >> 24) & 0xff;

    if (res == 0)
        res = 256;

    return res;
}

static inline ULONG getSOFT_THRESH()
{
    ULONG res;

    asm volatile("movec #0xea, %0":"=r"(res));

    return res;
}

static inline ULONG getSLOWDOWN_CHIP()
{
    ULONG res;
    
    asm volatile("movec #0x1e0, %0":"=r"(res));
    if (res & 1) return 1;
    else return 0;
}

static inline ULONG getSLOWDOWN_DBF()
{
    ULONG res;
    
    asm volatile("movec #0x1e0, %0":"=r"(res));
    if (res & 2) return 1;
    else return 0;
}

static inline ULONG getCCR_DEPTH()
{
    ULONG res;
    
    asm volatile("movec #0x1e0, %0":"=r"(res));
    return (res >> 3) & 0x1f;
}

static inline void setCCR_DEPTH(ULONG depth)
{
    ULONG reg;
    if (depth > 31)
        depth = 31;
    
    asm volatile("movec #0x1e0, %0":"=r"(reg));
    reg = (reg & 0xffffff07);
    reg |= (depth & 0x1f) << 3;
    asm volatile("movec %0, #0x1e0"::"r"(reg));
}

static inline ULONG getSLOW_CHIP_SPACING()
{
    ULONG res;
    
    asm volatile("movec #0x1e0, %0":"=r"(res));
    return 1 + ((res >> 8) & 0x07);
}

static inline void setSLOW_CHIP_SPACING(ULONG spacing)
{
    ULONG reg;

    // If spacing is set to 0xff do not set it at all
    if (spacing == 0xff)
        return;
    
    // Spacing is in range 1 to 8
    if (spacing == 0)
        spacing = 1;
    if (spacing > 8)
        spacing = 8;
    
    asm volatile("movec #0x1e0, %0":"=r"(reg));
    reg = (reg & 0xfffff8ff);
    reg |= ((spacing - 1) & 0x07) << 8;
    asm volatile("movec %0, #0x1e0"::"r"(reg));
}


static inline void setSOFT_THRESH(ULONG thresh)
{
    asm volatile("movec %0, #0xea"::"r"(thresh));
}

static inline void setINSN_DEPTH(ULONG depth)
{
    ULONG reg;
    if (depth > 256)
        depth = 256;
    
    asm volatile("movec #0xeb, %0":"=r"(reg));
    reg = (reg & 0x00ffffff);
    reg |= (depth & 0xff) << 24;
    asm volatile("movec %0, #0xeb"::"r"(reg));
}

static inline void setSLOWDOWN_CHIP(ULONG slow)
{
    ULONG reg;
    
    asm volatile("movec #0x1e0, %0":"=r"(reg));
    if (slow) reg |= 1;
    else reg &= 0xfffffffe;
    asm volatile("movec %0, #0x1e0"::"r"(reg));
    asm volatile("cinva ic":::"memory");
}

static inline void setSLOWDOWN_DBF(ULONG slow)
{
    ULONG reg;
    
    asm volatile("movec #0x1e0, %0":"=r"(reg));
    if (slow) reg |= 2;
    else reg &= 0xfffffffd;
    asm volatile("movec %0, #0x1e0"::"r"(reg));
    asm volatile("cinva ic":::"memory");
}

static inline void setBLIT_WAIT(ULONG enable)
{
    ULONG reg;
    
    asm volatile("movec #0x1e0, %0":"=r"(reg));
    if (enable) reg |= 1UL << 11;
    else reg &= ~(1UL << 11);
    asm volatile("movec %0, #0x1e0"::"r"(reg));
    asm volatile("cinva ic":::"memory");
}

static inline void setLOOP_COUNT(ULONG depth)
{
    ULONG reg;
    if (depth > 16)
        depth = 16;
    
    asm volatile("movec #0xeb, %0":"=r"(reg));
    reg = (reg & 0xffffff0f);
    reg |= (depth & 0xf) << 4;
    asm volatile("movec %0, #0xeb"::"r"(reg));
}

static inline void setINLINE_RANGE(ULONG depth)
{
    ULONG reg;
    if (depth > 65535)
        depth = 65535;
    
    asm volatile("movec #0xeb, %0":"=r"(reg));
    reg = (reg & 0xff0000ff);
    reg |= (depth & 0xffff) << 8;
    asm volatile("movec %0, #0xeb"::"r"(reg));
}

static inline void setSOFT_FLUSH(ULONG value)
{
    ULONG reg;
    
    asm volatile("movec #0xeb, %0":"=r"(reg));
    if (value)
        reg |= 1;
    else
        reg &= ~1;
    asm volatile("movec %0, #0xeb"::"r"(reg));
}

static inline void setCACHE_IE(ULONG value)
{
    ULONG reg;
    
    asm volatile("movec CACR, %0":"=r"(reg));
    if (value)
        reg |= 0x8000;
    else
        reg &= ~0x8000;
    asm volatile("movec %0, CACR"::"r"(reg));
}

static inline ULONG getJITCount()
{
    ULONG res;

    asm volatile("movec #0xe9, %0":"=r"(res));

    return res;
}

static inline void setDEBUG_EN(ULONG value)
{
    ULONG reg;
    
    asm volatile("movec #0xed, %0":"=r"(reg));
    reg &= ~3;
    if (value)
        reg |= 1;
    asm volatile("movec %0, #0xed"::"r"(reg));
}

static inline void setDEBUG_DISASM(ULONG value)
{
    ULONG reg;
    
    asm volatile("movec #0xed, %0":"=r"(reg));
    reg &= ~4;
    if (value)
        reg |= 4;
    asm volatile("movec %0, #0xed"::"r"(reg));
}

static inline void setDEBUG_LOW(ULONG value)
{
    asm volatile("movec %0, #0xee"::"r"(value));
}

static inline void setDEBUG_HIGH(ULONG value)
{
    asm volatile("movec %0, #0xef"::"r"(value));
}

typedef void (*putc_func)(void *data, char c);

static int int_strlen(char *buf)
{
    int len = 0;

    if (buf)
        while(*buf++)
            len++;

    return len;
}

static void int_itoa(char *buf, char base, uintptr_t value, char zero_pad, int precision, int size_mod, char big, int alternate_form, int neg, char sign)
{
    int length = 0;

    do {
        char c = value % base;

        if (c >= 10) {
            if (big)
                c += 'A'-10;
            else
                c += 'a'-10;
        }
        else
            c += '0';

        value = value / base;
        buf[length++] = c;
    } while(value != 0);

    if (precision != 0)
    {
        while (length < precision)
            buf[length++] = '0';
    }
    else if (size_mod != 0 && zero_pad)
    {
        int sz_mod = size_mod;
        if (alternate_form)
        {
            if (base == 16) sz_mod -= 2;
            else if (base == 8) sz_mod -= 1;
        }
        if (neg)
            sz_mod -= 1;

        while (length < sz_mod)
            buf[length++] = '0';
    }
    if (alternate_form)
    {
        if (base == 8)
            buf[length++] = '0';
        if (base == 16) {
            buf[length++] = big ? 'X' : 'x';
            buf[length++] = '0';
        }
    }

    if (neg)
        buf[length++] = '-';
    else {
        if (sign == '+')
            buf[length++] = '+';
        else if (sign == ' ')
            buf[length++] = ' ';
    }

    for (int i=0; i < length/2; i++)
    {
        char tmp = buf[i];
        buf[i] = buf[length - i - 1];
        buf[length - i - 1] = tmp;
    }

    buf[length] = 0;
}

void vkprintf_pc(putc_func putc_f, void *putc_data, const char * restrict format, va_list args)
{
    char tmpbuf[32];

    while(*format)
    {
        char c;
        char alternate_form = 0;
        int size_mod = 0;
        int length_mod = 0;
        int precision = 0;
        char zero_pad = 0;
        char *str;
        char sign = 0;
        char leftalign = 0;
        uintptr_t value = 0;
        intptr_t ivalue = 0;

        char big = 0;

        c = *format++;

        if (c != '%')
        {
            putc_f(putc_data, c);
        }
        else
        {
            c = *format++;

            if (c == '#') {
                alternate_form = 1;
                c = *format++;
            }

            if (c == '-') {
                leftalign = 1;
                c = *format++;
            }

            if (c == ' ' || c == '+') {
                sign = c;
                c = *format++;
            }

            if (c == '0') {
                zero_pad = 1;
                c = *format++;
            }

            while(c >= '0' && c <= '9') {
                size_mod = size_mod * 10;
                size_mod = size_mod + c - '0';
                c = *format++;
            }

            if (c == '.') {
                c = *format++;
                while(c >= '0' && c <= '9') {
                    precision = precision * 10;
                    precision = precision + c - '0';
                    c = *format++;
                }
            }

            big = 0;

            if (c == 'h')
            {
                c = *format++;
                if (c == 'h')
                {
                    c = *format++;
                    length_mod = 1;
                }
                else length_mod = 2;
            }
            else if (c == 'l')
            {
                c = *format++;
                if (c == 'l')
                {
                    c = *format++;
                    length_mod = 8;
                }
                else length_mod = 4;
            }
            else if (c == 'j')
            {
                c = *format++;
                length_mod = 9;
            }
            else if (c == 't')
            {
                c = *format++;
                length_mod = 10;
            }
            else if (c == 'z')
            {
                c = *format++;
                length_mod = 11;
            }

            switch (c) {
                case 0:
                    return;

                case '%':
                    putc_f(putc_data, '%');
                    break;

                case 'p':
                    value = va_arg(args, uintptr_t);
                    int_itoa(tmpbuf, 16, value, 1, 2*sizeof(uintptr_t), 2*sizeof(uintptr_t), big, 1, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    while (*str) {
                        putc_f(putc_data, *str++);
                    }
                    break;

                case 'X':
                    big = 1;
                    /* fallthrough */
                case 'x':
                    switch (length_mod) {
                        case 8:
                            value = va_arg(args, uint64_t);
                            break;
                        case 9:
                            value = va_arg(args, uintmax_t);
                            break;
                        case 10:
                            value = va_arg(args, uintptr_t);
                            break;
                        case 11:
                            value = va_arg(args, ULONG);
                            break;
                        default:
                            value = va_arg(args, unsigned int);
                            break;
                    }
                    int_itoa(tmpbuf, 16, value, zero_pad, precision, size_mod, big, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    while(*str) {
                        putc_f(putc_data, *str++);
                    }
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'u':
                    switch (length_mod) {
                        case 8:
                            value = va_arg(args, uint64_t);
                            break;
                        case 9:
                            value = va_arg(args, uintmax_t);
                            break;
                        case 10:
                            value = va_arg(args, uintptr_t);
                            break;
                        case 11:
                            value = va_arg(args, ULONG);
                            break;
                        default:
                            value = va_arg(args, unsigned int);
                            break;
                    }
                    int_itoa(tmpbuf, 10, value, zero_pad, precision, size_mod, 0, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    while(*str) {
                        putc_f(putc_data, *str++);
                    }
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'd':
                case 'i':
                    switch (length_mod) {
                        case 8:
                            ivalue = va_arg(args, int64_t);
                            break;
                        case 9:
                            ivalue = va_arg(args, intmax_t);
                            break;
                        case 10:
                            ivalue = va_arg(args, intptr_t);
                            break;
                        case 11:
                            ivalue = va_arg(args, ULONG);
                            break;
                        default:
                            ivalue = va_arg(args, int);
                            break;
                    }
                    if (ivalue < 0)
                        int_itoa(tmpbuf, 10, -ivalue, zero_pad, precision, size_mod, 0, alternate_form, 1, sign);
                    else
                        int_itoa(tmpbuf, 10, ivalue, zero_pad, precision, size_mod, 0, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    while(*str) {
                        putc_f(putc_data, *str++);
                    }
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'o':
                    switch (length_mod) {
                        case 8:
                            value = va_arg(args, uint64_t);
                            break;
                        case 9:
                            value = va_arg(args, uintmax_t);
                            break;
                        case 10:
                            value = va_arg(args, uintptr_t);
                            break;
                        case 11:
                            value = va_arg(args, ULONG);
                            break;
                        default:
                            value = va_arg(args, uint32_t);
                            break;
                    }
                    int_itoa(tmpbuf, 8, value, zero_pad, precision, size_mod, 0, alternate_form, 0, sign);
                    str = tmpbuf;
                    size_mod -= int_strlen(str);
                    if (!leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    while(*str) {
                        putc_f(putc_data, *str++);
                    }
                    if (leftalign)
                        while(size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    break;

                case 'c':
                    putc_f(putc_data, va_arg(args, int));
                    break;

                case 's':
                    {
                        str = va_arg(args, char *);
                        do {
                            if (*str == 0)
                                break;
                            else
                                putc_f(putc_data, *str);
                            size_mod--;
                        } while(*str++ && --precision);
                        while (size_mod-- > 0)
                            putc_f(putc_data, ' ');
                    }
                    break;

                default:
                    putc_f(putc_data, c);
                    break;
            }
        }
    }
}

void putc_s(void *data, char c)
{
    char **ppchr = data;
    char *pchr = *ppchr;
    *pchr++ = c;
    *pchr = 0;
    *ppchr = pchr;
}

void _sprintf(char *buf, const char * restrict format, ...)
{
    va_list v;
    va_start(v, format);
    vkprintf_pc(putc_s, &buf, format, v);
    va_end(v);
}

ULONG update()
{
    APTR ssp = SuperState();

    static unsigned long long old_cnt = 0;
    static unsigned long long old_arm_cnt = 0;
    static unsigned long long old_m68k_cnt = 0;
    static ULONG old_cmiss;

    unsigned long long arm_cnt = getARMCount();
    unsigned long long m68k_cnt = getM68kCount();
    unsigned long long cnt = getCounter();

    ULONG cmiss = getCMiss() * 1000;
    ULONG cnt_speed = getCounterSpeed() / 1000000;
    ULONG jit_free = getJITFree();
    ULONG jit_total = getJITSize();
    ULONG jit_count = getJITCount();

    if (ssp)
        UserState(ssp);

    if (old_arm_cnt != 0 && old_m68k_cnt != 0) {
        ULONG delta_arm = arm_cnt - old_arm_cnt;
        ULONG delta_m68k = m68k_cnt - old_m68k_cnt;
        ULONG delta_cnt = (cnt - old_cnt);
        ULONG gauge_max;
        ULONG delta_cmiss = cmiss - old_cmiss;

        ULONG mips_arm, mips_m68k, cmiss_ps;

        // Use divu directly. Doing that in C will make gcc pull 32-bit division for some reason
        asm volatile("divu.l %1, %0":"=r"(delta_cnt):"r"(cnt_speed),"0"(delta_cnt));
        asm volatile("divu.l %1, %0":"=r"(mips_arm):"r"(delta_cnt),"0"(delta_arm));
        asm volatile("divu.l %1, %0":"=r"(mips_m68k):"r"(delta_cnt),"0"(delta_m68k));
        delta_cnt /= 1000;
        asm volatile("divu.l %1, %0":"=r"(cmiss_ps):"r"(delta_cnt),"0"(delta_cmiss));

        get(MIPS_ARM, MUIA_Gauge_Max, &gauge_max);
        if (mips_arm > gauge_max)
            set(MIPS_ARM, MUIA_Gauge_Max, mips_arm);
        set(MIPS_ARM, MUIA_Gauge_Current, mips_arm);

        get(MIPS_M68k, MUIA_Gauge_Max, &gauge_max);
        if (mips_m68k > gauge_max)
            set(MIPS_M68k, MUIA_Gauge_Max, mips_m68k);
        set(MIPS_M68k, MUIA_Gauge_Current, mips_m68k);

        ULONG eff = (ULONG)(100.0 * (double)delta_m68k / (double)delta_arm);
        set(Effectiveness, MUIA_Gauge_Current, eff);

        get(CacheMiss, MUIA_Gauge_Max, &gauge_max);
        if (cmiss_ps > gauge_max)
            set(CacheMiss, MUIA_Gauge_Max, cmiss_ps);
        set(CacheMiss, MUIA_Gauge_Current, cmiss_ps);

        get(JITCount, MUIA_Gauge_Max, &gauge_max);
        if (jit_count > gauge_max)
            set(JITCount, MUIA_Gauge_Max, jit_count);
        set(JITCount, MUIA_Gauge_Current, jit_count);
    }

    ULONG jit_used = 100 * (jit_total - jit_free) / jit_total;

    set(JITUsage, MUIA_Gauge_Current, jit_used);

    if (MailBox)
    {
        ULONG temp = get_core_temperature();
        LONG volt = get_core_voltage();
        static char str_temp[10];
        static char str_volt[10];

        if (temp != 0)
        {
            temp = (temp + 50) / 100;

            _sprintf(str_temp, "%ld.%ld", temp / 10, temp % 10);

            set(CoreTemp, MUIA_Text_Contents, (ULONG)str_temp);
        }

        if (volt != 0)
        {
            _sprintf(str_volt, "%ld mV", (volt + 500) / 1000);

            set(CoreVolt, MUIA_Text_Contents, (ULONG)str_volt);
        }
    }

    old_arm_cnt = arm_cnt;
    old_m68k_cnt = m68k_cnt;
    old_cnt = cnt;
    old_cmiss = cmiss;
}

ULONG DoSavePreset()
{
    struct Library *AslBase = OpenLibrary("asl.library", 0);
    extern const char default_dir[];
    if (AslBase != NULL)
    {
        struct FileRequester *fr = AllocAslRequest(ASL_FileRequest, NULL);
        if (fr != NULL)
        {
            BOOL result = AslRequestTags(fr,
                ASLFR_TitleText, (ULONG)"Give preset a name...",
                ASLFR_DoSaveMode, TRUE,
                ASLFR_RejectIcons, TRUE,
                ASLFR_InitialDrawer, (ULONG)default_dir,
                TAG_DONE, 0UL
            );

            if (result)
            {
                struct Preset p;
                char *charptr;
                char *c;
                ULONG tmp;
                
                get(DebugMin, MUIA_String_Contents, (ULONG*)&charptr);
                p.pr_DebugStart = 0;
                c = charptr;
                while (*c) {
                    p.pr_DebugStart <<= 4;
                    if (*c >= '0' && *c <= '9')
                        p.pr_DebugStart |= (*c - '0') & 0xf;
                    else if (*c >= 'A' && *c <= 'F')
                        p.pr_DebugStart |= (*c - 'A' + 10) & 0xf;
                    else if (*c >= 'a' && *c <= 'f')
                        p.pr_DebugStart |= (*c - 'a' + 10) & 0xf;
                    c++;
                }

                get(DebugMax, MUIA_String_Contents, (ULONG*)&charptr);
                p.pr_DebugEnd = 0;
                c = charptr;
                while (*c) {
                    p.pr_DebugEnd <<= 4;
                    if (*c >= '0' && *c <= '9')
                        p.pr_DebugEnd |= (*c - '0') & 0xf;
                    else if (*c >= 'A' && *c <= 'F')
                        p.pr_DebugEnd |= (*c - 'A' + 10) & 0xf;
                    else if (*c >= 'a' && *c <= 'f')
                        p.pr_DebugEnd |= (*c - 'a' + 10) & 0xf;
                    c++;
                }

                p.pr_DebugFlag = 0;
                get(EnableDisasm, MUIA_Selected, &tmp);
                if (tmp) p.pr_DebugFlag |= DBGF_DISASM_ON;
                get(EnableDebug, MUIA_Selected, &tmp);
                if (tmp) p.pr_DebugFlag |= DBGF_DEBUG_ON;

                p.pr_JITFlags = 0;
                get(SlowCHIP, MUIA_Selected, &tmp);
                if (tmp) p.pr_JITFlags |= JITF_SLOW_CHIP;
                get(SlowDBF, MUIA_Selected, &tmp);
                if (tmp) p.pr_JITFlags |= JITF_SLOW_DBF;
                get(BlitWait, MUIA_Selected, &tmp);
                if (tmp) p.pr_JITFlags |= JITF_BLIT_WAIT;
                get(FastCache, MUIA_Selected, &tmp);
                if (tmp) p.pr_JITFlags |= JITF_FAST_CACHE;
                get(SoftFlush, MUIA_Selected, &tmp);
                if (tmp) p.pr_JITFlags |= JITF_SOFT_FLUSH;
                get(CCRDepth, MUIA_Numeric_Value, &tmp);
                p.pr_CCRDepth = tmp;
                get(INSNDepth, MUIA_Numeric_Value, &tmp);
                p.pr_INSNDepth = tmp - 1;
                get(LoopCount, MUIA_Numeric_Value, &tmp);
                p.pr_InlineLoopCnt = tmp;
                get(SoftThresh, MUIA_Numeric_Value, &tmp);
                p.pr_SoftFlushThreshold = tmp;
                get(InlineRange, MUIA_Numeric_Value, &tmp);
                p.pr_InlineRange = (1 << tmp) - 1;
                
                SavePreset(&p, fr->fr_File, fr->fr_Drawer);
            }

            FreeAslRequest(fr);
        }

        CloseLibrary(AslBase);
    }

    return 0;
}

ULONG DoLoadPreset()
{
    struct Library *AslBase = OpenLibrary("asl.library", 0);
    extern const char default_dir[];
    if (AslBase != NULL)
    {
        struct FileRequester *fr = AllocAslRequest(ASL_FileRequest, NULL);
        if (fr != NULL)
        {
            BOOL result = AslRequestTags(fr,
                ASLFR_TitleText, (ULONG)"Open existing preset...",
                ASLFR_DoSaveMode, FALSE,
                ASLFR_RejectIcons, TRUE,
                ASLFR_InitialDrawer, (ULONG)default_dir,
                TAG_DONE, 0UL
            );

            if (result)
            {
                struct Preset p;
                char *charptr;
                char *c;
                ULONG tmp;

                if (LoadPreset(&p, fr->fr_File, fr->fr_Drawer))
                {
                    char tmp_str[32];
                    _sprintf(tmp_str, "%08lx", p.pr_DebugStart);
                    set(DebugMin, MUIA_String_Contents, (ULONG)tmp_str);

                    _sprintf(tmp_str, "%08lx", p.pr_DebugEnd);
                    set(DebugMax, MUIA_String_Contents, (ULONG)tmp_str);

                    if (p.pr_DebugFlag & DBGF_DEBUG_ON)
                        set(EnableDebug, MUIA_Selected, TRUE);
                    else
                        set(EnableDebug, MUIA_Selected, FALSE);

                    if (p.pr_DebugFlag & DBGF_DISASM_ON)
                        set(EnableDisasm, MUIA_Selected, TRUE);
                    else
                        set(EnableDisasm, MUIA_Selected, FALSE);

                    if (p.pr_JITFlags & JITF_FAST_CACHE)
                        set(FastCache, MUIA_Selected, TRUE);
                    else
                        set(FastCache, MUIA_Selected, FALSE);

                    if (p.pr_JITFlags & JITF_FAST_CACHE)
                        set(FastCache, MUIA_Selected, TRUE);
                    else
                        set(FastCache, MUIA_Selected, FALSE);

                    if (p.pr_JITFlags & JITF_SOFT_FLUSH)
                        set(SoftFlush, MUIA_Selected, TRUE);
                    else
                        set(SoftFlush, MUIA_Selected, FALSE);

                    if (p.pr_JITFlags & JITF_SLOW_CHIP)
                        set(SlowCHIP, MUIA_Selected, TRUE);
                    else
                        set(SlowCHIP, MUIA_Selected, FALSE);

                    if (p.pr_JITFlags & JITF_SLOW_DBF)
                        set(SlowDBF, MUIA_Selected, TRUE);
                    else
                        set(SlowDBF, MUIA_Selected, FALSE);

                    if (p.pr_JITFlags & JITF_BLIT_WAIT)
                        set(BlitWait, MUIA_Selected, TRUE);
                    else
                        set(BlitWait, MUIA_Selected, FALSE);

                    set(CCRDepth, MUIA_Numeric_Value, p.pr_CCRDepth);
                    set(INSNDepth, MUIA_Numeric_Value, p.pr_INSNDepth + 1);
                    set(LoopCount, MUIA_Numeric_Value, p.pr_InlineLoopCnt);
                    set(SoftThresh, MUIA_Numeric_Value, p.pr_SoftFlushThreshold);

                    for (int i=0; i <= 16; i++) {
                        if ((1 << i) > p.pr_InlineRange) {
                            set(InlineRange, MUIA_Numeric_Value, i);
                            break;
                        }
                    }
                }
            }

            FreeAslRequest(fr);
        }

        CloseLibrary(AslBase);
    }

    return 0;
}

ULONG SliderDispatcher(REGARG(struct IClass *ic, "a0"), REGARG(Object *o, "a2"), REGARG(Msg message, "a1"))
{
    static char str[16];

    switch(message->MethodID)
    {
        case MUIM_Numeric_Stringify: {
            struct MUIP_Numeric_Stringify *m = (struct MUIP_Numeric_Stringify *)message;
            ULONG val = (1 << m->value) - 1;
            _sprintf(str, "%ld", val);
            return (ULONG)str;
        }
        default: return DoSuperMethodA(ic, o, message);
    }
}

ULONG UpdaterDispatcher(REGARG(struct IClass *ic, "a0"), REGARG(Msg message, "a1"), REGARG(Object *o, "a2"))
{
    ULONG *mId = (ULONG *)message;

    if (*mId == 0xdeadbeef) {
        update();
        return 0;
    }
    else
        return DoSuperMethodA(ic, o, message);
}

ULONG ChangeINSNDepth()
{
    ULONG insnDepth;

    get(INSNDepth, MUIA_Numeric_Value, &insnDepth);

    APTR ssp = SuperState();

    setINSN_DEPTH(insnDepth);

    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG ChangeLOOPCount()
{
    ULONG value;

    get(LoopCount, MUIA_Numeric_Value, &value);

    APTR ssp = SuperState();

    setLOOP_COUNT(value);

    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG ChangeInlineRange()
{
    ULONG value;

    get(InlineRange, MUIA_Numeric_Value, &value);

    value = (1 << value) - 1;

    APTR ssp = SuperState();

    setINLINE_RANGE(value);

    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG ChangeSoftFlush()
{
    ULONG value;

    get(SoftFlush, MUIA_Selected, &value);

    APTR ssp = SuperState();

    setSOFT_FLUSH(value);

    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG ChangeSoftThresh()
{
    ULONG value;

    get(SoftThresh, MUIA_Numeric_Value, &value);

    APTR ssp = SuperState();

    setSOFT_THRESH(value);

    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG ChangeFastCache()
{
    ULONG value;

    get(FastCache, MUIA_Selected, &value);

    APTR ssp = SuperState();

    setCACHE_IE(value);

    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG ChangeSlowCHIP()
{
    ULONG value;

    get(SlowCHIP, MUIA_Selected, &value);

    APTR ssp = SuperState();

    setSLOWDOWN_CHIP(value);
    
    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG ChangeSlowDBF()
{
    ULONG value;

    get(SlowDBF, MUIA_Selected, &value);

    APTR ssp = SuperState();

    setSLOWDOWN_DBF(value);
    
    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG ChangeBlitWait()
{
    ULONG value;

    get(BlitWait, MUIA_Selected, &value);

    APTR ssp = SuperState();

    setBLIT_WAIT(value);
    
    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG ChangeCCRDepth()
{
    ULONG ccrDepth;

    get(CCRDepth, MUIA_Numeric_Value, &ccrDepth);

    APTR ssp = SuperState();

    setCCR_DEPTH(ccrDepth);

    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG DoFlushCache()
{
    ULONG value;

    get(FastCache, MUIA_Selected, &value);

    APTR ssp = SuperState();

    asm volatile("cinva ic":::"memory");

    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG UpdateDebugLo()
{
    ULONG tmp;
    char *c;
    char *debug_low;

    get(DebugMin, MUIA_String_Contents, (ULONG*)&debug_low);

    tmp = 0;
    c = debug_low;
    while (*c) {

        tmp = tmp << 4;

        if (*c >= '0' && *c <= '9')
            tmp |= (*c - '0') & 0xf;
        else if (*c >= 'A' && *c <= 'F')
            tmp |= (*c - 'A' + 10) & 0xf;
        else if (*c >= 'a' && *c <= 'f')
            tmp |= (*c - 'a' + 10) & 0xf;
        
        c++;
    }

    APTR ssp = SuperState();
    setDEBUG_LOW(tmp);
    if (ssp)
        UserState(ssp);
}

ULONG UpdateDebugHi()
{
    ULONG tmp;
    char *c;
    char *debug_high;

    get(DebugMax, MUIA_String_Contents, (ULONG*)&debug_high);

    tmp = 0;
    c = debug_high;
    while (*c) {

        tmp = tmp << 4;

        if (*c >= '0' && *c <= '9')
            tmp |= (*c - '0') & 0xf;
        else if (*c >= 'A' && *c <= 'F')
            tmp |= (*c - 'A' + 10) & 0xf;
        else if (*c >= 'a' && *c <= 'f')
            tmp |= (*c - 'a' + 10) & 0xf;
        
        c++;
    }

    APTR ssp = SuperState();
    setDEBUG_HIGH(tmp);
    if (ssp)
        UserState(ssp);
}

ULONG ChangeDebugEnable()
{
    ULONG value;

    get(EnableDebug, MUIA_Selected, &value);

    APTR ssp = SuperState();

    setDEBUG_EN(value);

    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG ChangeDebugDisasm()
{
    ULONG value;

    get(EnableDisasm, MUIA_Selected, &value);

    APTR ssp = SuperState();

    setDEBUG_DISASM(value);

    if (ssp)
        UserState(ssp);
    
    return 0;
}

ULONG ResetToDefaults()
{
    set(DebugMin, MUIA_String_Contents, (ULONG)"00000000");
    set(DebugMax, MUIA_String_Contents, (ULONG)"ffffffff");
    set(EnableDebug, MUIA_Selected, FALSE);
    set(EnableDisasm, MUIA_Selected, FALSE);
    set(SlowCHIP, MUIA_Selected, FALSE);
    set(SlowDBF, MUIA_Selected, FALSE);
    set(BlitWait, MUIA_Selected, FALSE);
    set(FastCache, MUIA_Selected, TRUE);
    set(SoftFlush, MUIA_Selected, TRUE);
    set(CCRDepth, MUIA_Numeric_Value, 20);
    set(INSNDepth, MUIA_Numeric_Value, 256);
    set(LoopCount, MUIA_Numeric_Value, 8);
    set(SoftThresh, MUIA_Numeric_Value, 500);
    set(InlineRange, MUIA_Numeric_Value, 13);
    
    return 0;
}

struct Hook hook_INSNDepth = {
    .h_Entry = ChangeINSNDepth
};

struct Hook hook_LoopCount = {
    .h_Entry = ChangeLOOPCount
};

struct Hook hook_InlineRange = {
    .h_Entry = ChangeInlineRange
};

struct Hook hook_SoftFlush = {
    .h_Entry = ChangeSoftFlush
};

struct Hook hook_EnableDebug = {
    .h_Entry = ChangeDebugEnable
};

struct Hook hook_UpdateDebugLo = {
    .h_Entry = UpdateDebugLo
};

struct Hook hook_UpdateDebugHi = {
    .h_Entry = UpdateDebugHi
};

struct Hook hook_EnableDisasm = {
    .h_Entry = ChangeDebugDisasm
};

struct Hook hook_SoftThresh = {
    .h_Entry = ChangeSoftThresh
};

struct Hook hook_FastCache = {
    .h_Entry = ChangeFastCache
};

struct Hook hook_FlushCache = {
    .h_Entry = DoFlushCache
};

struct Hook hook_SlowCHIP = {
    .h_Entry = ChangeSlowCHIP
};

struct Hook hook_CCRDepth = {
    .h_Entry = ChangeCCRDepth
};

struct Hook hook_SlowDBF = {
    .h_Entry = ChangeSlowDBF
};

struct Hook hook_BlitWait = {
    .h_Entry = ChangeBlitWait
};

struct Hook hook_ResetToDefaults = {
    .h_Entry = ResetToDefaults
};

struct Hook hook_SavePreset = {
    .h_Entry = DoSavePreset
};

struct Hook hook_LoadPreset = {
    .h_Entry = DoLoadPreset
};

BOOL previewOnly;

void MUIMain()
{
    struct MUI_CustomClass *logSlider = MUI_CreateCustomClass(NULL, MUIC_Slider, NULL, 4, SliderDispatcher);
    struct MUI_CustomClass *updater = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 4, UpdaterDispatcher);

    Object *updaterObj;
    struct MUI_InputHandlerNode ihn;
    
    if (logSlider) {
        app = ApplicationObject, 
                MUIA_Application_Title, (ULONG)APPNAME,
                MUIA_Application_Version, (ULONG)version,
                MUIA_Application_Copyright, (ULONG)"(C) 2022-2023 Michal Schulz",
                MUIA_Application_Author, (ULONG)"Michal Schulz",
                MUIA_Application_Description, (ULONG)APPNAME,
                MUIA_Application_Base, (ULONG)"EMUCONTROL",
                MUIA_Application_SingleTask, TRUE,

                MUIA_Application_Menustrip, MenustripObject,
                    MUIA_Family_Child, MenuObject,
                        MUIA_Menu_Title, (ULONG)"Project",
                        MUIA_Family_Child, MenuOpen = MenuitemObject,
                            MUIA_Menuitem_Title, (ULONG)"Open",
                        End,
                        MUIA_Family_Child, MenuSaveAs = MenuitemObject,
                            MUIA_Menuitem_Title, (ULONG)"Save As",
                        End,
                        MUIA_Family_Child, MenuQuit = MenuitemObject,
                            MUIA_Menuitem_Title, (ULONG)"Quit",
                        End,
                    End,
                    MUIA_Family_Child, MenuObject,
                        MUIA_Menu_Title, (ULONG)"Edit",
                        MUIA_Family_Child, MenuDefaults = MenuitemObject,
                            MUIA_Menuitem_Title, (ULONG)"Reset to Defaults",
                        End,
                    End,
                End,

                SubWindow, MainWindow = WindowObject,
                    MUIA_Window_Title, (ULONG)APPNAME,
                    WindowContents, VGroup,
                        Child, updaterObj = NewObject(updater->mcc_Class, NULL, 
                            MUIA_ShowMe, FALSE,
                        TAG_DONE),
                        Child, MainArea = HGroup,
                            InnerSpacing(2, 2),
                            Child, VGroup,
                                Child, VGroup,
                                    GroupFrameT("JIT Controls"),
                                    Child, ColGroup(2),
                                        Child, Label("JIT instruction depth"),
                                        Child, INSNDepth = SliderObject,
                                            MUIA_Numeric_Min, 1,
                                            MUIA_Numeric_Max, 256,
                                            MUIA_Numeric_Value, 1,
                                            MUIA_ShortHelp, (ULONG)"Maximal number of m68k instructions\ntranslated into single block of ARM code",
                                        End,
                                        Child, Label("JIT inlining range"),
                                        Child, InlineRange = NewObject(logSlider->mcc_Class, NULL,
                                            MUIA_Numeric_Min, 0,
                                            MUIA_Numeric_Max, 16,
                                            MUIA_Numeric_Value, 0,
                                        TAG_DONE),
                                        Child, Label("Inline loop count"),
                                        Child, LoopCount = SliderObject,
                                            MUIA_Numeric_Min, 1,
                                            MUIA_Numeric_Max, 16,
                                            MUIA_Numeric_Value, 1,
                                        End,
                                        Child, Label("CCR scan depth"),
                                        Child, CCRDepth = SliderObject,
                                            MUIA_Numeric_Min, 0,
                                            MUIA_Numeric_Max, 31,
                                            MUIA_Numeric_Value, 20,
                                        End,   
                                        Child, Label("Soft flush threshold"),
                                        Child, SoftThresh = SliderObject,
                                            MUIA_Numeric_Min, 1,
                                            MUIA_Numeric_Max, 4000,
                                            MUIA_Numeric_Value, 1,
                                        End,                                
                                    End,
                                    Child, HGroup,
                                        Child, SoftFlush = MUI_MakeObject(MUIO_Button, "Soft flush"),
                                        Child, FastCache = MUI_MakeObject(MUIO_Button, "Fast cache"),
                                        Child, SlowCHIP = MUI_MakeObject(MUIO_Button, "Slow CHIP"),
                                        Child, SlowDBF = MUI_MakeObject(MUIO_Button, "Slow DBF"),
                                        Child, BlitWait = MUI_MakeObject(MUIO_Button, "Blit wait"),
                                        Child, CacheFlush = MUI_MakeObject(MUIO_Button, "Flush JIT cache"),
                                    End,
                                End,
                            End,
                            Child, VGroup,
                                Child, VGroup,
                                    GroupFrameT("Debug Controls"),
                                    Child, ColGroup(2),
                                        Child, Label("Low debug addres (hex)"),
                                        Child, DebugMin = StringObject,
                                            StringFrame,
                                            MUIA_String_Contents, "00000000",
                                            MUIA_String_MaxLen, 9,
                                            MUIA_String_Accept, "0123456789abcdefABCDEF",
                                        End,
                                        Child, Label("High debug addres (hex)"),
                                        Child, DebugMax = StringObject,
                                            StringFrame,
                                            MUIA_String_Contents, "ffffffff",                                        
                                            MUIA_String_MaxLen, 9,
                                            MUIA_String_Accept, "0123456789abcdefABCDEF",
                                        End,
                                    End,
                                    Child, HGroup,
                                        Child, EnableDebug = MUI_MakeObject(MUIO_Button, "Debug"),
                                        Child, EnableDisasm = MUI_MakeObject(MUIO_Button, "Disassemble"),
                                    End,
                                End,
                                Child, VSpace(-1),
                                Child, VGroup,
                                    GroupFrameT("RasPi Core Status"),
                                    Child, HGroup,
                                        Child, ColGroup(2),
                                            Child, Label("Temperature"),
                                            Child, CoreTemp = TextObject,
                                                TextFrame,
                                                MUIA_Text_Contents, "0.0",
                                            End,
                                        End,
                                        Child, ColGroup(2),
                                            Child, Label("Voltage"),
                                            Child, CoreVolt = TextObject,
                                                TextFrame,
                                                MUIA_Text_Contents, "0",
                                            End,
                                        End,
                                    End,
                                End,
                            End,
                        End,
                        Child, HGroup,
                            InnerSpacing(2, 2),
                            Child, VGroup,
                                GroupFrameT("JIT Statistics"),
                                Child, HGroup,
                                    Child, ColGroup(2),
                                        Child, Label("Cache usage:"),
                                        Child, JITUsage = GaugeObject,
                                            GaugeFrame,
                                            MUIA_Gauge_Max, 100,
                                            MUIA_Gauge_Current, 0,
                                            MUIA_Gauge_Horiz, TRUE,
                                            MUIA_Gauge_InfoText, (LONG)"%ld%% in use",
                                        End,
                                        Child, Label("JIT units:"),
                                        Child, JITCount = GaugeObject,
                                            GaugeFrame,
                                            MUIA_Gauge_Max, 10,
                                            MUIA_Gauge_Current, 0,
                                            MUIA_Gauge_Horiz, TRUE,
                                            MUIA_Gauge_InfoText, (LONG)"%ld units in cache",
                                        End,
                                        Child, Label("Cache misses:"),
                                        Child, CacheMiss = GaugeObject,
                                            GaugeFrame,
                                            MUIA_Gauge_Max, 10,
                                            MUIA_Gauge_Current, 0,
                                            MUIA_Gauge_Horiz, TRUE,
                                            MUIA_Gauge_InfoText, (LONG)"%ld per second",
                                        End,
                                    End,
                                    Child, ColGroup(2),
                                        Child, Label("M68k speed:"),
                                        Child, MIPS_M68k = GaugeObject,
                                            GaugeFrame,
                                            MUIA_Gauge_Max, 10,
                                            MUIA_Gauge_Current, 0,
                                            MUIA_Gauge_Horiz, TRUE,
                                            MUIA_Gauge_InfoText, (LONG)"%ld MIPS",
                                        End,
                                        Child, Label("ARM speed:"),
                                        Child, MIPS_ARM = GaugeObject,
                                            GaugeFrame,
                                            MUIA_Gauge_Max, 10,
                                            MUIA_Gauge_Current, 0,
                                            MUIA_Gauge_Horiz, TRUE,
                                            MUIA_Gauge_InfoText, (LONG)"%ld MIPS",
                                        End,
                                        Child, Label("Effectiveness:"),
                                        Child, Effectiveness = GaugeObject,
                                            GaugeFrame,
                                            MUIA_Gauge_Max, 100,
                                            MUIA_Gauge_Current, 0,
                                            MUIA_Gauge_Horiz, TRUE,
                                            MUIA_Gauge_InfoText, (LONG)"%ld%%",
                                        End,
                                    End,
                                End,
                            End,
                        End,
                    End,
                End,
            End;
        
        if (app)
        {
            ULONG isOpen;
            APTR ssp;
            ULONG tmp, cacr, thresh, debug_low, debug_high, debug_ctrl, ctrl2;

            DoMethod(MainWindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
                (ULONG)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

            DoMethod(app, MUIM_Notify, MUIA_Application_DoubleStart, MUIV_EveryTime,
                (ULONG)app, 3, MUIM_Set, MUIA_Application_Iconified, FALSE);

            DoMethod(app, MUIM_Notify, MUIA_Application_DoubleStart, MUIV_EveryTime,
                (ULONG)MainWindow, 1, MUIM_Window_ToFront);

            DoMethod(MenuQuit, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
                (ULONG)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

            DoMethod(MenuDefaults, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
                (ULONG)app, 2, MUIM_CallHook, (ULONG)&hook_ResetToDefaults);

            DoMethod(MenuOpen, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
                (ULONG)app, 2, MUIM_CallHook, (ULONG)&hook_LoadPreset);

            DoMethod(MenuSaveAs, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
                (ULONG)app, 2, MUIM_CallHook, (ULONG)&hook_SavePreset);

            set(SoftFlush, MUIA_InputMode, MUIV_InputMode_Toggle);
            set(FastCache, MUIA_InputMode, MUIV_InputMode_Toggle);
            set(SlowCHIP, MUIA_InputMode, MUIV_InputMode_Toggle);
            set(SlowDBF, MUIA_InputMode, MUIV_InputMode_Toggle);
            set(BlitWait, MUIA_InputMode, MUIV_InputMode_Toggle);
            set(EnableDebug, MUIA_InputMode, MUIV_InputMode_Toggle);
            set(EnableDisasm, MUIA_InputMode, MUIV_InputMode_Toggle);

            if (!previewOnly)
            {
                ihn.ihn_Flags = MUIIHNF_TIMER;
                ihn.ihn_Millis = 500;
                ihn.ihn_Object = updaterObj;
                ihn.ihn_Method = 0xdeadbeef;

                char tmp_str[32];

                DoMethod(app, MUIM_Application_AddInputHandler, &ihn);

                ssp = SuperState();

                asm volatile("movec #0xeb, %0; movec CACR, %1":"=r"(tmp), "=r"(cacr));
                asm volatile("movec #0xea, %0; movec #0xed, %1":"=r"(thresh), "=r"(debug_ctrl));
                asm volatile("movec #0xee, %0; movec #0xef, %1":"=r"(debug_low), "=r"(debug_high));
                asm volatile("movec #0x1e0, %0":"=r"(ctrl2));

                if (ssp)
                    UserState(ssp);

                set(SoftThresh, MUIA_Numeric_Value, thresh);
                if (debug_ctrl & 3)
                    set(EnableDebug, MUIA_Selected, TRUE);
                if (debug_ctrl & 4)
                    set(EnableDisasm, MUIA_Selected, TRUE);
                if (ctrl2 & 1)
                    set(SlowCHIP, MUIA_Selected, TRUE);
                if (ctrl2 & 2)
                    set(SlowDBF, MUIA_Selected, TRUE);
                if (ctrl2 & (1<<11))
                    set(BlitWait, MUIA_Selected, TRUE);
                
                set(CCRDepth, MUIA_Numeric_Value, (ctrl2 >> 3) & 0x1f);

                if (tmp & 0xff000000)
                    set(INSNDepth, MUIA_Numeric_Value, ((tmp >> 24) & 0xff));
                else
                    set(INSNDepth, MUIA_Numeric_Value, 256);

                _sprintf(tmp_str, "%08lx", debug_low);
                set(DebugMin, MUIA_String_Contents, (ULONG)tmp_str);

                _sprintf(tmp_str, "%08lx", debug_high);
                set(DebugMax, MUIA_String_Contents, (ULONG)tmp_str);

                if (tmp & 0x000000f0)
                    set(LoopCount, MUIA_Numeric_Value, ((tmp >> 4) & 0xf));
                else
                    set(LoopCount, MUIA_Numeric_Value, 16);

                if (tmp & 1)
                    set(SoftFlush, MUIA_Selected, TRUE);
                
                if (cacr & 0x8000)
                    set(FastCache, MUIA_Selected, TRUE);

                DoMethod(INSNDepth, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_INSNDepth);
                DoMethod(LoopCount, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_LoopCount);
                DoMethod(InlineRange, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_InlineRange);
                DoMethod(SoftThresh, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_SoftThresh);
                DoMethod(SoftFlush, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_SoftFlush);
                DoMethod(FastCache, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_FastCache);
                DoMethod(EnableDebug, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_EnableDebug);
                DoMethod(EnableDisasm, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_EnableDisasm);
                DoMethod(CacheFlush, MUIM_Notify, MUIA_Pressed, FALSE,
                    (ULONG)app, 2, MUIM_CallHook, &hook_FlushCache);
                DoMethod(SlowCHIP, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_SlowCHIP);
                DoMethod(SlowDBF, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_SlowDBF);
                DoMethod(BlitWait, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_BlitWait);
                DoMethod(CCRDepth, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_CCRDepth);

                DoMethod(DebugMin, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_UpdateDebugLo);
                DoMethod(DebugMin, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
                    (ULONG)MainWindow, 3, MUIM_Set, MUIA_Window_ActiveObject, DebugMax);
                DoMethod(DebugMax, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
                    (ULONG)app, 2, MUIM_CallHook, &hook_UpdateDebugHi);

                tmp = ((tmp >> 8) & 0xffff);
                
                for (int i=0; i <= 16; i++) {
                    if ((1 << i) > tmp) {
                        set(InlineRange, MUIA_Numeric_Value, i);
                        break;
                    }
                }
            }

            set(MainWindow, MUIA_Window_Open, TRUE);
            get(MainWindow, MUIA_Window_Open, &isOpen);
            if (isOpen) {
                ULONG signals = 0L;

                while(DoMethod(app, MUIM_Application_NewInput, &signals) != MUIV_Application_ReturnID_Quit)
                {
                    if(signals != 0)
                    {
                        signals = Wait(signals | SIGBREAKF_CTRL_C);
                        if(signals & SIGBREAKF_CTRL_C)
                            break;
                    }
                }
                if (!previewOnly) {
                    DoMethod(app, MUIM_Application_RemInputHandler, &ihn);
                }
                set(MainWindow, MUIA_Window_Open, FALSE);
            }
            MUI_DisposeObject(app);
        }
        MUI_DeleteCustomClass(logSlider);
        MUI_DeleteCustomClass(updater);
    }
}

struct TextAttr Topaz8 = { "topaz.font", 8, 0, 0, };

void GUIMain()
{
    struct TextFont *   font;
    struct Screen *     pubscreen;
    APTR                vi;

    font = OpenFont(&Topaz8);
    
    if (font != NULL)
    {
        pubscreen = LockPubScreen(NULL);

        if (pubscreen)
        {
            vi = GetVisualInfo(pubscreen, TAG_DONE);

            if (vi != NULL)
            {

                FreeVisualInfo(vi);
            }

            UnlockPubScreen(NULL, pubscreen);
        }

        CloseFont(font);
    }
}

#define RDA_TEMPLATE "LOAD/K,ICNT=InstructionCount/K/N,IRNG=InliningRange/K/N,LCNT=LoopCount/K/N,CACHE/S,NOCACHE/S,SC=SlowdownCHIP/S,NSC=NoSlowdownCHIP/S,SCS=SlowCHIPSpacing/K/N,DBF=SlowdownDBF/S,NDBF=NoSlowdownDBF/S,SF=SoftFlush/S,SFL=SoftFlushLimit/K/N,CCRD=CCRScanDepth/K/N,BW=BlitWait/S,NBW=NoBlitWait/S,GUI/S,S=Silent/S,DEF=LoadDefaults/S,PREVIEW/S"

enum {
    OPT_PRESET_LOAD,
    OPT_INSN_COUNT,
    OPT_INLINE_RANGE,
    OPT_LOOP_CNT,
    OPT_FAST_CACHE,
    OPT_SLOW_CACHE,
    OPT_CHIP_SLOWDOWN,
    OPT_NO_CHIP_SLOWDOWN,
    OPT_CHIP_SLOWDOWN_SPACING,
    OPT_DBF_SLOWDOWN,
    OPT_NO_DBF_SLOWDOWN,
    OPT_SOFT_FLUSH,
    OPT_SOFT_FLUSH_LIMIT,
    OPT_CCR_SCAN_DEPTH,
    OPT_BLIT_WAIT,
    OPT_NO_BLIT_WAIT,
    OPT_GUI,
    OPT_SILENT,
    OPT_DEFAULTS,
    OPT_PREVIEW,
    OPT_COUNT
};

LONG result[OPT_COUNT];

int main(int wantGUI)
{
    struct RDArgs *args;
    SysBase = *(struct ExecBase **)4;
    
    InitMailBox();

    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37);
    if (DOSBase == NULL)
        return -1;

    if (!wantGUI)
    {
        ULONG silent = 0;

        args = ReadArgs(RDA_TEMPLATE, result, NULL);

        if (args)
        {
            wantGUI = result[OPT_GUI];
            previewOnly = result[OPT_PREVIEW];
            silent = result[OPT_SILENT];

            if (!silent)
                Printf("%s\n", (ULONG)&VERSION_STRING[6]);

            if (result[OPT_PRESET_LOAD])
            {
                STRPTR name = (STRPTR)(result[OPT_PRESET_LOAD]);
                struct Preset *p = AllocMem(sizeof(struct Preset), MEMF_CLEAR);

                if (p)
                {
                    if (LoadPreset(p, name, NULL)) {
                        if (!silent)
                            Printf("Loading preset '%s'\n", (ULONG)name);

                        wantGUI = 0;

                        if (p->pr_InlineLoopCnt == 0)
                            p->pr_InlineLoopCnt = 1;
                        else if (p->pr_InlineLoopCnt > 16)
                            p->pr_InlineLoopCnt = 16;

                        if (p->pr_CCRDepth > 31)
                            p->pr_CCRDepth = 31;

                        if (p->pr_SoftFlushThreshold == 0)
                            p->pr_SoftFlushThreshold = 1;
                        else if (p->pr_SoftFlushThreshold > 4000)
                            p->pr_SoftFlushThreshold = 4000;

                        APTR ssp = SuperState();

                        setINLINE_RANGE(p->pr_InlineRange);
                        setSOFT_THRESH(p->pr_SoftFlushThreshold);
                        setDEBUG_LOW(p->pr_DebugStart);
                        setDEBUG_HIGH(p->pr_DebugEnd);
                        setDEBUG_EN(p->pr_DebugFlag & DBGF_DEBUG_ON);
                        setDEBUG_DISASM(p->pr_DebugFlag & DBGF_DISASM_ON);
                        setINSN_DEPTH((ULONG)p->pr_INSNDepth + 1);
                        setLOOP_COUNT(p->pr_InlineLoopCnt);
                        setCCR_DEPTH(p->pr_CCRDepth);
                        setSLOWDOWN_CHIP(p->pr_JITFlags & JITF_SLOW_CHIP);
                        setSLOWDOWN_DBF(p->pr_JITFlags & JITF_SLOW_DBF);
                        setSOFT_FLUSH(p->pr_JITFlags & JITF_SOFT_FLUSH);
                        setCACHE_IE(p->pr_JITFlags & JITF_FAST_CACHE);
                        setSLOW_CHIP_SPACING(p->pr_SlowChipSpacing);
                        setBLIT_WAIT(p->pr_JITFlags & JITF_BLIT_WAIT);
                            
                        if (ssp)
                            UserState(ssp);
                    }
                    FreeMem(p, sizeof(struct Preset));
                }
            }
            else if (result[OPT_DEFAULTS])
            {
                if (!silent)
                    Printf("Resetting to defaults\n");
                
                wantGUI = 0;
                
                APTR ssp = SuperState();

                setINLINE_RANGE(8191);
                setSOFT_THRESH(500);
                setDEBUG_LOW(0);
                setDEBUG_HIGH(0xffffffff);
                setDEBUG_EN(0);
                setDEBUG_DISASM(0);
                setINSN_DEPTH(256);
                setLOOP_COUNT(8);
                setCCR_DEPTH(20);
                setSLOWDOWN_CHIP(0);
                setSLOWDOWN_DBF(0);
                setSOFT_FLUSH(1);
                setCACHE_IE(1);
                setBLIT_WAIT(0);

                if (ssp) UserState(ssp);
            }
            else
            {
                if (result[OPT_INSN_COUNT]) {
                    ULONG icnt = *(ULONG*)(result[OPT_INSN_COUNT]);
                    
                    if (icnt < 1)
                        icnt = 1;
                    if (icnt > 256)
                        icnt = 256;

                    if (!silent)
                        Printf("- Changing JIT instruction depth to %ld\n", icnt);

                    APTR ssp = SuperState();
                    setINSN_DEPTH(icnt);
                    if (ssp)
                        UserState(ssp);
                }

                if (result[OPT_CHIP_SLOWDOWN_SPACING]) {
                    ULONG sp = *(ULONG*)(result[OPT_CHIP_SLOWDOWN_SPACING]);
                    
                    if (sp < 1)
                        sp = 1;
                    if (sp > 8)
                        sp = 8;

                    if (!silent)
                        Printf("- Changing Chip slowdown spacing to %ld\n", sp);

                    APTR ssp = SuperState();
                    setSLOW_CHIP_SPACING(sp);
                    if (ssp)
                        UserState(ssp);
                }

                if (result[OPT_INLINE_RANGE])
                {
                    ULONG irng = *(ULONG*)(result[OPT_INLINE_RANGE]);
                    
                    if (irng < 1)
                        irng = 1;
                    if (irng > 65535)
                        irng = 65535;

                    if (!silent)
                        Printf("- Changing JIT inlining range to %ld\n", irng);

                    APTR ssp = SuperState();
                    setINLINE_RANGE(irng);
                    if (ssp)
                        UserState(ssp);
                }

                if (result[OPT_SLOW_CACHE]) {
                    if (!silent)
                        Printf("- Enabling checksummed slow JIT cache\n");

                    APTR ssp = SuperState();
                    setCACHE_IE(0);
                    if (ssp)
                        UserState(ssp);
                }

                if (result[OPT_FAST_CACHE] && !result[OPT_SLOW_CACHE]) {

                    if (!silent)
                        Printf("- Enabling fast JIT cache\n");

                    APTR ssp = SuperState();
                    setCACHE_IE(1);
                    if (ssp)
                        UserState(ssp);
                }

                if (result[OPT_CHIP_SLOWDOWN]) {
                    if (!silent)
                        Printf("- Enabling slowdown of JIT running from CHIP memory\n");

                    APTR ssp = SuperState();
                    setSLOWDOWN_CHIP(1);
                    if (ssp)
                        UserState(ssp);
                }

                if (result[OPT_NO_CHIP_SLOWDOWN] && !result[OPT_CHIP_SLOWDOWN]) {
                    if (!silent)
                        Printf("- Disabling slowdown of JIT running from CHIP memory\n");

                    APTR ssp = SuperState();
                    setSLOWDOWN_CHIP(0);
                    if (ssp)
                        UserState(ssp);
                }

                if (result[OPT_DBF_SLOWDOWN]) {
                    if (!silent)
                        Printf("- Enabling slowdown of DBF busy loops running from CHIP memory\n");

                    APTR ssp = SuperState();
                    setSLOWDOWN_DBF(1);
                    if (ssp)
                        UserState(ssp);
                }

                if (result[OPT_NO_DBF_SLOWDOWN] && !result[OPT_DBF_SLOWDOWN]) {
                    if (!silent)
                        Printf("- Disabling slowdown of DBF busy loops running from CHIP memory\n");

                    APTR ssp = SuperState();
                    setSLOWDOWN_DBF(0);
                    if (ssp)
                        UserState(ssp);
                }

                if (result[OPT_CCR_SCAN_DEPTH]) {
                    ULONG ccrd = *(ULONG*)(result[OPT_CCR_SCAN_DEPTH]);
                    
                    if (ccrd > 31)
                        ccrd = 31;

                    if (!silent)
                        Printf("- Changing CCR scan depth to %ld\n", ccrd);

                    APTR ssp = SuperState();
                    setCCR_DEPTH(ccrd);
                    if (ssp)
                        UserState(ssp);
                }

                if (result[OPT_BLIT_WAIT]) {
                    if (!silent)
                        Printf("- Enabling automatic blitter wait\n");

                    APTR ssp = SuperState();
                    setBLIT_WAIT(1);
                    if (ssp)
                        UserState(ssp);
                }

                if (result[OPT_NO_BLIT_WAIT] && !result[OPT_BLIT_WAIT]) {
                    if (!silent)
                        Printf("- Disabling automatic blitter wait\n");

                    APTR ssp = SuperState();
                    setBLIT_WAIT(0);
                    if (ssp)
                        UserState(ssp);
                }
            }

            FreeArgs(args);
        }
    }

    if (wantGUI)
    {
        SetTaskPri(FindTask(NULL), 120);

        IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
        if (IntuitionBase != NULL)
        {
            GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37);
            if (GfxBase != NULL)
            {
                MUIMasterBase = OpenLibrary("muimaster.library", 0);
                if (MUIMasterBase != NULL)
                {
                    MUIMain();
                    CloseLibrary(MUIMasterBase);
                }
                else
                {
                    GadToolsBase = OpenLibrary("gadtools.library", 37);
                    if (GadToolsBase != NULL)
                    {
                        GUIMain();
                        CloseLibrary(GadToolsBase);
                    }
                }

                CloseLibrary((struct Library *)GfxBase);
            }
            CloseLibrary((struct Library *)IntuitionBase);
        }
    }

    CloseLibrary((struct Library *)DOSBase);
    return 0;
}
