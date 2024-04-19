/* Automatically generated header (sfdc 1.11f)! Do not edit! */

#ifndef _INLINE_GIC_H
#define _INLINE_GIC_H

#ifndef _SFDC_VARARG_DEFINED
#define _SFDC_VARARG_DEFINED
#ifdef __HAVE_IPTR_ATTR__
typedef APTR _sfdc_vararg __attribute__((iptr));
#else
typedef ULONG _sfdc_vararg;
#endif /* __HAVE_IPTR_ATTR__ */
#endif /* _SFDC_VARARG_DEFINED */

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif /* !__INLINE_MACROS_H */

#ifndef GIC_BASE_NAME
#define GIC_BASE_NAME GICBase
#endif /* !GIC_BASE_NAME */

#define GIC_AddIntServer(___intNum, ___interrupt) \
      LP2NR(0x6, GIC_AddIntServer , ULONG, ___intNum, d0, struct Interrupt *, ___interrupt, a1,\
      , GIC_BASE_NAME)

#define GIC_RemIntServer(___intNum, ___interrupt) \
      LP2NR(0xc, GIC_RemIntServer , ULONG, ___intNum, d0, struct Interrupt *, ___interrupt, a1,\
      , GIC_BASE_NAME)

#endif /* !_INLINE_GIC_H */
