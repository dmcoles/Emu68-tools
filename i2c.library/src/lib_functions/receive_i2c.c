/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <exec/types.h>
#include <proto/exec.h>
#include <common/endian.h>
#include <common/bcm_i2c.h>
#include <i2c_private.h>

#define RESULT(isError, ubIoError, ubAllocError) ((ubAllocError << 16) | (ubIoError << 8) | (isError))

ULONG ReceiveI2C(
	REGARG(UBYTE ubAddress, "d0"),
	REGARG(UWORD uwDataSize, "d1"),
	REGARG(UBYTE pData[], "a0"),
	REGARG(struct I2C_Base *i2cBase, "a6")
)
{
	// TODO: Semaphore shared with ReceiveI2C
	// TODO: Implement
	++i2cBase->RecvCalls;
	UBYTE isError = 1, ubIoError = I2C_OK, ubAllocError = I2C_OK;
	return RESULT(isError, ubIoError, ubAllocError);
}
