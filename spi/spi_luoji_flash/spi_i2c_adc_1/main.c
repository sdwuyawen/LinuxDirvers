///////////////////////////////////////////////////////////////
// 本程序只供学习使用，未经本公司许可，不得用于其它任何商业用途
// 适用开发板型号:Tiny2416、Mini2451、Tiny2451
// 技术论坛:www.arm9home.net
// 修改日期:2013/7/1
// 版权所有，盗版必究。
// Copyright(C) 广州友善之臂计算机科技有限公司
// All rights reserved							
///////////////////////////////////////////////////////////////
#include "s3c2416.h"
#include "uart.h"
#include "lcd.h"
#include "i2c.h"
#include "spi_tft.h"
#include "spi_flash.h"
#include "string.h"

unsigned char at24cxx_read(unsigned char address);
void at24cxx_write(unsigned char address, unsigned char data);


int memory_test(void);

void delay(void)
{
	volatile unsigned long i,j;
	for(i = 0; i < 0x100; i++)
	{
		for(j = 0; j < 0x1000; j++)
		{
		
		}
	}
}

int main()
{
	char c;
//	short color;
//	int address;
	int data;
	char buf[100];
	unsigned int val;

	unsigned int MID;
	unsigned int DID;
		
	uart_init();
	i2c_init();

#if 0
	spi_tft_init();
	
	lcd_clear(CL_RED);
	lcd_clear(CL_GREEN);
	lcd_clear(CL_BLUE);
#endif

	spi_flash_init();
	spi_flash_read_id(&MID, &DID);

	putstring("read spi flash ID = \r\n");

	putinthex(MID);
	putinthex(DID);

	spi_flash_read(8 * 1024, (unsigned char *)&val, sizeof(val));
	val++;
	spi_flash_erase_sector(8 * 1024);
	spi_flash_page_program(8 * 1024, (unsigned char *)&val, sizeof(val));
	putstring("val = ");
	putinthex(val);

	spi_flash_erase_sector(4 * 1024);
	spi_flash_page_program(4 * 1024, "Compass", 8);
	spi_flash_read(4 * 1024, buf, 8);

	putstring("buf = ");
	putstring(buf);
	putstring("\r\n");
	
	putchar('O');
	putchar('K');
	putchar('\r');
	putchar('\n');
	putstring("mmu enabled\r\n");
	putstring("12.nand_boot\r\n");

//	memory_test();

	// 初始化LCD控制器
	lcd_init();

	// 清屏
	lcd_clear_screen(((0xFF >> 3) << 11)|((0x00 >> 2)  << 5)|((0x00 >> 3)  << 0));		// 绿色   ((0xFF >> 3) << 11)|((0x00 >> 2)  << 5)|((0x00 >> 3)  << 0)
	lcd_clear_screen(((0x00 >> 3) << 11)|((0xFF >> 2)  << 5)|((0x00 >> 3)  << 0));		//红色
	lcd_clear_screen(((0x00 >> 3) << 11)|((0x00 >> 2)  << 5)|((0xFF >> 3)  << 0));		//蓝色
	lcd_draw_hline(10, 20 ,100 , 0x000000);
	lcd_draw_vline(30, 5, 200, 0xff0000);
//	lcd_draw_circle();

	//测试读写
	at24cxx_write(0, 0x12);
	data = at24cxx_read(0);
	putinthex(data);
	
	while (1)
	{
		c = getchar();
		putchar(c);

	}
	return 0;
}



int memory_test(void)
{
	
	volatile unsigned int *addr = (unsigned int *)0x30008000;
	unsigned int data = 0x00000000;
	/*
	unsigned int i;

	for(i = 0; i < 1024; i++)
	{
		addr[i] = i;
	}

	for(i = 0; i < 1024; i++)
	{
		if(addr[i] != i)
		{
			break;
		}
	}

	if(i == 1024)
	{
		putstring("Mem Test OK\n");
	}
	else
	{
		putstring("Mem Test Error");
		
		putchar(i / 1000 + '0');
		putchar((i % 1000) / 100+ '0');
		putchar((i % 100) / 10+ '0');
		putchar((i % 10) / 1+ '0');
	}*/

	unsigned int i;
	
	*addr = 0xAAAAAAAA;
	if(*addr == 0xAAAAAAAA)
	{
		putstring("Mem Test OK\r\n");
	}
	else
	{
		putstring("Mem Test Error\r\n");
	}

	addr = (unsigned int *)0x30008004;
	
	*addr= 0x12345678;
	data = *addr;
	
	if(data == 0x12345678)
	{
		putstring("Mem Test OK\r\n");
	}
	else
	{
		putstring("Mem Test Error\r\n");
	}
	putinthex(data);
	putstring("\r\n");
	putstring("\r\n");

	

	for(i = 0; i < 50; i++)
	{
		addr[i] = i;
	}
	/*
	*((unsigned int *)0x30000000) = 0xABCD1234;
	*((unsigned int *)0x30000004) = 0xABCD1235;*/

	for(i = 0; i < 50; i++)
	{
		data = addr[i];
		putinthex(data);
		/*if(addr[i] != i)
		{
			break;
		}*/
	}
	
	if(i == 50)
	{
		putstring("Mem Test ALL OK\r\n");
	}
	else
	{
		putstring("Mem Test Error\r\n");
		putinthex(i);
		putinthex(addr[i]);
	}

//	while(1);

	return 0;

}
