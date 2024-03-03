/* Automatically generated header (sfdc 1.11f)! Do not edit! */

#ifndef _INLINE_MAILBOX_H
#define _INLINE_MAILBOX_H

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

#ifndef MAILBOX_BASE_NAME
#define MAILBOX_BASE_NAME MailboxBase
#endif /* !MAILBOX_BASE_NAME */

#define MB_Send(___MBox, ___data) \
      LP2NR(0x6, MB_Send , ULONG, ___MBox, d0, ULONG, ___data, d1,\
      , MAILBOX_BASE_NAME)

#define MB_Recv(___MBox) \
      LP1(0xc, ULONG, MB_Recv , ULONG, ___MBox, d0,\
      , MAILBOX_BASE_NAME)

#define MB_Request(___MBox, ___Request, ___ReqSize) \
      LP3NR(0x12, MB_Request , ULONG, ___MBox, d0, APTR, ___Request, a0, ULONG, ___ReqSize, d1,\
      , MAILBOX_BASE_NAME)

#endif /* !_INLINE_MAILBOX_H */
