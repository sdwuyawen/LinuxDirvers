#include "s3c2416.h"
#include "gpio_spi.h"

#define CMD_AAI       		0xAD  		/* AAI 连续编程指令(FOR SST25VF016B) */
#define CMD_DISWR	  	0x04		/* 禁止写, 退出AAI状态 */
#define CMD_EWRSR	  	0x50		/* 允许写状态寄存器的命令 */
#define CMD_WRSR      	0x01  		/* 写状态寄存器命令 */
#define CMD_WRDISEN	0x04		/* 禁止写使能命令 */
#define CMD_WREN      	0x06		/* 写使能命令 */
#define CMD_READ      	0x03  		/* 读数据区命令 */
#define CMD_RDSR1      	0x05		/* 读状态寄存器1命令 */
#define CMD_RDSR2      	0x35		/* 读状态寄存器2命令 */
#define CMD_RDID      	0x9F		/* 读器件ID命令 */
#define CMD_SE        		0x20		/* 擦除扇区命令 */
#define CMD_BE        		0xC7		/* 批量擦除命令 */
#define CMD_PAGE_PROGRAM	0x02	/* 页写入 */
#define DUMMY_BYTE    	0xA5		/* 哑命令，可以为任意值，用于读操作 */

#define WIP_FLAG      	0x01		/* 状态寄存器中的正在编程标志（WIP) */

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

void spi_flash_read_id(unsigned int *pMID, unsigned int *pDID)
{
	spi_flash_cs_clr();

	SPIvSendByte(CMD_RDID);
//	spi_flash_send_address(0x00);

	*pMID = SPIvSendByte(0x00);

	*pDID = SPIvSendByte(0x00);
	
	*pDID = (*pDID << 8) | SPIvSendByte(0x00);
	
	spi_flash_cs_set();
}

/*
*********************************************************************************************************
*	函 数 名: spi_flash_write_enable
*	功能说明: 向器件发送写使能命令
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void spi_flash_write_enable(void)
{
	spi_flash_cs_clr();									/* 使能片选 */
	SPIvSendByte(CMD_WREN);						/* 发送命令 */
	spi_flash_cs_set();								/* 禁能片选 */
}

/*
*********************************************************************************************************
*	函 数 名: spi_flash_write_disable
*	功能说明: 向器件发送禁止写使能命令
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void spi_flash_write_disable(void)
{
	spi_flash_cs_clr();									/* 使能片选 */
	SPIvSendByte(CMD_WRDISEN);						/* 发送命令 */
	spi_flash_cs_set();								/* 禁能片选 */
}


static unsigned char spi_flash_read_status_reg1(void)
{
	unsigned char byte;
	
	spi_flash_cs_clr();

	SPIvSendByte(CMD_RDSR1);

	byte = SPIvSendByte(0x00);
	
	spi_flash_cs_set();							/* 禁能片选 */

	return byte;
}

static unsigned char spi_flash_read_status_reg2(void)
{
	unsigned char byte;
	
	spi_flash_cs_clr();

	SPIvSendByte(CMD_RDSR2);

	byte = SPIvSendByte(0x00);
	
	spi_flash_cs_set();							/* 禁能片选 */

	return byte;
}

static void spi_flash_wait_for_busy(void)
{
	while((spi_flash_read_status_reg1() & 0x01) != 0);
}
static void spi_flash_write_status_reg(unsigned char reg1, unsigned char reg2)
{
	/* 写入Write enable指令，允许写入status reg */
	spi_flash_write_enable();
	
	spi_flash_cs_clr();

	SPIvSendByte(CMD_WRSR);

	SPIvSendByte(reg1);
	SPIvSendByte(reg2);
	
	spi_flash_cs_set();							/* 禁能片选 */

	spi_flash_wait_for_busy();
}

static void spi_flash_status_reg_protect_off(void)
{
	unsigned char reg1, reg2;

	reg1 = spi_flash_read_status_reg1();
	reg2 = spi_flash_read_status_reg2();

	reg1 &= ~(1 << 7);
	reg2 &= ~(1 << 0);

	spi_flash_write_status_reg(reg1, reg2);
}

static void spi_flash_chip_protect_off(void)
{
	/* cmp = 0, bp2=bp1=bp0=0 */
	
	unsigned char reg1, reg2;

	reg1 = spi_flash_read_status_reg1();
	reg2 = spi_flash_read_status_reg2();

	reg1 &= ~((1 << 2) | (1 << 3) | (1 << 4));
	reg2 &= ~(1 << 6);

	spi_flash_write_status_reg(reg1, reg2);
}

/* 擦除4kb */
void spi_flash_erase_sector(unsigned int addr)
{
	spi_flash_write_enable();

	spi_flash_cs_clr();

	SPIvSendByte(CMD_SE);

	spi_flash_send_address(addr);
	
	spi_flash_cs_set();							/* 禁能片选 */

	spi_flash_wait_for_busy();
}

void spi_flash_page_program(unsigned int addr, unsigned char *buf, unsigned int len)
{
	int i;
	
	spi_flash_write_enable();

	spi_flash_cs_clr();

	SPIvSendByte(CMD_PAGE_PROGRAM);

	spi_flash_send_address(addr);

	for(i = 0; i < len; i++)
	{
		SPIvSendByte(buf[i]);
	}
	
	spi_flash_cs_set();							/* 禁能片选 */

	spi_flash_wait_for_busy();
}

void spi_flash_read(unsigned int addr, unsigned char *buf, unsigned int len)
{
	int i;
	
	spi_flash_cs_clr();

	SPIvSendByte(CMD_READ);

	spi_flash_send_address(addr);

	for(i = 0; i < len; i++)
	{
		buf[i] = SPIvSendByte(0x00);
	}
	
	spi_flash_cs_set();							/* 禁能片选 */	
}

void spi_flash_init(void)
{
	spi_init();
	
	/* deselect flash */
	spi_flash_cs_set();

	/* 去掉status reg的WP引脚保护 */
	spi_flash_status_reg_protect_off();

	/* 去掉存储空间写保护 */
	spi_flash_chip_protect_off();
}