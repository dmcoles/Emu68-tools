/* Automatically generated header (sfdc 1.11f)! Do not edit! */

#ifndef PROTO_UNICAM_H
#define PROTO_UNICAM_H

#include <clib/unicam_protos.h>

#ifndef _NO_INLINE
# if defined(__GNUC__)
#  ifdef __AROS__
#   include <defines/unicam.h>
#  else
#   include <inline/unicam.h>
#  endif
# else
#  include <pragmas/unicam_pragmas.h>
# endif
#endif /* _NO_INLINE */

#ifdef __amigaos4__
# include <interfaces/unicam.h>
# ifndef __NOGLOBALIFACE__
   extern struct UnicamIFace *IUnicam;
# endif /* __NOGLOBALIFACE__*/
#endif /* !__amigaos4__ */
#ifndef __NOLIBBASE__
  extern APTR
# ifdef __CONSTLIBBASEDECL__
   __CONSTLIBBASEDECL__
# endif /* __CONSTLIBBASEDECL__ */
  UnicamBase;
#endif /* !__NOLIBBASE__ */

#endif /* !PROTO_UNICAM_H */
