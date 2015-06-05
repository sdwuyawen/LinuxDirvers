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


#define CMD_AAI       		0xAD  		/* AAI 连续编程指令(FOR SST25VF016B) */
#define CMD_DISWR	  	0x04		/* 禁止写, 退出AAI状态 */
#define CMD_EWRSR	  	0x50		/* 允许写状态寄存器的命令 */
#define CMD_WRSR      	0x01  		/* 写状态寄存器命令 */
#define CMD_WRDISEN	0x04		/* 禁止写使能命令 */
#define CMD_WREN      	0x06		/* 写使能命令 */
#define CMD_READ      	0x03  		/* 读数据区命令 */
#define CMD_RDSR1      	0x05		/* 读状态寄存器1命令 */
#define CMD_RDSR2      	0x35		/* 读状态寄存器2命令 */
#define CMD_RDID      	0x9F		/* 读器件ID命令 */
#define CMD_SE        		0x20		/* 擦除扇区命令 */
#define CMD_BE        		0xC7		/* 批量擦除命令 */
#define CMD_PAGE_PROGRAM	0x02	/* 页写入 */
#define DUMMY_BYTE    	0xA5		/* 哑命令，可以为任意值，用于读操作 */

#define WIP_FLAG      	0x01		/* 状态寄存器中的正在编程标志（WIP) */


/* 构造注册 spi_driver */
static struct spi_device *spi_flash;


/* 参考:
 * drivers/mtd/devices/mtdram.c
 * drivers/mtd/devices/m25p80.c
 */

static void spi_flash_read_id(unsigned int *pMID, unsigned int *pDID)
{
	unsigned char tx_buf[1] = {0};
	unsigned char rx_buf[3] = {0};

	tx_buf[0] = CMD_RDID;

	spi_write_then_read(spi_flash, tx_buf, 1, rx_buf, 3);

	*pMID = rx_buf[0];
	*pDID = rx_buf[1];
	*pDID = (*pDID << 8) | rx_buf[2];
}


/* 在找到对应名字的spi_device时，会调用spi_driver的probe函数 */
static int __devinit spi_flash_probe(struct spi_device *spi)
{
	unsigned int MID, DID;
	spi_flash = spi;

	printk("%s\n", __FUNCTION__);
	
//	spi_tft_rs_pin = (int)spi->dev.platform_data;						/* spi->dev指向spi_board_info，在spi_new_device()里被注册 */
	s3c2410_gpio_setpin(spi->chip_select, 1);	
	s3c2410_gpio_cfgpin(spi->chip_select, S3C2410_GPIO_OUTPUT);		/* spi->chip_select指向spi_board_info的chip_select，在spi_new_device()里被注册 */

	spi_flash_read_id(&MID, &DID);

	printk("MID = %02x\n", MID);	
	printk("DID = %04x\n", DID);

	printk("%s\n", __FUNCTION__);
	
//	printk("spi_tft_rs_pin = %08x\n", spi_tft_rs_pin);					/* 0xa7 == S3C2410_GPF7 */
//	printk("spi->chip_select = %08x\n", spi->chip_select);					/* 0xea == S3C2410_GPH10 */
	
    return 0;
}


static int __devexit spi_flash_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver spi_flash_drv = {
	.driver = {
		.name	= "spigpio_flash",
		.owner	= THIS_MODULE,
	},
	.probe		= spi_flash_probe,
	.remove		= __devexit_p(spi_flash_remove),
};

static int spi_flash_init(void)
{
    return spi_register_driver(&spi_flash_drv);
}

static void spi_flash_exit(void)
{
    spi_unregister_driver(&spi_flash_drv);
}

module_init(spi_flash_init);
module_exit(spi_flash_exit);
MODULE_DESCRIPTION("OLED SPI Driver");
MODULE_AUTHOR("weidongshan@qq.com,www.100ask.net");
MODULE_LICENSE("GPL");
