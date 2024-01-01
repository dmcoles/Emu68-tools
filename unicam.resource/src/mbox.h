#ifndef _MBOX_H
#define _MBOX_H

#include "unicam.h"
#include <stdint.h>

ULONG enable_unicam_domain(struct UnicamBase *UnicamBase);
struct Size get_display_size(struct UnicamBase *UnicamBase);

#endif /* _MBOX_H */
