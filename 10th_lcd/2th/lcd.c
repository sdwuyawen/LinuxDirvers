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

static struct fb_info *s3c_lcd;

struct fb_ops s3c_fb_ops = {
	.owner		= THIS_MODULE,
//	.fb_setcolreg	= s3c_fb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};


static int lcd_init(void)
{
	/* 1. 分配一个fb_info */
	s3c_lcd = framebuffer_alloc(0, NULL);	/* 参数0是额外分配的大小，设置为0 */

	/* 2. 设置 */
	/* 2.1 设置固定的参数 */
	strcpy(s3c_lcd->fix.id, "mylcd");
	s3c_lcd->smem_len = 272*480*16/8
	s3c_lcd->type = FB_TYPE_PACKED_PIXELS;
	s3c_lcd->visual = FB_VISUAL_TRUECOLOR;	/* TFT */
	s3c_lcd->fix.line_length = 272*2;
	
	/* 2.2 设置可变的参数 */
	s3c_lcd->var.xres = 272;
	s3c_lcd->var.yres = 480;
	s3c_lcd->var.xres_virtual = 272;
	s3c_lcd->var.yres_virtual = 480;
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
	s3c_lcd->fb_ops = &s3c_fb_ops;
	
	/* 2.4 其他的设置 */
	//s3c_lcd->pseudo_palette =; //
	//s3c_lcd->screen_base  = ;  /* 显存的虚拟地址 */ 
	s3c_lcd->screen_size   = 272*480*16/8;

	/* 3. 硬件相关的操作 */
	/* 3.1 配置GPIO用于LCD */
	/* 3.2 根据LCD手册设置LCD控制器, 比如VCLK的频率等 */
	/* 3.3 分配显存(framebuffer), 并把地址告诉LCD控制器 */
	//s3c_lcd->smem_start = 		/* 显存的物理地址 */

	/* 4. 注册 */
	register_framebuffer(s3c_lcd);

}

static void lcd_exit(void)
{
}

module_init(lcd_init);
module_exit(lcd_exit); 

MODULE_LICENSE("GPL");