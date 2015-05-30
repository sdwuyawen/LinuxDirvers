/*
 * em9287_gpio.c
 *
 *  Created on: 2014年9月28日
 *      Author: Administrator
 */

#include "em9287_gpio.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "em9280_drivers.h"



int GPIO_OutEnable(int fd, unsigned int dwEnBits)
{
	int 				rc;
	struct double_pars	dpars;

	dpars.par1 = EM9280_GPIO_OUTPUT_ENABLE;		// 0
	dpars.par2 = dwEnBits;

	rc = write(fd, &dpars, sizeof(struct double_pars));
	return rc;
}

int GPIO_OutDisable(int fd, unsigned int dwDisBits)
{
	int 				rc;
	struct double_pars	dpars;

	dpars.par1 = EM9280_GPIO_OUTPUT_DISABLE;	// 1
	dpars.par2 = dwDisBits;

	rc = write(fd, &dpars, sizeof(struct double_pars));
	return rc;
}

int GPIO_OutSet(int fd, unsigned int dwSetBits)
{
	int 				rc;
	struct double_pars	dpars;

	dpars.par1 = EM9280_GPIO_OUTPUT_SET;	// 2
	dpars.par2 = dwSetBits;

	rc = write(fd, &dpars, sizeof(struct double_pars));
	return rc;
}

int GPIO_OutClear(int fd, unsigned int dwClearBits)
{
	int 				rc;
	struct double_pars	dpars;

	dpars.par1 = EM9280_GPIO_OUTPUT_CLEAR;	// 3
	dpars.par2 = dwClearBits;

	rc = write(fd, &dpars, sizeof(struct double_pars));
	return rc;
}

int GPIO_PinState(int fd, unsigned int* pPinState)
{
	int 				rc;
	struct double_pars	dpars;

	dpars.par1 = EM9280_GPIO_INPUT_STATE;	// 5
	dpars.par2 = *pPinState;

	rc = read(fd, &dpars, sizeof(struct double_pars));
	if(!rc)
	{
		*pPinState = dpars.par2;
	}
	return rc;
}


