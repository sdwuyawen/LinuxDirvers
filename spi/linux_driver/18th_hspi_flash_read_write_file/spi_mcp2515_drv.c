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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/mtd/compatmac.h>
#include <linux/mtd/mtd.h>

#include <linux/sched.h>

/* 构造注册 spi_driver */
static struct spi_device *spi_mcp2515;
unsigned int spi_mcp2515_reset_pin;

static unsigned char spi_mcp2515_reg(unsigned char addr)
{
	unsigned char tx_buf[2] = {0x03, addr};
	unsigned char rx_buf[1] = {0};

	spi_write_then_read(spi_mcp2515, tx_buf, 2, rx_buf, 1);

	return rx_buf[0];
}



/* 在找到对应名字的spi_device时，会调用spi_driver的probe函数 */
static int __devinit spi_mcp2515_probe(struct spi_device *spi)
{
	unsigned char byte;
	
	spi_mcp2515 = spi;

	printk("%s\n", __FUNCTION__);
	
	spi_mcp2515_reset_pin = (int)spi->dev.platform_data;				/* spi->dev指向spi_board_info，在spi_new_device()里被注册 */
	s3c2410_gpio_setpin(spi_mcp2515_reset_pin, 1);	
	s3c2410_gpio_cfgpin(spi_mcp2515_reset_pin, S3C2410_GPIO_OUTPUT);		/* spi->chip_select指向spi_board_info的chip_select，在spi_new_device()里被注册 */
	s3c2410_gpio_setpin(spi_mcp2515_reset_pin, 0);	
	msleep(100);
	s3c2410_gpio_setpin(spi_mcp2515_reset_pin, 1);	

	printk("spi->chip_select = %x\r\n", spi->chip_select);
//	s3c2410_gpio_setpin(spi->chip_select, 1);	
//	s3c2410_gpio_cfgpin(spi->chip_select, S3C2410_GPIO_OUTPUT);		/* spi->chip_select指向spi_board_info的chip_select，在spi_new_device()里被注册 */

	/* 读取flash的MID和DID */
	byte = spi_mcp2515_reg(0x0E);
	printk("mcp2515 reg 0x0E = %02x\n", byte);
	byte = spi_mcp2515_reg(0x0F);
	printk("mcp2515 reg 0x0F = %02x\n", byte);

	byte = spi_mcp2515_reg(0x0E);
	printk("mcp2515 reg 0x0E = %02x\n", byte);
	byte = spi_mcp2515_reg(0x0F);
	printk("mcp2515 reg 0x0F = %02x\n", byte);

	byte = spi_mcp2515_reg(0x0E);
	printk("mcp2515 reg 0x0E = %02x\n", byte);
	byte = spi_mcp2515_reg(0x0F);
	printk("mcp2515 reg 0x0F = %02x\n", byte);
	
	return 0;
}


static int __devexit spi_mcp2515_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver spi_mcp2515_drv = {
	.driver = {
		.name	= "hspi_mcp2515",
		.owner	= THIS_MODULE,
	},
	.probe		= spi_mcp2515_probe,
	.remove		= __devexit_p(spi_mcp2515_remove),
};

static int spi_mcp2515_init(void)
{
    return spi_register_driver(&spi_mcp2515_drv);
}

static void spi_mcp2515_exit(void)
{
    spi_unregister_driver(&spi_mcp2515_drv);
}

module_init(spi_mcp2515_init);
module_exit(spi_mcp2515_exit);
MODULE_DESCRIPTION("OLED SPI Driver");
MODULE_AUTHOR("weidongshan@qq.com,www.100ask.net");
MODULE_LICENSE("GPL");
