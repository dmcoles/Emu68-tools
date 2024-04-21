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

#define UnicamGetFramebuffer() \
      LP0(0x12, APTR, UnicamGetFramebuffer ,\
      , UNICAM_BASE_NAME)

#define UnicamGetFramebufferSize() \
      LP0(0x18, ULONG, UnicamGetFramebufferSize ,\
      , UNICAM_BASE_NAME)

#define UnicamGetCropSize() \
      LP0(0x1e, ULONG, UnicamGetCropSize ,\
      , UNICAM_BASE_NAME)

#define UnicamGetCropOffset() \
      LP0(0x24, ULONG, UnicamGetCropOffset ,\
      , UNICAM_BASE_NAME)

#define UnicamGetKernel() \
      LP0(0x2a, ULONG, UnicamGetKernel ,\
      , UNICAM_BASE_NAME)

#define UnicamGetConfig() \
      LP0(0x30, ULONG, UnicamGetConfig ,\
      , UNICAM_BASE_NAME)

#define UnicamGetSize() \
      LP0(0x36, ULONG, UnicamGetSize ,\
      , UNICAM_BASE_NAME)

#define UnicamGetMode() \
      LP0(0x3c, ULONG, UnicamGetMode ,\
      , UNICAM_BASE_NAME)

#endif /* !_INLINE_UNICAM_H */
