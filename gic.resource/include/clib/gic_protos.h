/* Automatically generated header (sfdc 1.11f)! Do not edit! */

#ifndef CLIB_GIC_PROTOS_H
#define CLIB_GIC_PROTOS_H

/*
**   $VER: gic_protos.h 0.1.0 $Id: gic_lib.sfd 0.1.0 $
**
**   C prototypes. For use with 32 bit integers only.
**
**   Copyright (c) 2001 Amiga, Inc.
**       All Rights Reserved
*/

#include <exec/types.h>
#include <exec/interrupts.h>
#include <resources/gic.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* "gic.resource" */
void GIC_AddIntServer(ULONG intNum, struct Interrupt * interrupt);
void GIC_RemIntServer(ULONG intNum, struct Interrupt * interrupt);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CLIB_GIC_PROTOS_H */
