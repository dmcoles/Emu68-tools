/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <exec/types.h>
#include <proto/exec.h>
#include <common/endian.h>
#include <common/bcm_i2c.h>
#include <i2c_private.h>
#include "../gpio.h"
#include "../bcm_peripheral.h"

#define CORE_CLOCK (150*1000*1000)
#define I2C_SPEED 100000

BYTE AllocI2C(
	__attribute__((unused)) REGARG(UBYTE Delay_Type, "d0"),
	__attribute__((unused)) REGARG(char *Name, "a1"),
	REGARG(struct I2C_Base *i2cBase, "a6")
)
{
	if(i2cBase->LibNode.lib_OpenCnt != 1) {
		return I2C_OK;
	}

	i2cBase->SendCalls = 0;
	i2cBase->SendBytes = 0;
	i2cBase->RecvCalls = 0;
	i2cBase->RecvBytes = 0;
	i2cBase->Lost = 0;
	i2cBase->Unheard = 0;
	i2cBase->Overflows = 0;
	i2cBase->Errors = 0;

	i2cBase->HwType = 2; // use legacy value because not every app supports arbitrary numbers

	struct ExecBase *SysBase = i2cBase->SysBase;
	InitSemaphore(&i2cBase->SemIo);

	// hardcoded BSC0 on pins 44,45
	// TODO: get from devicetree
	volatile tI2cRegs *pI2c = BCM_I2C0;
	volatile tGpioRegs *pGpio = BCM_GPIO;
	UBYTE ubPinSda = 44;
	UBYTE ubPinScl = 45;
	i2cBase->I2cHwRegs = (APTR)pI2c;

	gpioSetAlternate(pGpio, ubPinSda, GPIO44_AF_I2C0_SDA);
	gpioSetAlternate(pGpio, ubPinScl, GPIO45_AF_I2C0_SCL);
	gpioSetPull(pGpio, ubPinSda, GPIO_PULL_OFF);
	gpioSetPull(pGpio, ubPinScl, GPIO_PULL_OFF);
	wr32le(&pI2c->DIV, 6 * (CORE_CLOCK / I2C_SPEED));
	return I2C_OK;
}
