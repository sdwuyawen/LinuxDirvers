/****************************************************************************************************************
 * 文件名称	：	DS18B20_drive.c
 * 简介		:	OK6410 DS18B20驱动
 * 作者		：	异灵元（cp1300@139.com）
 * 创建时间	：	2012/09/18 20：37
 * 修改时间	：	2012/09/18
 * 说明		：	OK6410 开发板（S3C6410）DS18B20（GPIO）驱动
 ****************************************************************************************************************/

//系统头文件
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <linux/poll.h>

/* linux/interrupt.h中包含request_irq(),free_irq()的声明 */
#include <linux/interrupt.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/mach/irq.h>

#include <asm/arch/regs-irq.h>
#include <asm/arch/regs-gpio.h>

#include <asm/plat-s3c24xx/cpu.h>
//#include <asm/plat-s3c24xx/pm.h>
#include <asm/plat-s3c24xx/irq.h>

#include <linux/device.h>

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/sysdev.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <linux/miscdevice.h>

#define DS18B20_ERROR	0xf000	//初始化失败
//驱动模块名称
#define DEVICE_NAME "DS18B20"
//系统US延时定义
#define Delay_US(x)		udelay(x)
#define DS18B20_PIN		S3C2443_GPH14

//DS18B20设备访问信号量
struct semaphore DS18B20_sem;
static struct class *class;
static struct class_device *class_dev;
int major;


//函数声明
///////////////////////////////////////////////
static long OK6410_DS18B20_ioctl(
		struct file *file,
		unsigned int cmd,
		unsigned long arg);
static ssize_t OK6410_DS18B20_write(
		struct file *file,
		const char __user *buff,
		size_t size,
		loff_t *loff);
static ssize_t OK6410_DS18B20_read(
		struct file *file,
		char __user *buff,
		size_t size,
		loff_t *loff);
///////////////////////////////////////////////////


//设置DS18B20 IO为推挽输出模式
void Set18b20IOout(void)
{
#if 0
	unsigned int reg;

	reg = readl(S3C64XX_GPECON);
	reg |= 1;
	writel(reg,S3C64XX_GPECON);
#endif
	s3c2410_gpio_cfgpin(DS18B20_PIN, S3C2410_GPIO_OUTPUT);
}


//读IO
unsigned char Read18b20IO(void)
{
#if 0
	unsigned int reg;

	reg = readl(S3C64XX_GPEDAT);
	if(reg & 1)
		return 1;
	else
		return 0;
#endif
	if(s3c2410_gpio_getpin(DS18B20_PIN))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}


//写IO
void Write18b20IO(unsigned char data)
{
#if 0
	unsigned int reg;

	reg = readl(S3C64XX_GPEDAT);
	if(data)
		reg |= 1;
	else
		reg &= ~1;
	writel(reg,S3C64XX_GPEDAT);
#endif
	s3c2410_gpio_setpin(DS18B20_PIN, data);
}


//设在DS18B20 IO输入模式
void Set18b20IOin(void)
{
#if 0
	unsigned int reg;

	reg = readl(S3C64XX_GPECON);
	reg &= (~0xf);
	writel(reg,S3C64XX_GPECON);
#endif

	s3c2410_gpio_cfgpin(DS18B20_PIN, S3C2410_GPIO_INPUT);
}


//复位DS18B20
u8 DS18B20_Reset(void)
{
	u8 i = 0;

	// 配置GPIOB0输出模式
	Set18b20IOout();
	// 向18B20发送一个上升沿，并保持高电平状态约100微秒 
	Write18b20IO(1);
	Delay_US(100);
	// 向18B20发送一个下降沿，并保持低电平状态约600微秒 
	Write18b20IO(0);
	Delay_US(600);
	// 向18B20发送一个上升沿，此时可释放DS18B20总线 
	Write18b20IO(1);
	// 以上动作是给DS18B20一个复位脉冲 
	// 通过再次配置GPIOB1引脚成输入状态，可以检测到DS18B20是否复位成功
	Set18b20IOin();
	Delay_US(15);
	// 若总线在释放后总线状态为高电平，则复位失败
	while(Read18b20IO())
	{
		i ++;
		Delay_US(10);
		if(i > 30)
			return 1;
	}
	Delay_US(100);
	return 0x00;
}


//读DS18B20数据
u8 DS18B20_ReadData(void)
{
	u8 i,data = 0;

	Delay_US(5);
	for(i = 0;i < 8;i ++)
	{
		data >>= 1;
		Set18b20IOout();
		Write18b20IO(1);
		Delay_US(2);
		//开始read slot
		Write18b20IO(0);
		Delay_US(3);
		Write18b20IO(1);
		Set18b20IOin();	
		Delay_US(2);

		if(Read18b20IO())
			data |= 0x80;
		Delay_US(60);
	}
	Delay_US(5);
	return data;
}



//写DS18B20数据
void DS18B20_WriteData(u8 data)
{
	u8 i;

	Delay_US(5);
	Set18b20IOout();
	for(i = 0;i < 8;i ++)
	{
		//开始write slot
		Write18b20IO(0);
		Delay_US(3);
		Write18b20IO(data & 0x01);
		Delay_US(80);
		Write18b20IO(1);
		data >>= 1;
		Delay_US(5);
	}
	Delay_US(5);
}

//读取温度
int DS18B20_ReadTemper(void)
{
	u8 th, tl;
	int data;
	int data7;

	if(DS18B20_Reset())
	{
		return DS18B20_ERROR;
	}
	Delay_US(50);
	DS18B20_WriteData(0xcc);
	DS18B20_WriteData(0x44);
	
	if(DS18B20_Reset())
	{
		return DS18B20_ERROR;
	}
	Delay_US(50);
	DS18B20_WriteData(0xcc);
	DS18B20_WriteData(0xbe);

	Delay_US(50);
	tl = DS18B20_ReadData();
	th = DS18B20_ReadData();

	DS18B20_ReadData();
	DS18B20_ReadData();
	DS18B20_ReadData();
	DS18B20_ReadData();
	DS18B20_ReadData();
	data7 = DS18B20_ReadData();
	printk("data7 , 0x%02x\n", data7);
	
	if(DS18B20_Reset())
	{
		return DS18B20_ERROR;
	}
	data = th;
	data <<= 8;
	data |= tl;
	data = data * 625 / 100;

	return data;
}


/****************************************************************************************************************
*函数名		:	static long OK6410_DS18B20_ioctl(
						struct file *file,
						unsigned int cmd,
						unsigned long arg)
*功能       :	发送命令给DS18B20驱动模块，无实际作用，直接返回0
*参数       :	无作用
*返回       :	0
*依赖       : 	无
*作者       :	异灵元（cp1300@139.com）
*创建时间	:	2012/09/18 20：38
*最后修改时间:	2012/09/18 20：38
*说明		:	无
****************************************************************************************************************/
static long OK6410_DS18B20_ioctl(
		struct file *file,
		unsigned int cmd,
		unsigned long arg)
{
	return 0;
}


/****************************************************************************************************************
*函数名		:	static ssize_t OK6410_DS18B20_write(
						struct file *file,
						const char __user *buff,
						size_t size,
						loff_t *loff)
*功能       :	写数据到DS18B20驱动模块，无效函数，返回0
*参数       :	file：文件指针（无作用）；buff：数据缓冲区指针；buff：数据数量；loff:无作用
*返回       :	0：成功;<0:失败
*依赖       : 	linux底层宏
*作者       :	异灵元（cp1300@139.com）
*创建时间	:	2012/09/18 20：38
*最后修改时间:	2012/09/18 20：38
*说明		:	写无效
****************************************************************************************************************/
static ssize_t OK6410_DS18B20_write(
		struct file *file,
		const char __user *buff,
		size_t size,
		loff_t *loff)
{

	return 0;
}


/****************************************************************************************************************
*函数名		:	static ssize_t OK6410_DS18B20_read(
						struct file *file,
						char __user *buff,
						size_t size,
						loff_t *loff)
*功能       :	读DS18B20状态，低电平灯亮
*参数       :	file：文件指针（无作用）；buff：数据缓冲区指针；buff：数据数量；loff:无作用
*返回       :	0：成功;<0:失败
*依赖       : 	linux底层宏
*作者       :	异灵元（cp1300@139.com）
*创建时间	:	2012/09/18 20：38
*最后修改时间:	2012/09/18 20：38
*说明		:	读取到的是一个16位有符号的温度
****************************************************************************************************************/
static ssize_t OK6410_DS18B20_read(
		struct file *file,
		char __user *buff,
		size_t size,
		loff_t *loff)
{
	int temp;
	int ret;

	if(down_interruptible(&DS18B20_sem))		//获取信号量
		return -ERESTARTSYS;
	temp = DS18B20_ReadTemper();			//读取温度
	if(temp == DS18B20_ERROR)				//DS18B20初始化失败
	{
		up(&DS18B20_sem);							//释放信号量
		return -1;							//DS18B20读取失败，返回错误
	}

	ret = copy_to_user(buff, &temp, size);			//将温度写入用户空间							

	up(&DS18B20_sem);							//释放信号量

	return 0;
}


/*	这个结构是字符设备驱动的核心
*	当应用程序操作设备文件所提供的open,read,write等函数，
*	最终会调用到这个结构中的对应函数
*/
static struct file_operations dev_fops = {
		.owner				= THIS_MODULE,		//这是一个宏，指向编译模块时自动创建的__this_module变量
		.unlocked_ioctl 		= OK6410_DS18B20_ioctl,
		.read				= OK6410_DS18B20_read,
		.write				= OK6410_DS18B20_write,
};

/****************************************************************************************************************
*函数名		:	static int  __init OK6410_DS18B20_init(void)
*功能       :	DS18B20模块初始化函数
*参数       :	无
*返回       :	0：成功；<0:失败
*依赖       : 	linux底层宏定义
*作者       :	异灵元（cp1300@139.com）
*创建时间	:	2012/09/18 20：38
*最后修改时间:	2012/09/18 20：38
*说明		:	初始化DS18B20硬件，注册DS18B20驱动
****************************************************************************************************************/
static int  __init OK6410_DS18B20_init(void)
{
	/* 指定主设备号为BUTTON_MAJOR，返回0表示成功 */
	major = register_chrdev(0, DEVICE_NAME, &dev_fops);
	
	class = class_create(THIS_MODULE, DEVICE_NAME);
	class_dev = class_device_create(class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME); /* /dev/buttons */

	init_MUTEX(&DS18B20_sem);			//注册信号量
	printk(DEVICE_NAME " initialized\n");
	return 0;							//返回成功
}

/****************************************************************************************************************
*函数名		:	static void __exit OK6410_DS18B20_exit(void)
*功能       :	卸载DS18B20驱动
*参数       :	无
*返回       :	无
*依赖       : 	linux底层宏
*作者       :	异灵元（cp1300@139.com）
*创建时间	:	2012/09/18 20：38
*最后修改时间:	2012/09/18 20：38
*说明		:	卸载驱动
****************************************************************************************************************/
static void __exit OK6410_DS18B20_exit(void)
{
	unregister_chrdev(major, DEVICE_NAME);
	class_device_unregister(class_dev);
//	class_device_destroy(sixthdrv_class, MKDEV(major, 0));
	class_destroy(class);
}



//动态加载驱动接口（必须）
module_init(OK6410_DS18B20_init);
module_exit(OK6410_DS18B20_exit);
//其它信息（非必需）
MODULE_AUTHOR("sdwuyawen@126.com");						//驱动程序作者
MODULE_DESCRIPTION("OK6410(S3C6410) DS18B20 Driver");	//一些描述信息
MODULE_LICENSE("GPL");	//遵循的协议




