#include "s3c2416.h"
#include "s3c2416_spi.h"
#include "stdio.h"


/* 硬件SPI引脚初始化 */
void spi_hw_gpio_init(void)
{
	/* GPE11	MISO
	 * GPE12	MOSI
	 * GPE13	CLK
	 * GPL13	SS
	 */

	/* GPE11 MISO */
	GPECON &= ~(3 << (11 * 2));
	GPECON |= (2 << (11 * 2));
	GPEDAT &= ~(1 << 11);

	/* GPE12 MOSI */
	GPECON &= ~(3 << (12 * 2));
	GPECON |= (2 << (12 * 2));
	GPEDAT &= ~(1 << 12);

	/* GPE13 CLK */
	GPECON &= ~(3 << (13 * 2));
	GPECON |= (2 << (13 * 2));
	GPEDAT &= ~(1 << 13);

	/* GPL13 SS */
	GPLCON &= ~(3 << (13 * 2));
	GPLCON |= (2 << (13 * 2));
//	GPLDAT &= ~(1 << 13);
	GPLDAT |= (1 << 13);


}


static void spi_print_reg(void)
{
	printf("CH_CFG 	= 0x%08x\r\n",S3C_CH_CFG);
	printf("CLK_CFG = 0x%08x\r\n", S3C_CLK_CFG);
	printf("MODE_CFG = 0x%08x\r\n", S3C_MODE_CFG);
	printf("SLAVE_CFG = 0x%08x\r\n", S3C_SLAVE_SEL);
	printf("INT_EN 	= 0x%08x\r\n", S3C_SPI_INT_EN);
	printf("SPI_STATUS = 0x%08x\r\n", S3C_SPI_STATUS);
	printf("PACKET_CNT = 0x%08x\r\n", S3C_PACKET_CNT);
	printf("PEND_CLR = 0x%08x\r\n", S3C_PENDING_CLR);
}

void spi_reg_init(void)
{
	unsigned int spi_chcfg;
	unsigned int spi_clkcfg;
	unsigned int spi_modecfg;
	unsigned int spi_inten;
	unsigned int spi_packet;
	unsigned int spi_slavecfg;
	unsigned char prescaler = 30;

	
	/* Enable HS-SPI_0 (EPLL) clock, Enable SPICLK0 (MPLL) */
	SCLKCON |= (1 << 14) | (1 << 19);

	/* Enable PCLK into the SPI_HS0 */
	PCLKCON |= (1 << 6);

	/* HSSPI_EN2 = 1 */
	MISCCR |= (1 << 31);

	LOCKCON1 = 0x800;

	/* ESYSCLK uses EPLL Output */
	CLKSRC |= (1 <<6);

	/* 96MHz? */
	EPLLCON = (48<<16)|(3<<8)|(1<<0);
	/* EPLL On */
	EPLLCON &= ~(1 << 24);
	/* Epll Ratio is 1 */
	CLKDIV1 &= ~(3 << 24);
	CLKDIV1 |= (0 << 24);

	MISCCR &= ~(7<<8);
	MISCCR |= (1<<8);

	/* Epll, prescaler = 1					*/
	/* clock =  ( clock source / (2 * ( prescaler + 1)))	*/
	S3C_CLK_CFG = 0x501;					//Use EPLL Clock and Clock On Prescaler 1


	/* 1. Set transfer type (CPOL & CPHA set) */
	spi_chcfg = SPI_CH_RISING | SPI_CH_FORMAT_A;
	spi_chcfg |= SPI_CH_MASTER;

	S3C_CH_CFG = spi_chcfg;


	/* 2. Set clock configuration register */
	spi_clkcfg = SPI_ENCLK_ENABLE;
	spi_clkcfg |= SPI_CLKSEL_ECLK;

	S3C_CLK_CFG = spi_clkcfg;

	spi_clkcfg = S3C_CLK_CFG;

	/* SPI clockout = clock source / (2 * (prescaler +1)) */
	spi_clkcfg |= prescaler;
	S3C_CLK_CFG = spi_clkcfg;


	/* 3. Set SPI MODE configuration register */
	spi_modecfg = SPI_MODE_CH_TSZ_BYTE;
	spi_modecfg |= SPI_MODE_TXDMA_OFF| SPI_MODE_SINGLE| SPI_MODE_RXDMA_OFF;

//	if (msg->wbuf)
//		spi_modecfg |= ( 0x3f << 5); /* Tx FIFO trigger level in INT mode */
//	if (msg->rbuf)
//		spi_modecfg |= ( 0x3f << 11); /* Rx FIFO trigger level in INT mode */

	spi_modecfg |= ( 0x3ff << 19);

	S3C_MODE_CFG = spi_modecfg;


	/* 4. Set SPI INT_EN register */
	spi_inten = 0;
//	if (msg->wbuf)
//		spi_inten = SPI_INT_TX_FIFORDY_EN|SPI_INT_TX_UNDERRUN_EN|SPI_INT_TX_OVERRUN_EN;
//	if (msg->rbuf){
//		spi_inten = SPI_INT_RX_FIFORDY_EN|SPI_INT_RX_UNDERRUN_EN|SPI_INT_RX_OVERRUN_EN|SPI_INT_TRAILING_EN;
//	}
	S3C_SPI_INT_EN = spi_inten;

	S3C_PENDING_CLR = 0x1f;


	/* 5. Set Packet Count configuration register */
	spi_packet = SPI_PACKET_CNT_EN;
	spi_packet |= 0xffff;
	S3C_PACKET_CNT = spi_packet;


	/* 6. Set Tx or Rx Channel on */
	spi_chcfg = S3C_CH_CFG;

	spi_chcfg |= SPI_CH_TXCH_OFF | SPI_CH_RXCH_OFF;

//	if (msg->wbuf)
		spi_chcfg |= SPI_CH_TXCH_ON;
//	if (msg->rbuf)
		spi_chcfg |= SPI_CH_RXCH_ON;

	S3C_CH_CFG = spi_chcfg;


	/* 7. Set nSS low to start Tx or Rx operation ?????? */
	spi_slavecfg = S3C_SLAVE_SEL;
	spi_slavecfg |= SPI_SLAVE_SIG_INACT;
	spi_slavecfg &= ~SPI_SLAVE_AUTO;
	spi_slavecfg |= (0x3f << 4);

	S3C_SLAVE_SEL = spi_slavecfg;
}

/* 硬件SPI */
void spi_hw_init(void)
{
	spi_hw_gpio_init();
	spi_reg_init();
	spi_print_reg();
}

void spi_hw_cs_clr(void)
{
	unsigned int spi_slavecfg;
	
	spi_slavecfg = S3C_SLAVE_SEL;
	spi_slavecfg &= ~SPI_SLAVE_SIG_INACT;
	spi_slavecfg &= ~SPI_SLAVE_AUTO;

	S3C_SLAVE_SEL = spi_slavecfg;
}

void spi_hw_cs_set(void)
{
	unsigned int spi_slavecfg;
	
	spi_slavecfg = S3C_SLAVE_SEL;
	spi_slavecfg |= SPI_SLAVE_SIG_INACT;
	spi_slavecfg &= ~SPI_SLAVE_AUTO;

	S3C_SLAVE_SEL = spi_slavecfg;
}

unsigned char  spi_hw_send_byte(unsigned char value)
{
	S3C_SPI_TX_DATA = value;

	while((S3C_SPI_STATUS & (1 << 21)) == 0)
	{
//		spi_print_reg();
	}
//	printf("\r\n");

//	printf("end of %s\r\n", __FUNCTION__);
	
//	spi_print_reg();
//	printf("\r\n");

	delay_ms(100);
	return S3C_SPI_RX_DATA;

	return 0;
}

#if 0
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

#endif
