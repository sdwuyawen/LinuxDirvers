/* 参考drivers\input\keyboard\gpio_keys.c */

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
/* 新增加的 */
/* asm/plat-s3c24xx/pm.h中struct sys_device结构在linux/sysdev.h中定义 */
#include <linux/sysdev.h>
/* 新增加 结束 */

#include <asm/gpio.h>


#include <linux/kernel.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
/* linux/interrupt.h中包含request_irq(),free_irq()的声明 */
#include <linux/interrupt.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach/irq.h>
#include <asm/arch/regs-irq.h>
#include <asm/arch/regs-gpio.h>
#include <asm/plat-s3c24xx/cpu.h>
#include <asm/plat-s3c24xx/pm.h>
#include <asm/plat-s3c24xx/irq.h>

struct pin_desc{
	int irq;
	char *name;
	unsigned int pin;
	unsigned int key_val;
};

static struct input_dev *buttons_dev;
/* 定时器 */
static struct timer_list buttons_timer;
static struct pin_desc *irq_pd = 0;

struct pin_desc pins_desc[5] = {
	{IRQ_EINT0, "S1", S3C2410_GPF0, KEY_L},
	{IRQ_EINT1, "S2", S3C2410_GPF1, KEY_S},
	{IRQ_EINT2, "S3", S3C2410_GPF2, KEY_ENTER},
	{IRQ_EINT3, "S4", S3C2410_GPF3, KEY_LEFTSHIFT},
	{IRQ_EINT4, "S5", S3C2410_GPF4, KEY_A},
};

volatile unsigned long *gpfcon;
volatile unsigned long *gpfdat;
volatile unsigned long *gpfudp;

/*
  * 确定按键值
  */
static irqreturn_t buttons_irq(int irq, void *dev_id)
{
	/* 10ms后启动定时器 */
	irq_pd = (struct pin_desc *)dev_id;
	mod_timer(&buttons_timer, jiffies+HZ/100);
		
	return IRQ_RETVAL(IRQ_HANDLED);
}

static void buttons_timer_function(unsigned long data)
{
	struct pin_desc * pindesc = irq_pd;
	unsigned int pinval;

	if (!pindesc)	/* 还未进入按键中断时，已经进入了定时器超时处理 */
		return;
	
	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if (pinval)
	{
		/* 松开 : 最后一个参数: 0-松开, 1-按下 */
		input_event(buttons_dev, EV_KEY, pindesc->key_val, 0);
		input_sync(buttons_dev);
	}
	else
	{
		/* 按下 */
		input_event(buttons_dev, EV_KEY, pindesc->key_val, 1);
		input_sync(buttons_dev);
	}
}


static int buttons_init(void)
{
	int i;
	unsigned long pend;
	
	/* 1. 分配一个input_dev结构体 */
	buttons_dev = input_allocate_device();
	if (!buttons_dev)
		return -ENOMEM;

	/* 2. 设置 */
	/* 2.1 能产生哪类事件 */
	set_bit(EV_KEY, buttons_dev->evbit);
	/* 支持REP重复类事件 */
	set_bit(EV_REP, buttons_dev->evbit);

	/* 2.2 能产生这类操作里的哪些事件: L,S,ENTER,LEFTSHIT */
	set_bit(KEY_L, buttons_dev->keybit);
	set_bit(KEY_S, buttons_dev->keybit);
	set_bit(KEY_ENTER, buttons_dev->keybit);
	set_bit(KEY_LEFTSHIFT, buttons_dev->keybit);
	set_bit(KEY_A, buttons_dev->keybit);

	/* 3. 注册 */
	input_register_device(buttons_dev);

	/* 4. 硬件相关的操作 */
	
	/* 初始化定时器 */
	init_timer(&buttons_timer);
	buttons_timer.function = buttons_timer_function;
	buttons_timer.expires  = 0;		/* 超时时间是0，立刻会进入定时器处理函数 */
	add_timer(&buttons_timer); 

	gpfcon = (volatile unsigned long *)ioremap(0x56000050, 16);
	gpfdat = gpfcon + 1;
	gpfudp = gpfdat + 1;

	/* 配置内部上拉 */
	*gpfudp &= ~((0x3<<(0*2)) | (0x3<<(1*2)) | (0x3<<(2*2)) | (0x3<<(3*2)) | (0x3<<(4*2)));
	*gpfudp |= ((0x2<<(0*2)) | (0x2<<(1*2)) | (0x2<<(2*2)) | (0x2<<(3*2)) | (0x2<<(4*2)));

	/* 开机后，EINT3会多一次上升沿中断，所以在open里清除EINT0-EINT3的SRCPND */
	pend = __raw_readl(S3C2410_SRCPND);
	printk("S3C2410_SRCPND=0x%08x\n", (unsigned int)pend);
	
	__raw_writel(pend & 0x0f, S3C2410_SRCPND);

	pend = __raw_readl(S3C2410_SRCPND);
	printk("S3C2410_SRCPND=0x%08x\n", (unsigned int)pend);
	
	for (i = 0; i < 5; i++)
	{
		request_irq(pins_desc[i].irq, buttons_irq, IRQT_BOTHEDGE, pins_desc[i].name, &pins_desc[i]);
	}

	return 0;
}

static void buttons_exit(void)
{
	int i;
	for (i = 0; i < 5; i++)
	{
		free_irq(pins_desc[i].irq, &pins_desc[i]);
	}

	del_timer(&buttons_timer);
	input_unregister_device(buttons_dev);
	input_free_device(buttons_dev);	
	
	iounmap(gpfcon);
}


module_init(buttons_init);
module_exit(buttons_exit);

MODULE_LICENSE("GPL");
