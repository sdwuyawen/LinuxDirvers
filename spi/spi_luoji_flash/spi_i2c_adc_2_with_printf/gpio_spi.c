#include "s3c2416.h"
#include "gpio_spi.h"

void delay_ms(unsigned int ms)
{
	volatile unsigned long i,j;
	for(i = 0; i < ms; i++)
	{
		for(j = 0; j < 0x1000; j++)
		{
		
		}
	}
}

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

	GPFCON &= ~(3 << (7 * 2));
#if 0
	/* GPF7 output */
	GPFCON |= (1 << (7 * 2));
#endif
	/* GPF7 input */
	GPFCON |= (0 << (7 * 2));
	GPFDAT &= ~(1 << 7);
	GPFUDP &= ~(3 << (7 * 2));
	GPFUDP |= (2 << (7 * 2));

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

static unsigned char spi_get_miso(void)
{
	if(GPFDAT & (1 << 7))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

unsigned char  SPIvSendByte(unsigned char value)
{
	unsigned char i = 0;
	unsigned char byte_read = 0;

	delay_ms(1);
	
	for(i = 8; i > 0; i--)
	{
		delay_ms(1);
		spi_clk_clr();
		delay_ms(1);
		
		if(value & 0x80)	
			spi_do_set(); //输出数据
		else 
			spi_do_clr();

		byte_read <<= 1;
		byte_read |= spi_get_miso();

		delay_ms(1);
 
		spi_clk_set();
		delay_ms(1);
		
		spi_clk_clr();
		delay_ms(1);
		
		value <<= 1; 
	}

	spi_clk_clr();

	delay_ms(1);

	return byte_read;
}


unsigned char SPIvRecvByte(void)
{
	unsigned char i = 0;
	unsigned char value = 0;

	delay_ms(1);
	spi_do_clr();
	delay_ms(1);
	for(i = 8; i > 0; i--)
	{
		delay_ms(1);
		spi_clk_clr();
		delay_ms(1);

		value <<= 1; 

		spi_clk_set();
		delay_ms(1);

		value |= spi_get_miso();
		delay_ms(1);

		spi_clk_clr();
		delay_ms(1);
	}

	return value;
}