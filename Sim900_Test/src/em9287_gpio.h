/*
 * rs485_gpio.h
 *
 *  Created on: 2014年9月28日
 *      Author: LDD
 */

#ifndef RS485_GPIO_H_
#define RS485_GPIO_H_


/* Add this for macros that defined unix flavor */
#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif



#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include "em9280_drivers.h"

#ifdef  __cplusplus
# define MODBUS_BEGIN_DECLS  extern "C" {
# define MODBUS_END_DECLS    }
#else
# define MODBUS_BEGIN_DECLS
# define MODBUS_END_DECLS
#endif

MODBUS_BEGIN_DECLS

int GPIO_OutEnable(int fd, unsigned int dwEnBits);
int GPIO_OutDisable(int fd, unsigned int dwDisBits);
int GPIO_OpenDrainEnable(int fd, unsigned int dwODBits);
int GPIO_OutSet(int fd, unsigned int dwSetBits);
int GPIO_OutClear(int fd, unsigned int dwClearBits);
int GPIO_PinState(int fd, unsigned int* pPinState);

MODBUS_END_DECLS
#endif /* RS485_GPIO_H_ */
