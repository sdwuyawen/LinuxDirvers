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

/* 分配/设置/注册一个platform_driver */

static int led_probe(struct platform_device *pdev)
{
	/* 根据platform_device的资源进行ioremap */

	/* 注册字符设备驱动程序 */

	printk("led_probe, found led\n");

	return 0;
}

static int led_remove(struct platform_device *pdev)
{
	/* 卸载字符设备驱动程序 */
	
	/* iounmap */

	printk("led_remove, remove led\n");

	return 0;
}

struct platform_driver led_drv = {
	.probe		= led_probe,
	.remove		= led_remove,
	.driver		= {
	.name	= "myled",
	}
};


static int led_drv_init(void)
{
	platform_driver_register(&led_drv);
	return 0;
}

static void led_drv_exit(void)
{
	platform_driver_unregister(&led_drv);
}

module_init(led_drv_init);
module_exit(led_drv_exit);

MODULE_LICENSE("GPL");
