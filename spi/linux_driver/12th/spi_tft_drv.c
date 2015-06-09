#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/spi/spi.h>

#include <linux/fs.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

#include <linux/delay.h>




/* 构造注册 spi_driver */

static int major;
static struct class *class;

static unsigned char *ker_buf;
static int spi_tft_rs_pin;
static struct spi_device *spi_oled_dev;

#if 0
static void OLED_Set_DC(char val)
{
    s3c2410_gpio_setpin(spi_tft_rs_pin, val);
}

static void OLEDWriteCmd(unsigned char cmd)
{
    OLED_Set_DC(0); /* command */
    spi_write(spi_oled_dev, &cmd, 1);
    OLED_Set_DC(1); /*  */
}

static void OLEDWriteDat(unsigned char dat)
{
    OLED_Set_DC(1); /* data */
    spi_write(spi_oled_dev, &dat, 1);
    OLED_Set_DC(1); /*  */
}

static void OLEDSetPageAddrMode(void)
{
    OLEDWriteCmd(0x20);
    OLEDWriteCmd(0x02);
}

static void OLEDSetPos(int page, int col)
{
    OLEDWriteCmd(0xB0 + page); /* page address */

    OLEDWriteCmd(col & 0xf);   /* Lower Column Start Address */
    OLEDWriteCmd(0x10 + (col >> 4));   /* Lower Higher Start Address */
}


static void OLEDClear(void)
{
    int page, i;
    for (page = 0; page < 8; page ++)
    {
        OLEDSetPos(page, 0);
        for (i = 0; i < 128; i++)
            OLEDWriteDat(0);
    }
}

void OLEDClearPage(int page)
{
    int i;
    OLEDSetPos(page, 0);
    for (i = 0; i < 128; i++)
        OLEDWriteDat(0);    
}

void OLEDInit(void)
{
    /* 向OLED发命令以初始化 */
    OLEDWriteCmd(0xAE); /*display off*/ 
    OLEDWriteCmd(0x00); /*set lower column address*/ 
    OLEDWriteCmd(0x10); /*set higher column address*/ 
    OLEDWriteCmd(0x40); /*set display start line*/ 
    OLEDWriteCmd(0xB0); /*set page address*/ 
    OLEDWriteCmd(0x81); /*contract control*/ 
    OLEDWriteCmd(0x66); /*128*/ 
    OLEDWriteCmd(0xA1); /*set segment remap*/ 
    OLEDWriteCmd(0xA6); /*normal / reverse*/ 
    OLEDWriteCmd(0xA8); /*multiplex ratio*/ 
    OLEDWriteCmd(0x3F); /*duty = 1/64*/ 
    OLEDWriteCmd(0xC8); /*Com scan direction*/ 
    OLEDWriteCmd(0xD3); /*set display offset*/ 
    OLEDWriteCmd(0x00); 
    OLEDWriteCmd(0xD5); /*set osc division*/ 
    OLEDWriteCmd(0x80); 
    OLEDWriteCmd(0xD9); /*set pre-charge period*/ 
    OLEDWriteCmd(0x1f); 
    OLEDWriteCmd(0xDA); /*set COM pins*/ 
    OLEDWriteCmd(0x12); 
    OLEDWriteCmd(0xdb); /*set vcomh*/ 
    OLEDWriteCmd(0x30); 
    OLEDWriteCmd(0x8d); /*set charge pump enable*/ 
    OLEDWriteCmd(0x14); 

    OLEDSetPageAddrMode();

    OLEDClear();
    
    OLEDWriteCmd(0xAF); /*display ON*/    
}


#define OLED_CMD_INIT       0x100001
#define OLED_CMD_CLEAR_ALL  0x100002
#define OLED_CMD_CLEAR_PAGE 0x100003
#define OLED_CMD_SET_POS    0x100004

static long oled_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int page;
    int col;
    
    switch (cmd)
    {
        case OLED_CMD_INIT:
        {
            OLEDInit();
            break;
        }
        case OLED_CMD_CLEAR_ALL:
        {
            OLEDClear();
            break;
        }
        case OLED_CMD_CLEAR_PAGE:
        {
            page = arg;
            OLEDClearPage(page);
            break;
        }
        case OLED_CMD_SET_POS:
        {
            page = arg & 0xff;
            col  = (arg >> 8) & 0xff;
            OLEDSetPos(page, col);
            break;
        }
    }
    return 0;
}

static ssize_t oled_write(struct file *file,
				      const char __user *buf,
				      size_t count, loff_t *ppos)
{
    if (count > 4096)
        return -EINVAL;
    copy_from_user(ker_buf, buf, count);
    spi_write(spi_oled_dev, ker_buf, count);
    return 0;
}

#endif

//-----------------------------SPI 总线配置--------------------------------------//
#define USE_HARDWARE_SPI     0		//1:Enable Hardware SPI;0:USE Soft SPI

//-------------------------屏幕物理像素设置--------------------------------------//
#define LCD_X_SIZE	        176
#define LCD_Y_SIZE	        220

/////////////////////////////////////用户配置区///////////////////////////////////	 
//支持横竖屏快速定义切换
#define USE_HORIZONTAL  		0	//定义是否使用横屏 		0,不使用.1,使用.

#ifdef USE_HORIZONTAL//如果定义了横屏 
#define X_MAX_PIXEL	        LCD_Y_SIZE
#define Y_MAX_PIXEL	        LCD_X_SIZE
#else
#define X_MAX_PIXEL	        LCD_X_SIZE
#define Y_MAX_PIXEL	        LCD_Y_SIZE
#endif
//////////////////////////////////////////////////////////////////////////////////
	 
#if 0
#define RED  	0xf800
#define GREEN	0x07e0
#define BLUE 	0x001f
#define WHITE	0xffff
#define BLACK	0x0000
#define YELLOW  0xFFE0
#define GRAY0   0xEF7D   	//灰色0 3165 00110 001011 00101
#define GRAY1   0x8410      	//灰色1      00000 000000 00000
#define GRAY2   0x4208      	//灰色2  1111111111011111
#endif

/*
	LCD 颜色代码，CL_是Color的简写
	16Bit由高位至低位， RRRR RGGG GGGB BBBB

	下面的RGB 宏将24位的RGB值转换为16位格式。
	启动windows的画笔程序，点击编辑颜色，选择自定义颜色，可以获得的RGB值。

	推荐使用迷你取色器软件获得你看到的界面颜色。
*/
#define RGB(R,G,B)	(((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3))	/* 将8位R,G,B转化为 16位RGB565格式 */
#define RGB565_R(x)  ((x >> 8) & 0xF8)
#define RGB565_G(x)  ((x >> 3) & 0xFC)
#define RGB565_B(x)  ((x << 3) & 0xF8)
enum
{
	CL_WHITE        = RGB(255,255,255),	/* 白色 */
	CL_BLACK        = RGB(  0,  0,  0),	/* 黑色 */
	CL_RED          = RGB(255,	0,  0),	/* 红色 */
	CL_GREEN        = RGB(  0,255,  0),	/* 绿色 */
	CL_BLUE         = RGB(  0,	0,255),	/* 蓝色 */
	CL_YELLOW       = RGB(255,255,  0),	/* 黄色 */

	CL_GREY			= RGB( 98, 98, 98), 	/* 深灰色 */
	CL_GREY1		= RGB( 150, 150, 150), 	/* 浅灰色 */
	CL_GREY2		= RGB( 180, 180, 180), 	/* 浅灰色 */
	CL_GREY3		= RGB( 200, 200, 200), 	/* 最浅灰色 */
	CL_GREY4		= RGB( 230, 230, 230), 	/* 最浅灰色 */

	CL_BUTTON_GREY	= RGB( 220, 220, 220), /* WINDOWS 按钮表面灰色 */

	CL_MAGENTA      = 0xF81F,	/* 红紫色，洋红色 */
	CL_CYAN         = 0x7FFF,	/* 蓝绿色，青色 */

	CL_BLUE1        = RGB(  0,  0, 240),		/* 深蓝色 */
	CL_BLUE2        = RGB(  0,  0, 128),		/* 深蓝色 */
	CL_BLUE3        = RGB(  68, 68, 255),		/* 浅蓝色1 */
	CL_BLUE4        = RGB(  0, 64, 128),		/* 浅蓝色1 */

	/* UI 界面 Windows控件常用色 */
	CL_BTN_FACE		= RGB(236, 233, 216),	/* 按钮表面颜色(灰) */
	CL_BOX_BORDER1	= RGB(172, 168,153),	/* 分组框主线颜色 */
	CL_BOX_BORDER2	= RGB(255, 255,255),	/* 分组框阴影线颜色 */


	CL_MASK			= 0x9999	/* 颜色掩码，用于文字背景透明 */
};

static void lcd_reset_clr(void)
{
//	GPGDAT &= ~(1 << 5);
	s3c2410_gpio_setpin(S3C2410_GPG5, 0);
}

static void lcd_reset_set(void)
{
//	GPGDAT |= (1 << 5);
//	s3c2410_gpio_setpin(spi_tft_rs_pin, 1);
	s3c2410_gpio_setpin(S3C2410_GPG5, 1);
}

static void lcd_rs_clr(void)
{
	s3c2410_gpio_setpin(spi_tft_rs_pin, 0);
}

static void lcd_rs_set(void)
{
	s3c2410_gpio_setpin(spi_tft_rs_pin, 1);
}

/****************************************************************************
* 名    称：Lcd_WriteIndex(unsigned char Index)
* 功    能：向液晶屏写一个8位指令
* 入口参数：Index   寄存器地址
* 出口参数：无
* 说    明：调用前需先选中控制器，内部函数
****************************************************************************/
void Lcd_WriteIndex(unsigned char index)
{
	lcd_rs_clr();

	spi_write(spi_oled_dev, &index, 1);

}

/****************************************************************************
* 名    称：Lcd_WriteData(unsigned char Data)
* 功    能：向液晶屏写一个8位数据
* 入口参数：dat     寄存器数据
* 出口参数：无
* 说    明：向控制器指定地址写入数据，内部函数
****************************************************************************/
void Lcd_WriteData(unsigned char data)
{
	lcd_rs_set();

	spi_write(spi_oled_dev, &data, 1);
}

/****************************************************************************
* 名    称：void Lcd_WriteData_16Bit(unsigned short Data)
* 功    能：向液晶屏写一个16位数据
* 入口参数：Data
* 出口参数：无
* 说    明：向控制器指定地址写入一个16位数据
****************************************************************************/
void Lcd_WriteData_16Bit(unsigned short data)
{	
	Lcd_WriteData(data >> 8);
	Lcd_WriteData(data);	
}

/****************************************************************************
* 名    称：void LCD_WriteReg(unsigned char Index,unsigned short Data)
* 功    能：写寄存器数据
* 入口参数：Index,Data
* 出口参数：无
* 说    明：本函数为组合函数，向Index地址的寄存器写入Data值
****************************************************************************/
void LCD_WriteReg(unsigned char index,unsigned short data)
{
	Lcd_WriteIndex(index);
  	Lcd_WriteData_16Bit(data);
}

/****************************************************************************
* 名    称：void Lcd_Reset(void)
* 功    能：液晶硬复位函数
* 入口参数：无
* 出口参数：无
* 说    明：液晶初始化前需执行一次复位操作
****************************************************************************/
void Lcd_Reset(void)
{
	s3c2410_gpio_cfgpin(S3C2410_GPG5, S3C2410_GPIO_OUTPUT);
	
	lcd_reset_clr();
	msleep(100);
	lcd_reset_set();
	msleep(50);
}



/*************************************************
函数名：LCD_Set_XY
功能：设置lcd显示起始点
入口参数：xy坐标
返回值：无
*************************************************/
void Lcd_SetXY(unsigned short Xpos, unsigned short Ypos)
{	
#if USE_HORIZONTAL//如果定义了横屏  	    	
	LCD_WriteReg(0x21,Xpos);
	LCD_WriteReg(0x20,Ypos);
#else//竖屏	
	LCD_WriteReg(0x20,Xpos);
	LCD_WriteReg(0x21,Ypos);
#endif
	Lcd_WriteIndex(0x22);		
} 
/*************************************************
函数名：LCD_Set_Region
功能：设置lcd显示区域，在此区域写点数据自动换行
入口参数：xy起点和终点
返回值：无
*************************************************/
//设置显示窗口
void Lcd_SetRegion(unsigned char xStar, unsigned char yStar,unsigned char xEnd,unsigned char yEnd)
{
//#if USE_HORIZONTAL//如果定义了横屏
// 	LCD_WriteReg(0x36,xEnd);
// 	LCD_WriteReg(0x37,xStar);
// 	LCD_WriteReg(0x38,yEnd);
// 	LCD_WriteReg(0x39,yStar);
// 	LCD_WriteReg(0x20,xStar);
// 	LCD_WriteReg(0x21,yStar);
// #else//竖屏	
	LCD_WriteReg(0x38,xEnd);
	LCD_WriteReg(0x39,xStar);
	LCD_WriteReg(0x36,yEnd);
	LCD_WriteReg(0x37,yStar);
	LCD_WriteReg(0x21,xStar);
	LCD_WriteReg(0x20,yStar);
// #endif
	Lcd_WriteIndex(0x22);	
}

	
/*************************************************
函数名：LCD_DrawPoint
功能：画一个点
入口参数：xy坐标和颜色数据
返回值：无
*************************************************/
void Gui_DrawPoint(unsigned short x,unsigned short y,unsigned short Data)
{
	Lcd_SetXY(x,y);
	Lcd_WriteData_16Bit(Data);

}    

/*************************************************
函数名：Lcd_Clear
功能：全屏清屏函数
入口参数：填充颜色COLOR
返回值：无
*************************************************/
void Lcd_Clear(unsigned short Color)               
{	
   unsigned int i,m;
   Lcd_SetRegion(0,0,X_MAX_PIXEL-1,Y_MAX_PIXEL-1);
   for(i=0;i<X_MAX_PIXEL;i++)
    for(m=0;m<Y_MAX_PIXEL;m++)
    {	
	  	Lcd_WriteData_16Bit(Color);
    }   
}


/****************************************************************************
* 名    称：void spi_tft_reg_init(void)
* 功    能：液晶初始化函数
* 入口参数：无
* 出口参数：无
* 说    明：液晶初始化_ILI9225_176X220
****************************************************************************/
static void spi_tft_reg_init(void)
{	
	Lcd_Reset(); //Reset before LCD Init.

	//LCD Init For 2.2inch LCD Panel with ILI9225.	
	LCD_WriteReg(0x10, 0x0000); // Set SAP,DSTB,STB
	LCD_WriteReg(0x11, 0x0000); // Set APON,PON,AON,VCI1EN,VC
	LCD_WriteReg(0x12, 0x0000); // Set BT,DC1,DC2,DC3
	LCD_WriteReg(0x13, 0x0000); // Set GVDD
	LCD_WriteReg(0x14, 0x0000); // Set VCOMH/VCOML voltage
	msleep(40); // Delay 20 ms
	
	// Please follow this power on sequence
	LCD_WriteReg(0x11, 0x0018); // Set APON,PON,AON,VCI1EN,VC
	LCD_WriteReg(0x12, 0x1121); // Set BT,DC1,DC2,DC3
	LCD_WriteReg(0x13, 0x0063); // Set GVDD
	LCD_WriteReg(0x14, 0x3961); // Set VCOMH/VCOML voltage
	LCD_WriteReg(0x10, 0x0800); // Set SAP,DSTB,STB
	msleep(10); // Delay 10 ms
	LCD_WriteReg(0x11, 0x1038); // Set APON,PON,AON,VCI1EN,VC
	msleep(30); // Delay 30 ms
	
	
	LCD_WriteReg(0x02, 0x0100); // set 1 line inversion

#if USE_HORIZONTAL//如果定义了横屏
	//R01H:SM=0,GS=0,SS=0 (for details,See the datasheet of ILI9225)
	LCD_WriteReg(0x01, 0x001C); // set the display line number and display direction
	//R03H:BGR=1,ID0=1,ID1=1,AM=1 (for details,See the datasheet of ILI9225)
	LCD_WriteReg(0x03, 0x1038); // set GRAM write direction .
#else//竖屏
	//R01H:SM=0,GS=0,SS=1 (for details,See the datasheet of ILI9225)
	LCD_WriteReg(0x01, 0x011C); // set the display line number and display direction 
	//R03H:BGR=1,ID0=1,ID1=1,AM=0 (for details,See the datasheet of ILI9225)
	LCD_WriteReg(0x03, 0x1030); // set GRAM write direction.
#endif

	LCD_WriteReg(0x07, 0x0000); // Display off
	LCD_WriteReg(0x08, 0x0808); // set the back porch and front porch
	LCD_WriteReg(0x0B, 0x1100); // set the clocks number per line
	LCD_WriteReg(0x0C, 0x0000); // CPU interface
	LCD_WriteReg(0x0F, 0x0501); // Set Osc
	LCD_WriteReg(0x15, 0x0020); // Set VCI recycling
	LCD_WriteReg(0x20, 0x0000); // RAM Address
	LCD_WriteReg(0x21, 0x0000); // RAM Address
	
	//------------------------ Set GRAM area --------------------------------//
	LCD_WriteReg(0x30, 0x0000); 
	LCD_WriteReg(0x31, 0x00DB); 
	LCD_WriteReg(0x32, 0x0000); 
	LCD_WriteReg(0x33, 0x0000); 
	LCD_WriteReg(0x34, 0x00DB); 
	LCD_WriteReg(0x35, 0x0000); 
	LCD_WriteReg(0x36, 0x00AF); 
	LCD_WriteReg(0x37, 0x0000); 
	LCD_WriteReg(0x38, 0x00DB); 
	LCD_WriteReg(0x39, 0x0000); 
	
	
	// ---------- Adjust the Gamma 2.2 Curve -------------------//
	LCD_WriteReg(0x50, 0x0603); 
	LCD_WriteReg(0x51, 0x080D); 
	LCD_WriteReg(0x52, 0x0D0C); 
	LCD_WriteReg(0x53, 0x0205); 
	LCD_WriteReg(0x54, 0x040A); 
	LCD_WriteReg(0x55, 0x0703); 
	LCD_WriteReg(0x56, 0x0300); 
	LCD_WriteReg(0x57, 0x0400); 
	LCD_WriteReg(0x58, 0x0B00); 
	LCD_WriteReg(0x59, 0x0017); 
	
	
	
	LCD_WriteReg(0x0F, 0x0701); // Vertical RAM Address Position
	LCD_WriteReg(0x07, 0x0012); // Vertical RAM Address Position
	msleep(50); // Delay 50 ms
	LCD_WriteReg(0x07, 0x1017); // Vertical RAM Address Position  
	
}

/*************************************************
函数名：Lcd_Clear
功能：全屏清屏函数
入口参数：填充颜色COLOR
返回值：无
*************************************************/
static void lcd_clear(unsigned short color)               
{	
	unsigned int i,m;
	Lcd_SetRegion(0, 0, X_MAX_PIXEL - 1, Y_MAX_PIXEL - 1);
	for(i = 0; i < X_MAX_PIXEL; i++)
		for(m = 0; m < Y_MAX_PIXEL; m++)
		{	
			Lcd_WriteData_16Bit(color);
		}   
}

void spi_tft_lcd_clear(unsigned int color)
{
	Lcd_Clear((unsigned short)color);
}

#define TFT_CMD_INIT       		0x100001
#define TFT_CMD_CLEAR_ALL  		0x100002

static long tft_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd)
	{
		case TFT_CMD_INIT:
		{
			printk("TFT_CMD_INIT\n");
//			sps_tft_cld_init();
			spi_tft_reg_init();
			break;
		}
		case TFT_CMD_CLEAR_ALL:
		{
			printk("TFT_CMD_CLEAR_ALL\n");
			spi_tft_lcd_clear(arg);
			break;
		}
	}
	return 0;
}

static struct file_operations spitft_ops = {
	.owner            = THIS_MODULE,
	.unlocked_ioctl   = tft_ioctl,
	.write            = NULL,
};

static int __devinit spi_tft_probe(struct spi_device *spi)
{
	spi_oled_dev = spi;
	spi_tft_rs_pin = (int)spi->dev.platform_data;						/* spi->dev指向spi_board_info，在spi_new_device()里被注册 */
	s3c2410_gpio_cfgpin(spi_tft_rs_pin, S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_cfgpin(spi->chip_select, S3C2410_GPIO_OUTPUT);		/* spi->chip_select指向spi_board_info的chip_select，在spi_new_device()里被注册 */

//	printk("spi_tft_rs_pin = %08x\n", spi_tft_rs_pin);					/* 0xa7 == S3C2410_GPF7 */
//	printk("spi->chip_select = %08x\n", spi->chip_select);					/* 0xea == S3C2410_GPH10 */
	
	ker_buf = kmalloc(4096, GFP_KERNEL);

	/* 注册一个 file_operations */
	major = register_chrdev(0, "spigpio_tft", &spitft_ops);

	class = class_create(THIS_MODULE, "spigpio_tft");

	/* 为了让mdev根据这些信息来创建设备节点 */
//	device_create(class, NULL, MKDEV(major, 0), NULL, "spigpio_tft"); /* /dev/spigpio_tft */
	class_device_create(class, NULL, MKDEV(major, 0), NULL, "spigpio_tft");
    
    
    return 0;
}

static int __devexit spi_tft_remove(struct spi_device *spi)
{
//	device_destroy(class, MKDEV(major, 0));
	class_device_destroy(class, MKDEV(major, 0));
	class_destroy(class);
	unregister_chrdev(major, "spigpio_tft");

	kfree(ker_buf);
    
	return 0;
}


static struct spi_driver spi_tft_drv = {
	.driver = {
		.name	= "spigpio_tft",
		.owner	= THIS_MODULE,
	},
	.probe		= spi_tft_probe,
	.remove		= __devexit_p(spi_tft_remove),
};

static int spi_tft_init(void)
{
    return spi_register_driver(&spi_tft_drv);
}

static void spi_tft_exit(void)
{
    spi_unregister_driver(&spi_tft_drv);
}

module_init(spi_tft_init);
module_exit(spi_tft_exit);
MODULE_DESCRIPTION("OLED SPI Driver");
MODULE_AUTHOR("weidongshan@qq.com,www.100ask.net");
MODULE_LICENSE("GPL");


