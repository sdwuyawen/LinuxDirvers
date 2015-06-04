#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <sound/core.h>
#include <linux/spi/spi.h>


/* 构造注册 spi_driver */

static int major;
static struct class *class;

static long oled_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return 0;
}

static ssize_t oled_write(struct file *file,
				      const char __user *buf,
				      size_t count, loff_t *ppos)
{
    return 0;
}


static struct file_operations oled_ops = {
	.owner            = THIS_MODULE,
	.unlocked_ioctl   = oled_ioctl,
	.write            = oled_write,
};

static int __devinit spi_oled_probe(struct spi_device *spi)
{
	/* 注册一个 file_operations */
	major = register_chrdev(0, "oled", &oled_ops);

	class = class_create(THIS_MODULE, "oled");

	/* 为了让mdev根据这些信息来创建设备节点 */
	device_create(class, NULL, MKDEV(major, 0), NULL, "oled"); /* /dev/oled */
    
    
    return 0;
}

static int __devexit spi_oled_remove(struct spi_device *spi)
{

	device_destroy(class, MKDEV(major, 0));
	class_destroy(class);
	unregister_chrdev(major, "oled");

	return 0;
}


static struct spi_driver spi_oled_drv = {
	.driver = {
		.name	= "oled",
		.owner	= THIS_MODULE,
	},
	.probe		= spi_oled_probe,
	.remove		= __devexit_p(spi_oled_remove),
};

static int spi_oled_init(void)
{
    return spi_register_driver(&spi_oled_drv);
}

static void spi_oled_exit(void)
{
    spi_unregister_driver(&spi_oled_drv);
}

module_init(spi_oled_init);
module_exit(spi_oled_exit);
MODULE_DESCRIPTION("OLED SPI Driver");
MODULE_AUTHOR("weidongshan@qq.com,www.100ask.net");
MODULE_LICENSE("GPL");


