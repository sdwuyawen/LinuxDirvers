#ifndef	_S3C2416_H_
#define 	_S3C2416_H_

#define __REG(x)		(*((volatile unsigned long *)(x)))
#define __REGb(x)	(*(volatile unsigned char *)(x))

#define GPFCON		__REG(0x56000050)
#define GPFDAT		__REG(0x56000054)
#define GPFUDP		__REG(0x56000058)

#define GPGCON		__REG(0x56000060)
#define GPGDAT		__REG(0x56000064)
#define GPGUDP		__REG(0x56000068)

#define GPHCON		__REG(0x56000070)
#define GPHDAT		__REG(0x56000074)
#define GPHUDP		__REG(0x56000078)

#define GPECON		__REG(0x56000040)
#define GPEUDP		__REG(0x56000048)


#define INTMSK          __REG(0x4A000008)
#define SRCPND		__REG(0x4A000000)
#define INTPND		__REG(0x4A000010)

#if 0
#define SRCPND		(*(volatile unsigned long *)0x4A000000)
#define INTPND		(*(volatile unsigned long *)0x4A000010)
#endif

#endif