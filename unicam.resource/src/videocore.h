#ifndef _VIDEOCORE_H
#define _VIDEOCORE_H

#include "unicam.h"
#include <stdint.h>

void VC4_ConstructUnicamDL(struct UnicamBase *UnicamBase, ULONG kernel);
void VC6_ConstructUnicamDL(struct UnicamBase *UnicamBase, ULONG kernel);

#endif /* _VIDEOCORE_H */
