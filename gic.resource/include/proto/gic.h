/* Automatically generated header (sfdc 1.11f)! Do not edit! */

#ifndef PROTO_GIC_H
#define PROTO_GIC_H

#include <clib/gic_protos.h>

#ifndef _NO_INLINE
# if defined(__GNUC__)
#  ifdef __AROS__
#   include <defines/gic.h>
#  else
#   include <inline/gic.h>
#  endif
# else
#  include <pragmas/gic_pragmas.h>
# endif
#endif /* _NO_INLINE */

#ifdef __amigaos4__
# include <interfaces/gic.h>
# ifndef __NOGLOBALIFACE__
   extern struct GICIFace *IGIC;
# endif /* __NOGLOBALIFACE__*/
#endif /* !__amigaos4__ */
#ifndef __NOLIBBASE__
  extern APTR
# ifdef __CONSTLIBBASEDECL__
   __CONSTLIBBASEDECL__
# endif /* __CONSTLIBBASEDECL__ */
  GICBase;
#endif /* !__NOLIBBASE__ */

#endif /* !PROTO_GIC_H */
