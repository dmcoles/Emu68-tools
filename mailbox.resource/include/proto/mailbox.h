/* Automatically generated header (sfdc 1.11f)! Do not edit! */

#ifndef PROTO_MAILBOX_H
#define PROTO_MAILBOX_H

#include <clib/mailbox_protos.h>

#ifndef _NO_INLINE
# if defined(__GNUC__)
#  ifdef __AROS__
#   include <defines/mailbox.h>
#  else
#   include <inline/mailbox.h>
#  endif
# else
#  include <pragmas/mailbox_pragmas.h>
# endif
#endif /* _NO_INLINE */

#ifdef __amigaos4__
# include <interfaces/mailbox.h>
# ifndef __NOGLOBALIFACE__
   extern struct MailboxIFace *IMailbox;
# endif /* __NOGLOBALIFACE__*/
#endif /* !__amigaos4__ */
#ifndef __NOLIBBASE__
  extern APTR
# ifdef __CONSTLIBBASEDECL__
   __CONSTLIBBASEDECL__
# endif /* __CONSTLIBBASEDECL__ */
  MailboxBase;
#endif /* !__NOLIBBASE__ */

#endif /* !PROTO_MAILBOX_H */
