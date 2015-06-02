#include "s3c2416.h"
#include "gpio_spi.h"

#if 0
static void delay_ms(unsigned int ms)
{
	volatile unsigned long i,j;
	for(i = 0; i < ms; i++)
	{
		for(j = 0; j < 0x1000; j++)
		{
		
		}
	}
}
#endif


static void spi_flash_cs_clr(void)
{
	GPHDAT &= ~(1 << 10);
}

static void spi_flash_cs_set(void)
{
	GPHDAT |= (1 << 10);
}

static void spi_flash_send_address(unsigned int addr)
{
	SPIvSendByte((addr >> 16) & 0xFF);
	SPIvSendByte((addr >> 8) & 0xFF);
	SPIvSendByte((addr >> 0) & 0xFF);
}


void spi_flash_init(void)
{
	spi_init();

	/* deselect flash */
	spi_flash_cs_set();
}

void spi_flash_read_id(unsigned int *pMID, unsigned int *pDID)
{
	delay_ms(1);

	spi_flash_cs_clr();

	delay_ms(1);

	SPIvSendByte(0x9F);
//	spi_flash_send_address(0x00);

	delay_ms(1);

	*pMID = SPIvRecvByte();

	delay_ms(1);

	*pDID = SPIvRecvByte();
	
	*pDID = (*pDID << 8) | SPIvRecvByte();

	delay_ms(1);
	
	spi_flash_cs_set();

	delay_ms(1);
}
