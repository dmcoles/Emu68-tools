/* Automatically generated header (sfdc 1.11f)! Do not edit! */

#ifndef CLIB_MAILBOX_PROTOS_H
#define CLIB_MAILBOX_PROTOS_H

/*
**   $VER: mailbox_protos.h 1.0.0 $Id: mailbox_lib.sfd 1.0.0 $
**
**   C prototypes. For use with 32 bit integers only.
**
**   Copyright (c) 2001 Amiga, Inc.
**       All Rights Reserved
*/

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* "mailbox.resource" */
void MB_Send(ULONG MBox, ULONG data);
ULONG MB_Recv(ULONG MBox);
void MB_Request(ULONG MBox, APTR Request, ULONG ReqSize);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CLIB_MAILBOX_PROTOS_H */
