#ifndef _WIFIPI_H
#define _WIFIPI_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/devices.h>

#include <dos/dos.h>
#include <libraries/dos.h>
#include <intuition/intuitionbase.h>
#include <libraries/expansionbase.h>

#include <stdint.h>

#if defined(__INTELLISENSE__)
#define asm(x) /* x */
#endif

#define STR(s) #s
#define XSTR(s) STR(s)

#define WIFIPI_VERSION  0
#define WIFIPI_REVISION 1
#define WIFIPI_PRIORITY -100

struct WiFiUnit;

struct WiFiBase
{
    struct Device       w_Device;
    BPTR                w_SegList;
    struct ExecBase *   w_SysBase;
    struct Library *    w_UtilityBase;
    struct WiFiUnit *   w_Unit;
    struct Library *    w_DosBase;
    APTR                w_DeviceTreeBase;
    APTR                w_SDIO;
    APTR                w_MailBox;
    APTR                w_GPIO;
    APTR                w_RequestOrig;
    ULONG *             w_Request;
    ULONG               w_SDIOClock;
    ULONG               w_BlockSize;
    ULONG               w_BlocksToTransfer;
    ULONG               w_LastError;
    ULONG               w_LastInterrupt;
    ULONG               w_Res0;
    ULONG               w_Res1;
    ULONG               w_Res2;
    ULONG               w_Res3;
    APTR                w_Buffer;
    ULONG               w_CardRCA;
    ULONG               w_LastCMD;
    UBYTE               w_LastCMDSuccess;
    ULONG               w_LastBackplaneWindow;

    APTR                w_FirmwareBase;
    ULONG               w_FirmwareSize;

    APTR                w_CLMBase;
    ULONG               w_CLMSize;

    APTR                w_ConfigBase;
    ULONG               w_ConfigSize;
};

struct WiFiUnit
{
    struct Unit         wu_Unit;
    struct MinList      wu_Openers;
    struct WiFiBase *   wu_Base;
    ULONG               wu_Flags;
};

struct Opener
{
    struct MinNode      o_Node;
    struct MsgPort      o_ReadPort;
    struct Hook *       o_FilterHook;
    BOOL              (*o_RXFunc)(APTR, APTR, ULONG);
    BOOL              (*o_TXFunc)(APTR, APTR, ULONG);
};

/* Standard interface flags (netdevice->flags). */
#define IFF_UP          0x1             /* interface is up              */
#define IFF_BROADCAST   0x2             /* broadcast address valid      */
#define IFF_DEBUG       0x4             /* turn on debugging            */
#define IFF_LOOPBACK    0x8             /* is a loopback net            */
#define IFF_POINTOPOINT 0x10            /* interface is has p-p link    */
#define IFF_NOTRAILERS  0x20            /* avoid use of trailers        */
#define IFF_RUNNING     0x40            /* resources allocated          */
#define IFF_NOARP       0x80            /* no ARP protocol              */
#define IFF_PROMISC     0x100           /* receive all packets          */
#define IFF_ALLMULTI    0x200           /* receive all multicast packets*/

#define IFF_MASTER      0x400           /* master of a load balancer    */
#define IFF_SLAVE       0x800           /* slave of a load balancer     */

#define IFF_MULTICAST   0x1000          /* Supports multicast           */

#define IFF_VOLATILE    (IFF_LOOPBACK|IFF_POINTOPOINT|IFF_BROADCAST|IFF_MASTER|IFF_SLAVE|IFF_RUNNING)

#define IFF_PORTSEL     0x2000          /* can set media type           */
#define IFF_AUTOMEDIA   0x4000          /* auto media select active     */
#define IFF_DYNAMIC     0x8000          /* dialup device with changing addresses*/
#define IFF_SHARED      0x10000         /* interface may be shared */
#define IFF_CONFIGURED  0x20000         /* interface already configured */

static inline __attribute__((always_inline)) void putch(UBYTE data asm("d0"), APTR ignore asm("a3"))
{
    (void)ignore;
    *(UBYTE*)0xdeadbeef = data;
}


static inline uint64_t LE64(uint64_t x) { return __builtin_bswap64(x); }
static inline uint32_t LE32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t LE16(uint16_t x) { return __builtin_bswap16(x); }

static inline ULONG rd32(APTR addr, ULONG offset)
{
    APTR addr_off = (APTR)((ULONG)addr + offset);
    ULONG val = LE32(*(volatile ULONG *)addr_off);
    asm volatile("nop");
    return val;
}

static inline void wr32(APTR addr, ULONG offset, ULONG val)
{
    APTR addr_off = (APTR)((ULONG)addr + offset);
    *(volatile ULONG *)addr_off = LE32(val);
    asm volatile("nop");
}

static inline ULONG rd32be(APTR addr, ULONG offset)
{
    APTR addr_off = (APTR)((ULONG)addr + offset);
    ULONG val = *(volatile ULONG *)addr_off;
    asm volatile("nop");
    return val;
}

static inline void wr32be(APTR addr, ULONG offset, ULONG val)
{
    APTR addr_off = (APTR)((ULONG)addr + offset);
    *(volatile ULONG *)addr_off = val;
    asm volatile("nop");
}

struct WiFiBase * WiFi_Init(struct WiFiBase *base asm("d0"), BPTR seglist asm("a0"), struct ExecBase *SysBase asm("a6"));

#define bug(string, ...) \
    do { ULONG args[] = {0, __VA_ARGS__}; RawDoFmt(string, &args[1], (APTR)putch, NULL); } while(0)


BOOL LoadFirmware(struct WiFiBase *WiFiBase, UWORD chipID, UWORD chipREV);

#endif
