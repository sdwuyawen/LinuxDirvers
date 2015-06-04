#include "s3c2416.h"

/* GPIO模拟SPI */
void spi_gpio_init(void)
{
	/* GPF5	CLK
	 * GPF7	RS,	0:命令.	1:数据
	 * GPG3	MOSI
	 * GPG5	RST
	 * GPH10	CS
	 */

	/* GPF5 output */
	GPFCON &= ~(3 << (5 * 2));
	GPFCON |= (1 << (5 * 2));
	GPFDAT &= ~(1 << 5);

	/* GPF7 output */
	GPFCON &= ~(3 << (7 * 2));
	GPFCON |= (1 << (7 * 2));
	GPFDAT &= ~(1 << 7);

	/* GPG3 output */
	GPGCON &= ~(3 << (3 * 2));
	GPGCON |= (1 << (3 * 2));
	GPGDAT &= ~(1 << 3);

	/* GPG5 output */
	GPGCON &= ~(3 << (5 * 2));
	GPGCON |= (1 << (5 * 2));
	GPGDAT &= ~(1 << 5);

	/* GPH10 output */
	GPHCON &= ~(3 << (10 * 2));
	GPHCON |= (1 << (10 * 2));
	GPHDAT &= ~(1 << 10);
}

/* GPIO模拟 */
void spi_init(void)
{
	spi_gpio_init();
}

static void spi_clk_clr(void)
{
	GPFDAT &= ~(1 << 5);
}

static void spi_clk_set(void)
{
	GPFDAT |= (1 << 5);
}

static void spi_do_clr(void)
{
	GPGDAT &= ~(1 << 3);
}

static void spi_do_set(void)
{
	GPGDAT |= (1 << 3);
}


void  SPIvSendByte(unsigned char value)
{
	unsigned char i = 0;
	
	for(i = 8; i > 0; i--)
	{
		spi_clk_clr();
		
		if(value & 0x80)	
			spi_do_set(); //输出数据
		else 
			spi_do_clr();
 
		spi_clk_set();
		
		value <<= 1; 
	}
}