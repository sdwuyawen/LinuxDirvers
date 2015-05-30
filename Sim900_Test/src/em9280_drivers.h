#ifndef __ASM_ARCH_EM9280_DRIVERS_H
#define __ASM_ARCH_EM9280_DRIVERS_H

#include <asm-generic/ioctl.h>

#define EM9280_DEV_MAJOR			251
#define EM9280_SYSINFO_MINOR		0					//em9280_sysinfo
#define EM9280_ISA_MINOR			1					//em9280_isa
#define EM9280_GPIO_MINOR			2					//em9280_gpio
#define EM9280_KEYPAD_MINOR		3					//em9280_keypad
#define EM9280_IRQ1_MINOR			4					//em9280_irq1
#define EM9280_IRQ2_MINOR			5					//em9280_irq2 (EM9160 only)
#define EM9280_CAN1_MINOR			6					//em9280_can1
#define EM9280_CAN2_MINOR			7					//em9280_can2 (base on ISA)
#define EM9280_PWM1_MINOR			8					//em9280_pwm1(->GPIO12 of EM9160)
#define EM9280_PWM2_MINOR			9					//em9280_pwm2(->GPIO14 of EM9160)
#define EM9280_PWM3_MINOR			10					//em9280_pwm3(->GPIO15 of EM9160)
#define EM9280_I2C_MINOR			11					//em9280_i2c (GPIO based)
#define EM9280_LCD_MINOR			12					//em9280_lcd
#define EM9280_SPI_MINOR			13					//em9280_spi(use hardware SPI1)
#define EM9280_AD_MINOR			14					//em9280_ad
#define EM9280_PWM4_MINOR			15					//em9280_pwm4(->GPIOX9 of EM9460 V2.0 only!)
#define EM9280_IRQ3_MINOR			16					//em9160/em9460
#define EM9280_IRQ4_MINOR			17					//em9160/em9460
#define EM9280_IRQ5_MINOR			18					//em9160/em9460


#define EM9280_BOARD_TYPE_EM9280	0
#define EM9280_BOARD_TYPE_EM9281	1
#define EM9280_BOARD_TYPE_EM9283	3
#define EM9280_BOARD_TYPE_EM9287	7



/*
typedef	unsigned int	KEY_CODE;

struct  can_msg
{
	unsigned int 	id;			//identifier (11 or 29 bits)
	int				type;		//standard(0) or extended frame(1)
	int				rtr;		//remote transmission request(1 when true)
	int				len;		//data length 0..8
	unsigned char	data[8];	//data bytes
	struct timeval	timestamp;  // timestamp for received messages in the format of [seconds, microseconds] since Epoch (january 1. 1970).
};

struct accept_filter
{
	unsigned int  accept_code;
	unsigned int  accept_mask;
	unsigned char filter_mode;
};

typedef	unsigned int	DWORD;
	
struct drv_statistics
{
	DWORD NumISTEvents;
	DWORD NumRxDataFrameInt;
	DWORD NumRxDataFramePutRing;
	DWORD NumTxDataFramePutRing;
	DWORD NumTxDataFrameInt;
	DWORD NumTxSuccessful;
	DWORD NumTxFailed;
	DWORD NumErrorWarningInt;
	DWORD NumErrorWarningLevel;
	DWORD NumErrorPassive;
	DWORD NumErrorBusOff;
	DWORD NumSwitchedToNormalOperation;
	DWORD NumOverrunInt;
	DWORD NumBusErrorInt;
	DWORD NumArbitrationLostInt;
	DWORD MaxNumMsgsInRing;
	DWORD RXErrorCounter;
	DWORD TXErrorCounter;
	DWORD Status;
};

//--------------------definition for CAN driver------------------------
typedef enum
{
	CAN_BAUDRATE_10K = 0,
	CAN_BAUDRATE_20K,		// = 1
	CAN_BAUDRATE_50K,		// = 2
	CAN_BAUDRATE_100K,		// = 3
	CAN_BAUDRATE_125K,		// = 4
	CAN_BAUDRATE_250K,		// = 5
	CAN_BAUDRATE_500K,		// = 6
	CAN_BAUDRATE_1000K,		// = 7
	CAN_BAUDRATE_60K,		// = 8
	CAN_BAUDRATE_SIZE
} CAN_BAUDRATE;

#define CANCONTROLLER_NORMAL						0
#define CANCONTROLLER_WARNING_LIMIT_REACHED		(1<<0)		// 1
#define CANCONTROLLER_ERROR_PASSIVE				(1<<1)		// 2
#define CANCONTROLLER_BUS_OFF						(1<<2)		// 4
#define CANCONTROLLER_OVERRUN						(1<<3)		// 8
#define CANCONTROLLER_BUS_ERROR					(1<<4)		// 16
#define CANCONTROLLER_ABITRATION_LOST				(1<<5)		// 32
#define RING_BUFFER_FULL								(1<<6)		// 64

*/


/*
* Emlinix JUN-2-2010: double input parameters can be used in more than one driver
*/
struct double_pars
{
	unsigned int	par1;
	unsigned int	par2;
};


/*
* Emlinix JUN-3-2010: struct for lcd driver
*/
struct lcd_line
{
	unsigned int	type;			// = 0: point; = 1: line; = 2: bar
	unsigned int	x0;
	unsigned int	y0;
	unsigned int	x1;
	unsigned int	y1;
	unsigned int 	color;			// = 0: write "0";  = 1: write "1", = 2: xor operation
};

struct lcd_block
{
	unsigned int 	x0;
	unsigned int	y0;
	unsigned int	xsize;			// = 1 - 8; left alignment
	unsigned int	ysize;			// = 1 - 16;
	unsigned char	data[16];		// block data to be copied
};

struct spi_config
{
	unsigned int	uCPOL;			// clock polarity = 0: ; = 1: 
	unsigned int	uCPHA;			// clock phase = 0: ; = 1:
	unsigned int	data_bit;		// = 8, 9, .. 16
	unsigned int	baudrate;		// < 100000000bps => 100Mbps
};


/*
* Emlinix FEB-15-2010: ioctl cmd code definitions:
*/
#define EM9280_MAGIC								EM9280_DEV_MAJOR

#define EM9280_SYSINFO_IOCTL_GET_DBGSL			_IOR(EM9280_MAGIC,  0x00, unsigned int)
#define EM9280_SYSINFO_IOCTL_GET_BOARDTYPE		_IOR(EM9280_MAGIC,  0x01, unsigned int)
#define EM9280_SYSINFO_IOCTL_GET_VID				_IOR(EM9280_MAGIC,  0x02, unsigned int)
#define EM9280_SYSINFO_IOCTL_GET_UID				_IOR(EM9280_MAGIC,  0x03, unsigned int)
#define EM9280_SYSINFO_IOCTL_GET_BOOTSTATUS		_IOR(EM9280_MAGIC,  0x04, unsigned int)

#define EM9280_GPIO_IOCTL_OUT_ENABLE				_IOW(EM9280_MAGIC,  0x60, unsigned int)
#define EM9280_GPIO_IOCTL_OUT_DISABLE			_IOW(EM9280_MAGIC,  0x61, unsigned int)
#define EM9280_GPIO_IOCTL_OUT_SET					_IOW(EM9280_MAGIC,  0x62, unsigned int)
#define EM9280_GPIO_IOCTL_OUT_CLEAR				_IOW(EM9280_MAGIC,  0283, unsigned int)
#define EM9280_GPIO_IOCTL_OPEN_DRAIN				_IOW(EM9280_MAGIC,  0284, unsigned int)
#define EM9280_GPIO_IOCTL_PIN_STATE				_IOR(EM9280_MAGIC,  0285, unsigned int)

#define EM9280_IRQ_IOCTL_GET_COUNT				_IOR(EM9280_MAGIC,  0x80, unsigned int)
#define EM9280_IOCTL_SET_IDLE						_IOR(EM9280_MAGIC,  0x81, unsigned int)
#define EM9280_IOCTL_GET_IDLESTATE				_IOR(EM9280_MAGIC,  0x82, unsigned int)

#define EM9280_CAN_IOCTL_START_CHIP				_IO(EM9280_MAGIC,   0xa0)
#define EM9280_CAN_IOCTL_STOP_CHIP				_IO(EM9280_MAGIC,   0xa1)
#define EM9280_CAN_IOCTL_SET_BAUD				_IOW(EM9280_MAGIC,  0xa2, unsigned int)
#define EM9280_CAN_IOCTL_SET_FILTER				_IOW(EM9280_MAGIC,  0xa3, struct accept_filter)
#define EM9280_CAN_IOCTL_GET_ERRORCODE 			_IOR(EM9280_MAGIC,  0xa4, unsigned int)
#define EM9280_CAN_IOCTL_READ_REG				_IOWR(EM9280_MAGIC, 0xa5, struct isa_io)
#define EM9280_CAN_IOCTL_GET_STATISTICS			_IOR(EM9280_MAGIC,  0xa6, struct drv_statistics)
#define EM9280_CAN_IOCTL_SET_SELFTEST			_IOW(EM9280_MAGIC,  0xa7, unsigned int)
#define EM9280_CAN_IOCTL_GET_ECCREG 				_IOR(EM9280_MAGIC,  0xa8, unsigned int)
//#define EM9280_CAN_IOCTL_CLEAR_RXBUF				_IO(EM9280_MAGIC,   0xa9)
//#define EM9280_CAN_IOCTL_CLEAR_TXBUF				_IO(EM9280_MAGIC,   0xaa)
//#define EM9280_CAN_IOCTL_LAST_TIMESTAMP			_IOR(EM9280_MAGIC,  0xab, struct timeval)

#define EM9280_PWM_IOCTL_START					_IOW(EM9280_MAGIC,  0xb0, struct double_pars)
#define EM9280_PWM_IOCTL_STOP						_IO(EM9280_MAGIC,   0xb1)
#define EM9280_COUNT_IOCTL_START					_IOW(EM9280_MAGIC,  0xb2, struct double_pars)
#define EM9280_COUNT_IOCTL_STOP					_IO(EM9280_MAGIC,   0xb3)

#define EM9280_I2C_IOCTL_CONFIG					_IOW(EM9280_MAGIC,  0xc0, struct i2c_config)
#define EM9280_I2C_IOCTL_WRITE					_IOW(EM9280_MAGIC,  0xc1, struct i2c_io)
#define EM9280_I2C_IOCTL_READ						_IOWR(EM9280_MAGIC, 0xc2, struct i2c_io)

#define EM9280_LCD_IOCTL_TYPE						_IOW(EM9280_MAGIC,  0xd0, unsigned int)
#define EM9280_LCD_IOCTL_LINE						_IOW(EM9280_MAGIC,  0xd1, struct lcd_line)
#define EM9280_LCD_IOCTL_BLOCK					_IOW(EM9280_MAGIC,  0xd2, struct lcd_block)
#define EM9280_LCD_IOCTL_CLEAR					_IO(EM9280_MAGIC,   0xd3)
#define EM9280_LCD_IOCTL_UPDATE					_IO(EM9280_MAGIC,   0xd4)

#define EM9280_SPI_IOCTL_GET_CONFIG				_IOR(EM9280_MAGIC,  0xe0, struct spi_config)
#define EM9280_SPI_IOCTL_SET_CONFIG				_IOW(EM9280_MAGIC,  0xe1, struct spi_config)

#define	GPIO0		(1 <<  0)
#define	GPIO1		(1 <<  1)
#define	GPIO2		(1 <<  2)
#define	GPIO3		(1 <<  3)
#define	GPIO4		(1 <<  4)
#define	GPIO5		(1 <<  5)
#define	GPIO6		(1 <<  6)
#define	GPIO7		(1 <<  7)
#define	GPIO8		(1 <<  8)
#define	GPIO9		(1 <<  9)
#define	GPIO10		(1 << 10)
#define	GPIO11		(1 << 11)
#define	GPIO12		(1 << 12)
#define	GPIO13		(1 << 13)
#define	GPIO14		(1 << 14)
#define	GPIO15		(1 << 15)
#define	GPIO16		(1 << 16)
#define	GPIO17		(1 << 17)
#define	GPIO18		(1 << 18)
#define	GPIO19		(1 << 19)
#define	GPIO20		(1 << 20)
#define	GPIO21		(1 << 21)
#define	GPIO22		(1 << 22)
#define	GPIO23		(1 << 23)
#define	GPIO24		(1 << 24)
#define	GPIO25		(1 << 25)
#define	GPIO26		(1 << 26)
#define	GPIO27		(1 << 27)
#define	GPIO28		(1 << 28)
#define	GPIO29		(1 << 29)
#define	GPIO30		(1 << 30)
#define	GPIO31		(1 << 31)
#define	GPIOX_FLAG	(1 << 31)

#define	EM9280_GPIO_OUTPUT_ENABLE		0
#define	EM9280_GPIO_OUTPUT_DISABLE		1
#define	EM9280_GPIO_OUTPUT_SET			2
#define	EM9280_GPIO_OUTPUT_CLEAR		3
#define	EM9280_GPIO_INPUT_STATE			5









#endif	//__ASM_ARCH_EM9280_DRIVERS_H









