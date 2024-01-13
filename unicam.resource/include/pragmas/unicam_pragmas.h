/* Automatically generated header (sfdc 1.11f)! Do not edit! */
#ifndef PRAGMAS_UNICAM_PRAGMAS_H
#define PRAGMAS_UNICAM_PRAGMAS_H

/*
**   $VER: unicam_pragmas.h 0.1.0 $Id: unicam_lib.sfd 0.1.0 $
**
**   Direct ROM interface (pragma) definitions.
**
**   Copyright (c) 2001 Amiga, Inc.
**       All Rights Reserved
*/

#if defined(LATTICE) || defined(__SASC) || defined(_DCC)
#ifndef __CLIB_PRAGMA_LIBCALL
#define __CLIB_PRAGMA_LIBCALL
#endif /* __CLIB_PRAGMA_LIBCALL */
#else /* __MAXON__, __STORM__ or AZTEC_C */
#ifndef __CLIB_PRAGMA_AMICALL
#define __CLIB_PRAGMA_AMICALL
#endif /* __CLIB_PRAGMA_AMICALL */
#endif /* */

#if defined(__SASC_60) || defined(__STORM__)
#ifndef __CLIB_PRAGMA_TAGCALL
#define __CLIB_PRAGMA_TAGCALL
#endif /* __CLIB_PRAGMA_TAGCALL */
#endif /* __MAXON__, __STORM__ or AZTEC_C */

#ifdef __CLIB_PRAGMA_LIBCALL
 #pragma libcall UnicamBase UnicamStart 6 43210806
#endif /* __CLIB_PRAGMA_LIBCALL */
#ifdef __CLIB_PRAGMA_AMICALL
 #pragma amicall(UnicamBase, 0x6, UnicamStart(a0,d0,d1,d2,d3,d4))
#endif /* __CLIB_PRAGMA_AMICALL */
#ifdef __CLIB_PRAGMA_LIBCALL
 #pragma libcall UnicamBase UnicamStop c 00
#endif /* __CLIB_PRAGMA_LIBCALL */
#ifdef __CLIB_PRAGMA_AMICALL
 #pragma amicall(UnicamBase, 0xc, UnicamStop())
#endif /* __CLIB_PRAGMA_AMICALL */
#ifdef __CLIB_PRAGMA_LIBCALL
 #pragma libcall UnicamBase UnicamGetFramebuffer 12 00
#endif /* __CLIB_PRAGMA_LIBCALL */
#ifdef __CLIB_PRAGMA_AMICALL
 #pragma amicall(UnicamBase, 0x12, UnicamGetFramebuffer())
#endif /* __CLIB_PRAGMA_AMICALL */
#ifdef __CLIB_PRAGMA_LIBCALL
 #pragma libcall UnicamBase UnicamGetFramebufferSize 18 00
#endif /* __CLIB_PRAGMA_LIBCALL */
#ifdef __CLIB_PRAGMA_AMICALL
 #pragma amicall(UnicamBase, 0x18, UnicamGetFramebufferSize())
#endif /* __CLIB_PRAGMA_AMICALL */
#ifdef __CLIB_PRAGMA_LIBCALL
 #pragma libcall UnicamBase UnicamGetCropSize 1e 00
#endif /* __CLIB_PRAGMA_LIBCALL */
#ifdef __CLIB_PRAGMA_AMICALL
 #pragma amicall(UnicamBase, 0x1e, UnicamGetCropSize())
#endif /* __CLIB_PRAGMA_AMICALL */
#ifdef __CLIB_PRAGMA_LIBCALL
 #pragma libcall UnicamBase UnicamGetCropOffset 24 00
#endif /* __CLIB_PRAGMA_LIBCALL */
#ifdef __CLIB_PRAGMA_AMICALL
 #pragma amicall(UnicamBase, 0x24, UnicamGetCropOffset())
#endif /* __CLIB_PRAGMA_AMICALL */
#ifdef __CLIB_PRAGMA_LIBCALL
 #pragma libcall UnicamBase UnicamGetKernel 2a 00
#endif /* __CLIB_PRAGMA_LIBCALL */
#ifdef __CLIB_PRAGMA_AMICALL
 #pragma amicall(UnicamBase, 0x2a, UnicamGetKernel())
#endif /* __CLIB_PRAGMA_AMICALL */
#ifdef __CLIB_PRAGMA_LIBCALL
 #pragma libcall UnicamBase UnicamGetConfig 30 00
#endif /* __CLIB_PRAGMA_LIBCALL */
#ifdef __CLIB_PRAGMA_AMICALL
 #pragma amicall(UnicamBase, 0x30, UnicamGetConfig())
#endif /* __CLIB_PRAGMA_AMICALL */

#endif /* PRAGMAS_UNICAM_PRAGMAS_H */
