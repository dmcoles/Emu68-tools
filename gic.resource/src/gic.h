#ifndef _GIC_H_FE559305_A36C_454F_870C_019BD1804213
#define _GIC_H_FE559305_A36C_454F_870C_019BD1804213

#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <common/compiler.h>
#include <stdint.h>
#include <resources/gic.h>

#define GIC_VERSION 0
#define GIC_REVISION 2
#define GIC_PRIORITY 127

struct GICBase
{
    struct Library      g_Node;
    struct ExecBase *   g_SysBase;
    APTR                g_ROMBase;
    struct ConfigDev *  g_ConfigDev;

    struct List         g_IntServers[3];

    APTR                g_Handlers[3];
    UBYTE               g_Vectors[3];
};

#define GIC_FUNC_COUNT 2
#define BASE_NEG_SIZE ((GIC_FUNC_COUNT) * 6)
#define BASE_POS_SIZE (sizeof(struct GICBase))

static inline uint64_t LE64(uint64_t x) { return __builtin_bswap64(x); }
static inline uint32_t LE32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t LE16(uint16_t x) { return __builtin_bswap16(x); }

void kprintf(REGARG(const char *msg, "a0"), REGARG(void *args, "a1"));

#define bug(string, ...)                 \
    do                                   \
    {                                    \
        ULONG args[] = {0, __VA_ARGS__}; \
        kprintf(string, &args[1]);       \
    } while (0)

void L_GIC_AddIntServer(REGARG(ULONG intNum, "d0"), REGARG(struct Interrupt *interrupt, "a1"), REGARG(struct GICBase *GICBase, "a6"));
void L_GIC_RemIntServer(REGARG(ULONG intNum, "d0"), REGARG(struct Interrupt *interrupt, "a1"), REGARG(struct GICBase *GICBase, "a6"));

#endif /* _GIC_H_FE559305_A36C_454F_870C_019BD1804213 */
