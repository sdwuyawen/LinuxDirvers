#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

static struct class *firstdrv_class;
static struct class_device	*firstdrv_class_dev;

volatile unsigned long *gpbcon = NULL;
volatile unsigned long *gpbdat = NULL;

extern int myprintk(const char *fmt, ...);

static int first_drv_open(struct inode *inode, struct file *file)
{
	static int cnt = 0;

	myprintk("first_drv_open,%d\n", cnt++);
	
	*gpbcon &= ~( 0x3<<(1*2) );
	*gpbcon |= ( 0x1<<(1*2) );
	
	return 0;
}

static ssize_t first_drv_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{
	int val;
	static int cnt = 0;

	myprintk("first_drv_write,%d\n", cnt++);

	copy_from_user(&val, buf, count); //	copy_to_user();

	if (val == 1)
	{
		// ���
		*gpbdat &= ~(0x1<<1);
	}
	else
	{
		// ���
		*gpbdat |= (0x1<<1);
	}
	
	return 0;
}

static struct file_operations first_drv_fops = {
    .owner  =   THIS_MODULE,    /* ����һ���꣬�������ģ��ʱ�Զ�������__this_module���� */
    .open   =   first_drv_open,     
	.write	=	first_drv_write,	   
};


int major;
static int first_drv_init(void)
{
	major = register_chrdev(0, "first_drv", &first_drv_fops); // ע��, �����ں�
	
	firstdrv_class = class_create(THIS_MODULE, "firstdrv");

	firstdrv_class_dev = class_device_create(firstdrv_class, NULL, MKDEV(major, 0), NULL, "xyz"); /* /dev/xyz */
	
	gpbcon = (volatile unsigned long *)ioremap(0x56000010, 16);
	gpbdat = gpbcon + 1;

	return 0;
}

static void first_drv_exit(void)
{
	unregister_chrdev(major, "first_drv"); // ж��
	
	class_device_unregister(firstdrv_class_dev);
	class_destroy(firstdrv_class);
}

module_init(first_drv_init);
module_exit(first_drv_exit);


MODULE_LICENSE("GPL");
