/* Automatically generated header (sfdc 1.11f)! Do not edit! */

#ifndef _INLINE_UNICAM_H
#define _INLINE_UNICAM_H

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

#ifndef UNICAM_BASE_NAME
#define UNICAM_BASE_NAME UnicamBase
#endif /* !UNICAM_BASE_NAME */

#define UnicamStart(___address, ___lanes, ___datatype, ___width, ___height, ___bbp) \
      LP6NR(0x6, UnicamStart , ULONG *, ___address, a0, UBYTE, ___lanes, d0, UBYTE, ___datatype, d1, ULONG, ___width, d2, ULONG, ___height, d3, UBYTE, ___bbp, d4,\
      , UNICAM_BASE_NAME)

#define UnicamStop() \
      LP0NR(0xc, UnicamStop ,\
      , UNICAM_BASE_NAME)

#endif /* !_INLINE_UNICAM_H */
