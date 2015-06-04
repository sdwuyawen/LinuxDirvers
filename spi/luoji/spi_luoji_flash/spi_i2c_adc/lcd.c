///////////////////////////////////////////////////////////////
// 本程序只供学习使用，未经本公司许可，不得用于其它任何商业用途
// 适用开发板型号:Tiny2416、Mini2451、Tiny2451
// 技术论坛:www.arm9home.net
// 修改日期:2013/7/1
// 版权所有，盗版必究。
// Copyright(C) 广州友善之臂计算机科技有限公司
// All rights reserved							
///////////////////////////////////////////////////////////////

#include "lcd.h"
#include "stdio.h"

#define VIDCON0			(*(volatile unsigned *)0x4c800000)	
#define VIDCON1			(*(volatile unsigned *)0x4c800004)	
#define VIDTCON0		(*(volatile unsigned *)0x4c800008)	
#define VIDTCON1		(*(volatile unsigned *)0x4c80000C)	
#define VIDTCON2		(*(volatile unsigned *)0x4c800010)	
#define WINCON0			(*(volatile unsigned *)0x4c800014)	
#define WINCON1			(*(volatile unsigned *)0x4c800018)	
#define VIDOSD0A		(*(volatile unsigned *)0x4c800028)	
#define VIDOSD0B		(*(volatile unsigned *)0x4c80002C)
#define VIDOSD1A		(*(volatile unsigned *)0x4c800034)	
#define VIDOSD1B		(*(volatile unsigned *)0x4c800038)	
#define VIDOSD1C		(*(volatile unsigned *)0x4c80003C)	
#define VIDW00ADD0B0	(*(volatile unsigned *)0x4c800064)	
#define VIDW00ADD0B1	(*(volatile unsigned *)0x4c800068)	
#define VIDW01ADD0		(*(volatile unsigned *)0x4c80006C)	
#define VIDW00ADD1B0	(*(volatile unsigned *)0x4c80007C)	
#define VIDW00ADD1B1	(*(volatile unsigned *)0x4c800080)	
#define VIDW01ADD1		(*(volatile unsigned *)0x4c800084)	
#define VIDW00ADD2B0	(*(volatile unsigned *)0x4c800094)	
#define VIDW00ADD2B1	(*(volatile unsigned *)0x4c800098)	
#define VIDW01ADD2		(*(volatile unsigned *)0x4c80009C)	
#define VIDINTCON		(*(volatile unsigned *)0x4c8000AC)	
#define W1KEYCON0		(*(volatile unsigned *)0x4c8000B0)	
#define W1KEYCON1		(*(volatile unsigned *)0x4c8000B4)	
#define W2KEYCON0		(*(volatile unsigned *)0x4c8000B8)	
#define W2KEYCON1		(*(volatile unsigned *)0x4c8000BC)	
#define W3KEYCON0		(*(volatile unsigned *)0x4c8000C0)	
#define W3KEYCON1		(*(volatile unsigned *)0x4c8000C4)	
#define W4KEYCON0		(*(volatile unsigned *)0x4c8000C8)	
#define W4KEYCON1		(*(volatile unsigned *)0x4c8000CC)	
#define WIN0MAP			(*(volatile unsigned *)0x4c8000D0)	
#define WIN1MAP			(*(volatile unsigned *)0x4c8000D4)	
#define WPALCON			(*(volatile unsigned *)0x4c8000E4)	
#define SYSIFCON0		(*(volatile unsigned *)0x4c800130)	
#define SYSIFCON1		(*(volatile unsigned *)0x4c800134)	
#define DITHMODE1		(*(volatile unsigned *)0x4c800138)	
#define SIFCCON0		(*(volatile unsigned *)0x4c80013C)	
#define SIFCCON1		(*(volatile unsigned *)0x4c800140)	
#define SIFCCON2		(*(volatile unsigned *)0x4c800144)	
#define CPUTRIGCON1		(*(volatile unsigned *)0x4c80015C)	
#define CPUTRIGCON2		(*(volatile unsigned *)0x4c800160)	
#define VIDW00ADD0B1	(*(volatile unsigned *)0x4c800068)	
#define VIDW01ADD0		(*(volatile unsigned *)0x4c80006C)	
#define GPCCON			(*(volatile unsigned *)(0x56000020))	
#define GPCDAT			(*(volatile unsigned *)(0x56000024))	
#define GPCUDP			(*(volatile unsigned *)(0x56000028))	
#define GPDCON			(*(volatile unsigned *)(0x56000030))	
#define GPDDAT			(*(volatile unsigned *)(0x56000034))	
#define GPDUDP			(*(volatile unsigned *)(0x56000038))	
#define MISCCR			(*(volatile unsigned *)(0x56000080))	
#define GPLCON			(*(volatile unsigned *)(0x560000f0))	
#define GPLDAT			(*(volatile unsigned *)(0x560000f4))	
#define GPBCON 			(*(volatile unsigned long *)0x56000010)
#define GPBDAT 			(*(volatile unsigned long *)0x56000014)
#define GPGCON 			(*(volatile unsigned long *)0x56000060)
#define GPGDAT 			(*(volatile unsigned long *)0x56000064)

#define FRAME_BUFFER   		0x32f00000
#define ROW					272
#define COL					480
#define HSPW 				(41 - 1)
#define HBPD			 	(2- 1)
#define HFPD 				(2 - 1)
#define VSPW				(10 - 1)
#define VBPD 				(2 - 1)
#define VFPD 				(2 - 1)
#define LINEVAL 			(271)
#define HOZVAL				(479)

// 初始化LCD
void lcd_init(void)
{
	// 配置GPIO用于LCD相关的功能
	GPCCON = 0xAAAAAAAA;
	GPDCON = 0xAAAAAAAA; 

#if 0
	// 打开LCD电源
	GPBCON &= ~(0x3<<2);
	GPBCON |= (1<<2);
	GPBDAT |= (1<<1);
#endif
	// 打开背光
	GPBCON &= ~(0x3<<(3*2));
	GPBCON |= (1<<(3*2));
	GPBDAT |= (1<<3);

	// 配置VIDCONx，设置接口类型、时钟、极性和使能LCD控制器等
	VIDCON0 = (0<<22)|(0<<13)|(9<<6)|(1<<5)|(1<<4)|(0<<2)|(3<<0);
	VIDCON1 = (0<<7)|(1<<6)|(1<<5)|(0<<4);	/* VCLK在下降沿取数据。VSYNC和HSYNC都是低电平有效，平时是高电平。VDEN高有效 */

	// 配置VIDTCONx，设置时序和长宽等
	// 设置时序
	VIDTCON0 = VBPD<<16 | VFPD<<8 | VSPW<<0;
	VIDTCON1 = HBPD<<16 | HFPD<<8 | HSPW<<0;
	// 设置长宽
	VIDTCON2 = (LINEVAL << 11) | (HOZVAL << 0);

	// 配置WINCON0，设置window0的数据格式
	// 需要配置BITSWP,BYTSWP,HAWSWP
	WINCON0 = (1<<0);
	WINCON0 |= (1<<16);		/* HAWSWP=1 */
	WINCON0 &= ~(0xf << 2);
//	WINCON0 |= 0xB<<2;	/* 选择24bpp，即8-8-8模式 */
	WINCON0 |= 0x5<<2;	/* 选择16bpp，即5-6-5模式 */

	// 配置VIDOSD0A/B/C,设置window0的坐标系
#define LeftTopX     0
#define LeftTopY     0
#define RightBotX   479
#define RightBotY   271
	VIDOSD0A = (LeftTopX<<11) | (LeftTopY << 0);
	VIDOSD0B = (RightBotX<<11) | (RightBotY << 0);
	/* VIDOSD0C是什么? */

	// 置VIDW00ADD0B0和VIDW00ADD1B0，设置framebuffer的地址
	VIDW00ADD0B0 = FRAME_BUFFER;
//	VIDW00ADD1B0 = (((HOZVAL + 1)*4 + 0) * (LINEVAL + 1)) & (0xffffff);
//	VIDW00ADD1B0 = FRAME_BUFFER + ROW * COL * 2;	/* 这个参数好像没什么用? */
	/* 测试时发现，FRAME_BUFFER + (HOZVAL + 1) * (LINEVAL + 1) * 3 + 1可以，去掉+1就不行。为什么? */
//	VIDW00ADD1B0 = FRAME_BUFFER + (HOZVAL + 1) * (LINEVAL + 1) * 4 + 0;	/* 每个像素是8-8-8模式，对齐后占4字节 */
	VIDW00ADD1B0 = FRAME_BUFFER + (HOZVAL + 1) * (LINEVAL + 1) * 2 + 0;	/* 每个像素是5-6-5模式，对齐后占2字节 */
	
}

// 描点
void lcd_draw_pixel(int row, int col, int color)
{
	unsigned short * pixel = (unsigned short  *)FRAME_BUFFER;
	*(pixel + row * COL + col) = color;
}

// 清屏
void lcd_clear_screen(int color)
{
#if 0
	int i, j;
	for (i = 0; i < ROW; i++)
	{
		for (j = 0; j < COL; j++)
		{
			lcd_draw_pixel(i, j, color);
		}
	}
#endif

#if 0
	int i;
	unsigned short * pixel = (unsigned short  *)FRAME_BUFFER;

	for (i = 0; i < ROW * COL; i++)
	{
		*(pixel + i) = color;
	}
#endif

	int i;
	unsigned long * pixel = (unsigned long  *)FRAME_BUFFER;

	color = color | (color << 16);
	for (i = 0; i < ROW * COL / 2; i++)
	{
		*(pixel + i) = color;
	}
}

// 划横线
void lcd_draw_hline(int row, int col1, int col2, int color)
{
	int j;

	// 描第row行，第j列
	for (j = col1; j <= col2; j++)
		lcd_draw_pixel(row, j, color);
}

// 划竖线
void lcd_draw_vline(int col, int row1, int row2, int color)
{
	int i;
	// 描第i行，第col列
	for (i = row1; i <= row2; i++)
		lcd_draw_pixel(i, col, color);
}

// 划十字
void lcd_draw_cross(int row, int col, int halflen, int color)
{
	lcd_draw_hline(row, col-halflen, col+halflen, color);
	lcd_draw_vline(col, row-halflen, row+halflen, color);
}

// 绘制同心圆
void lcd_draw_circle(void)
{
	int x,y;
	int color;
	unsigned char red,green,blue,alpha;
	int xsize = ROW;
	int ysize = COL;

	for (y = 0; y < ysize; y++)
		for (x = 0; x < xsize; x++)
		{
			color = ((x-xsize/2)*(x-xsize/2) + (y-ysize/2)*(y-ysize/2))/64;
			red   = (color/8) % 256;
			green = (color/4) % 256;
			blue  = (color/2) % 256;
			alpha = (color*2) % 256;

			color |= ((int)alpha << 24);
			color |= ((int)red   << 16);
			color |= ((int)green << 8 );
			color |= ((int)blue       );

			lcd_draw_pixel(x,y,color);
		}
}
