#ifndef	_GPIO_SPI_H
#define	_GPIO_SPI_H

void delay_ms(unsigned int ms);
void spi_init(void);
void SPIvSendByte(unsigned char value);
unsigned char SPIvRecvByte(void);

#endif

