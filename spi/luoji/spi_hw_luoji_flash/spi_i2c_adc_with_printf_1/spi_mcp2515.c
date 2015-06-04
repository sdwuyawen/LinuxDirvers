#include "s3c2416.h"
#include "s3c2416_spi.h"
#include "spi_mcp2515.h"
#include "stdio.h"

extern void delay_ms(unsigned int ms);

void spi_mcp2515_init(void)
{
	/* 初始化硬件SPI */
	spi_hw_init();

	/* GPH9 RST */
	GPHCON &= ~(3 << (9 * 2));
	GPHCON |= (1 << (9 * 2));
	GPHDAT |= (1 << 9);

	delay_ms(200);
	spi_mcp2515_rst_clr();
	delay_ms(200);
	spi_mcp2515_rst_set();
	delay_ms(200);
}

void spi_mcp2515_rst_clr(void)
{
	GPHDAT &= ~(1 << 9);
}

void spi_mcp2515_rst_set(void)
{
	GPHDAT |= (1 << 9);
}

unsigned char spi_mcp2515_read_reg(unsigned addr)
{
	unsigned char byte;
	
	spi_hw_cs_clr();
#if 1
//	delay_ms(1);
	spi_hw_send_byte(0x03);
//	delay_ms(1);
	spi_hw_send_byte(addr);
//	delay_ms(1);

	byte = spi_hw_send_byte(0xFF);
//	delay_ms(1);
	
	spi_hw_cs_set();							/* 禁能片选 */
//	delay_ms(1);
#endif
//	delay_ms(1);
//	spi_hw_send_byte(0xAA);
//	spi_hw_send_byte(0x55);
//	delay_ms(1);

	return byte;
}
