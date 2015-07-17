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

#include <linux/device.h>			/* class_create()的声明 */
#include <linux/cdev.h>			/* cdev_add()的声明 */

/* 1. 确定主设备号 */
static int major;

int hello_open(struct inode *inode, struct file *file)
{
	printk("hello_open\n");
	return 0;
}

int hello2_open(struct inode *inode, struct file *file)
{
	printk("hello2_open\n");
	return 0;
}

/* 2. 构造file_operations */
static struct file_operations hello_fops = {
	.owner = THIS_MODULE,
	.open  = hello_open,
};
static struct file_operations hello2_fops = {
	.owner = THIS_MODULE,
	.open  = hello2_open,
};

#define HELLO_CNT   2

static struct cdev hello_cdev;
static struct cdev hello2_cdev;
static struct class *cls;

static int hello_init(void)
{
	dev_t devid;
	
#if 0	
	major = register_chrdev(0, "hello", &hello_fops); /* (major,  0), (major, 1), ..., (major, 255)都对应hello_fops */
#endif

	/* 根据是否有主设备号，选择调用region注册函数 */
	if (major) 
	{
		devid = MKDEV(major, 0);
		register_chrdev_region(devid, HELLO_CNT, "hello");  /* (major,0~1) 对应 hello_fops, (major, 2~255)都不对应hello_fops */
	} 
	else 
	{
		alloc_chrdev_region(&devid, 0, HELLO_CNT, "hello"); /* (major,0~1) 对应 hello_fops, (major, 2~255)都不对应hello_fops */
		major = MAJOR(devid);                     
	}

	/* 创建chrdev major.0, major.1*/
	cdev_init(&hello_cdev, &hello_fops);
	cdev_add(&hello_cdev, devid, HELLO_CNT);

	/* 注册region，创建chrdev major.2 */
	devid = MKDEV(major, 2);
	register_chrdev_region(devid, 1, "hello2");
	cdev_init(&hello2_cdev, &hello2_fops);
	cdev_add(&hello2_cdev, devid, 1);

	/* 创建设备类和设备节点 */
	cls = class_create(THIS_MODULE, "hello");
	class_device_create(cls, NULL, MKDEV(major, 0), NULL, "hello0"); /* /dev/hello0 */
	class_device_create(cls, NULL, MKDEV(major, 1), NULL, "hello1"); /* /dev/hello1 */
	class_device_create(cls, NULL, MKDEV(major, 2), NULL, "hello2"); /* /dev/hello2 */
	class_device_create(cls, NULL, MKDEV(major, 3), NULL, "hello3"); /* /dev/hello3 */

	return 0;
}

static void hello_exit(void)
{
	class_device_destroy(cls, MKDEV(major, 0));
	class_device_destroy(cls, MKDEV(major, 1));
	class_device_destroy(cls, MKDEV(major, 2));
	class_device_destroy(cls, MKDEV(major, 3));
	class_destroy(cls);

#if 0
	unregister_chrdev(major, "hello");
#endif
	cdev_del(&hello2_cdev);
	unregister_chrdev_region(MKDEV(major, 2), 1);
	
	cdev_del(&hello_cdev);
	unregister_chrdev_region(MKDEV(major, 0), HELLO_CNT);
}


module_init(hello_init);
module_exit(hello_exit);


MODULE_LICENSE("GPL");