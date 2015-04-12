#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include <asm/mach/map.h>
#include <asm/arch/regs-lcd.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/fb.h>


struct fb_ops s3c_fb_ops = {
	.owner		= THIS_MODULE,
//	.fb_setcolreg	= s3c_fb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static struct fb_info *s3c_lcd;

/* GPIO寄存器 */
static volatile unsigned long *gpbcon;
static volatile unsigned long *gpbdat;
static volatile unsigned long *gpccon;
static volatile unsigned long *gpdcon;

/* LCD控制器寄存器 */
static volatile unsigned long *lcd_regs;
static volatile unsigned long *VIDCON0;
static volatile unsigned long *VIDCON1;
static volatile unsigned long *VIDTCON0;
static volatile unsigned long *VIDTCON1;
static volatile unsigned long *VIDTCON2;
static volatile unsigned long *WINCON0;
static volatile unsigned long *VIDOSD0A;
static volatile unsigned long *VIDOSD0B;
static volatile unsigned long *VIDW00ADD0B0;
static volatile unsigned long *VIDW00ADD1B0;

/* LCD控制器配置数据 */
#define FRAME_BUFFER   		s3c_lcd->screen_base	/* 显存地址，需要修改.0x32f00000 */
#define ROW					272
#define COL					480
#define HSPW 				(41 - 1)
#define HBPD			 	(2- 1)
#define HFPD 				(2 - 1)
#define VSPW				(10 - 1)
#define VBPD 				(2 - 1)
#define VFPD 				(2 - 1)
#define LINEVAL 				(271)
#define HOZVAL				(479)


// 描点
void lcd_draw_pixel(int row, int col, int color)
{
	unsigned short * pixel = (unsigned short  *)FRAME_BUFFER;
	*(pixel + row * COL + col) = color;
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

static int lcd_init(void)
{
	/* 1. 分配一个fb_info */
	s3c_lcd = framebuffer_alloc(0, NULL);	/* 参数0是额外分配的大小，设置为0 */

	/* 2. 设置 */
	/* 2.1 设置固定的参数 */
	strcpy(s3c_lcd->fix.id, "mylcd");
	s3c_lcd->fix.smem_len = 272*480*16/8;
	s3c_lcd->fix.type = FB_TYPE_PACKED_PIXELS;
	s3c_lcd->fix.visual = FB_VISUAL_TRUECOLOR;	/* TFT */
	s3c_lcd->fix.line_length = 480*2;
	
	/* 2.2 设置可变的参数 */
	s3c_lcd->var.xres = 480;
	s3c_lcd->var.yres = 272;
	s3c_lcd->var.xres_virtual = 480;
	s3c_lcd->var.yres_virtual = 272;
	s3c_lcd->var.bits_per_pixel = 16;

	/* RGB:565 */
	s3c_lcd->var.red.offset     = 11;
	s3c_lcd->var.red.length     = 5;
	
	s3c_lcd->var.green.offset   = 5;
	s3c_lcd->var.green.length   = 6;

	s3c_lcd->var.blue.offset    = 0;
	s3c_lcd->var.blue.length    = 5;

	s3c_lcd->var.activate       = FB_ACTIVATE_NOW;
	
	
	/* 2.3 设置操作函数 */
	s3c_lcd->fbops = &s3c_fb_ops;
	
	/* 2.4 其他的设置 */
	//s3c_lcd->pseudo_palette =; //假的调色板
	//s3c_lcd->screen_base  = xxx;  /* 显存的虚拟地址 */ 
	s3c_lcd->screen_size   = 272*480*16/8;

	/* 3. 硬件相关的操作 */
	/* 3.1 配置GPIO用于LCD */
	gpbcon = ioremap(0x56000010, 8);
	gpbdat = gpbcon+1;
	gpccon = ioremap(0x56000020, 4);
	gpdcon = ioremap(0x56000030, 4);

	*gpccon  = 0xaaaaaaaa;   /* GPIO管脚用于VD[7:0],LCDVF[2:0],VM,VFRAME,VLINE,VCLK,LEND */
	*gpdcon  = 0xaaaaaaaa;   /* GPIO管脚用于VD[23:8] */

	*gpbcon &= ~(0x03<<(3*2));  /* GPB3设置为输出引脚 */
	*gpbcon |= ( 0x1<<(3*2) );
	*gpbdat |= (0x1<<3);			/* 背光亮 */

	/* 3.2 根据LCD手册设置LCD控制器, 比如VCLK的频率等 */
	lcd_regs = ioremap(0x4C800000, 0x200);
	VIDCON0 = ioremap(0x4c800000, 4);
	VIDCON1 = ioremap(0x4c800004, 4);
	VIDTCON0 = ioremap(0x4c800008, 4);
	VIDTCON1 = ioremap(0x4c80000C, 4);
	VIDTCON2 = ioremap(0x4c800010, 4);
	WINCON0 = ioremap(0x4c800014, 4);
	VIDOSD0A = ioremap(0x4c800028, 4);
	VIDOSD0B = ioremap(0x4c80002C, 4);
	VIDW00ADD0B0 = ioremap(0x4c800064, 4);
	VIDW00ADD1B0 = ioremap(0x4c80007C, 4);


	// 配置VIDCONx，设置接口类型、时钟、极性和使能LCD控制器等
	*VIDCON0 = (0<<22)|(0<<13)|(9<<6)|(1<<5)|(1<<4)|(0<<2)|(3<<0);
	*VIDCON1 = (0<<7)|(1<<6)|(1<<5)|(0<<4);	/* VCLK在下降沿取数据。VSYNC和HSYNC都是低电平有效，平时是高电平。VDEN高有效 */

	// 配置VIDTCONx，设置时序和长宽等
	// 设置时序
	*VIDTCON0 = VBPD<<16 | VFPD<<8 | VSPW<<0;
	*VIDTCON1 = HBPD<<16 | HFPD<<8 | HSPW<<0;
	// 设置长宽
	*VIDTCON2 = (LINEVAL << 11) | (HOZVAL << 0);

	// 配置WINCON0，设置window0的数据格式
	// 需要配置BITSWP,BYTSWP,HAWSWP
	*WINCON0 = (1<<0);
	*WINCON0 |= (1<<16);		/* HAWSWP=1 */
	*WINCON0 &= ~(0xf << 2);
	*WINCON0 |= 0x5<<2;	/* 选择16bpp，即5-6-5模式 */

	// 配置VIDOSD0A/B/C,设置window0的坐标系
#define LeftTopX     0
#define LeftTopY     0
#define RightBotX   479	/* 这里x,y和韦东山相反 */
#define RightBotY   271
	*VIDOSD0A = (LeftTopX<<11) | (LeftTopY << 0);
	*VIDOSD0B = (RightBotX<<11) | (RightBotY << 0);	
	
	/* 3.3 分配显存(framebuffer), 并把地址告诉LCD控制器 */
	//申请显存，返回虚拟地址，申请后s3c_lcd->fix.smem_start存放显存物理地址
	s3c_lcd->screen_base = dma_alloc_writecombine(NULL, s3c_lcd->fix.smem_len, &s3c_lcd->fix.smem_start, GFP_KERNEL);
	// 置VIDW00ADD0B0和VIDW00ADD1B0，设置framebuffer的地址
	*VIDW00ADD0B0 = s3c_lcd->fix.smem_start;
	*VIDW00ADD1B0 = s3c_lcd->fix.smem_start + s3c_lcd->fix.smem_len;

	printk("s3c_lcd->fix.smem_start=0x%08x\n", (unsigned int)s3c_lcd->fix.smem_start);
	printk("*VIDW00ADD0B0=0x%08x\n", (unsigned int)*VIDW00ADD0B0);
	printk("s3c_lcd->screen_base=0x%08x\n", (unsigned int)s3c_lcd->screen_base);

	lcd_clear_screen(((0xFF >> 3) << 11)|((0x00 >> 2)  << 5)|((0x00 >> 3)  << 0));		// 绿色  								// 黑色
	lcd_clear_screen(((0x00 >> 3) << 11)|((0xFF >> 2)  << 5)|((0x00 >> 3)  << 0));		//红色
	lcd_clear_screen(((0x00 >> 3) << 11)|((0x00 >> 2)  << 5)|((0xFF >> 3)  << 0));		//蓝色
	lcd_draw_hline(10, 20 ,100 , 0x000000);
	lcd_draw_vline(30, 5, 200, 0xff0000);
	//s3c_lcd->smem_start = xxx		/* 显存的物理地址 */
	
	/* 4. 注册 */
	register_framebuffer(s3c_lcd);

	return 0;

}

static void lcd_exit(void)
{
	
}

module_init(lcd_init);
module_exit(lcd_exit); 

MODULE_LICENSE("GPL");
