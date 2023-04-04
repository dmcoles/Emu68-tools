/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <exec/types.h>
#include <proto/exec.h>
#include <common/endian.h>
#include <common/bcm_i2c.h>
#include <i2c_private.h>

STRPTR I2CErrText(
	REGARG(ULONG ulErrorCode, "d0"),
	REGARG(struct I2C_Base *i2cBase, "a6")
)
{
	UBYTE isSuccess = (ulErrorCode >> 0) & 0xFF;
	UBYTE ubIoError = (ulErrorCode >> 8) & 0xFF;
	UBYTE ubAllocError = (ulErrorCode >> 16) & 0xFF;
	if(isSuccess) {
		return "OK";
	}

	switch(ubIoError) {
		case 1:  return "data rejected";
		case 2:  return "no reply";
		case 3:  return "SDA trashed";
		case 4:  return "SDA always LO";
		case 5:  return "SDA always HI";
		case 8:
			// Decode alloc error
			switch(ubAllocError) {
				case 0: return "hardware is busy"; // IO error 8
				case 1: return "port is busy";
				case 2: return "port bits are busy";
				case 3: return "no ";
				case 5: return "temporary shutdown";
			}
	}

	// According to autodocs, errors should be at most 20-character long
	return "???";
}
