#ifndef _UNICAM_H
#define _UNICAM_H

#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <common/compiler.h>
#include <stdint.h>

#define UNICAM_VERSION  0
#define UNICAM_REVISION 1
#define UNICAM_PRIORITY 100

struct UnicamBase {
    struct Library      u_Node;
    struct ExecBase *   u_SysBase;
    APTR                u_ROMBase;
    struct ConfigDev *  u_ConfigDev;
    APTR                u_MailBox;
    ULONG *             u_Request;
    APTR                u_RequestBase;

    BOOL                u_IsVC6;
};

#define UNICAM_FUNC_COUNT   0
#define BASE_NEG_SIZE       ((UNICAM_FUNC_COUNT) * 6)
#define BASE_POS_SIZE       (sizeof(struct UnicamBase))

static inline uint64_t LE64(uint64_t x) { return __builtin_bswap64(x); }
static inline uint32_t LE32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t LE16(uint16_t x) { return __builtin_bswap16(x); }

extern void kprintf(REGARG(const char * msg, "a0"), REGARG(void * args, "a1"));

#define bug(string, ...) \
    do { ULONG args[] = {0, __VA_ARGS__}; kprintf(string, &args[1]); } while(0)

#endif /* _UNICAM_H */
