#ifndef _PRESETS_H
#define _PRESETS_H

#include <exec/types.h>
#include <stdint.h>

struct Preset {
    UWORD pr_InlineRange;
    UWORD pr_SoftFlushThreshold;
    ULONG pr_DebugStart;
    ULONG pr_DebugEnd;
    UBYTE pr_DebugFlag;
    UBYTE pr_INSNDepth;
    UBYTE pr_InlineLoopCnt;
    UBYTE pr_CCRDepth;
    UBYTE pr_JITFlags;
};

#define JITF_SOFT_FLUSH     0x01
#define JITF_FAST_CACHE     0x02
#define JITF_SLOW_CHIP      0x04
#define JITF_SLOW_DBF       0x08

#define DBGF_DEBUG_ON       0x01
#define DBGF_DISASM_ON      0x02


int SavePreset(struct Preset * preset, char *name, char *path);
int LoadPreset(struct Preset * preset, char *name, char *path);

#endif /* _PRESETS_H */
