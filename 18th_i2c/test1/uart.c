///////////////////////////////////////////////////////////////
// 本程序只供学习使用，未经本公司许可，不得用于其它任何商业用途
// 适用开发板型号:Tiny2416、Mini2451、Tiny2451
// 技术论坛:www.arm9home.net
// 修改日期:2013/7/1
// 版权所有，盗版必究。
// Copyright(C) 广州友善之臂计算机科技有限公司
// All rights reserved							
///////////////////////////////////////////////////////////////

// 功能:初始化串口
#include "uart.h" 
#define ULCON0   		( *((volatile unsigned long *)0x50000000) )
#define UCON0    		( *((volatile unsigned long *)0x50000004) )
#define UFCON0   	 	( *((volatile unsigned long *)0x50000008) )
#define UMCON0    		( *((volatile unsigned long *)0x5000000C) )
#define UTRSTAT0  		( *((volatile unsigned long *)0x50000010) )
#define UFSTAT0 		( *((volatile unsigned long *)0x50000018) )
#define UTXH0      		( *((volatile unsigned char *)0x50000020) )
#define URXH0      		( *((volatile unsigned char *)0x50000024) ) 
#define UBRDIV0    		( *((volatile unsigned short *)0x50000028) )
#define UDIVSLOT0  		( *((volatile unsigned short *)0x5000002C) )
#define GPHCON     		( *((volatile unsigned long *)0x56000070 ) )

void uart_init(void)
{
	// 配置引脚  
	GPHCON = (GPHCON & ~0xffff ) | 0xaaaa;
		
	// 设置数据格式等  
	ULCON0 = 0x3;  					// 数据位:8, 无校验, 停止位: 1, 8n1 
	UCON0  = 0x5;  					// 时钟：PCLK，禁止中断，使能UART发送、接收 
	UFCON0 = 0x01; 					// FIFO ENABLE
	UMCON0 = 0;						// 无流控
	
	// 设置波特率  
	// DIV_VAL = (PCLK / (bps x 16 ) ) - 1 = (66500000/(115200x16))-1 = 35.08
	// DIV_VAL = 35.08 = UBRDIVn + (num of 1’s in UDIVSLOTn)/16 
	UBRDIV0   = 35;
	UDIVSLOT0 = 0x1;
}

// 接收一个字符  
char getchar(void)
{
	while ((UFSTAT0 & 0x7f) == 0);  // 如果RX FIFO空，等待 
	return URXH0;                   // 取数据 
}

// 发送一个字符  
void putchar(char c)
{
	while (UFSTAT0 & (1<<14)); 		// 如果TX FIFO满，等待 
	UTXH0 = c;                      // 写数据 
}


// 发送一个字符串
void putstring(char *string)
{
	while((*string) != 0x00)
	{
		putchar(*string);
		string ++;
	}
}

void putinthex(unsigned int data)
{
	/*
	putchar((data /0x10000000)+ '0');
	putchar((data %0x10000000) / 0x01000000+ '0');
	putchar((data %0x01000000) / 0x00100000+ '0');
	putchar((data %0x00100000) / 0x00010000+ '0');
	putchar((data %0x00010000) / 0x00001000+ '0');
	putchar((data %0x00001000) / 0x00000100+ '0');
	putchar((data %0x00000100) / 0x00000010+ '0');
	putchar((data %0x00000010) / 0x00000001+ '0');
	putchar('\r');
	putchar('\n');
	*/

	unsigned int i;
	unsigned char data_char;

	for(i = 0; i < 8; i++)
	{
		data_char = (data >> (4 *(7 - i))) & 0x0F;
		if(data_char > 9)
		{
			putchar(data_char - 0x0A + 'A');
		}
		else
		{
			putchar(data_char + '0');
		}
	}
	putchar('\r');
	putchar('\n');
	
}