#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-clock.h>
#include <asm/arch/regs-spi.h>
#include <asm/dma.h>

#if defined (CONFIG_CPU_S3C2443)
#include <asm/arch/regs-s3c2443-clock.h>
#elif defined (CONFIG_CPU_S3C2450)
#include <asm/arch/regs-s3c2450-clock.h>
#elif defined (CONFIG_CPU_S3C2416)
#include <asm/arch/regs-s3c2416-clock.h>
#else
# error CPU Clock source is not defined.
#endif

#include "spi-dev.h"
#include "hspi-s3c2443.h"

#if 0
#undef debug
#else
#define debug
#endif

#ifdef debug
#define DBG(x...)       printk(x)
#define DEBUG	printk("%s :: %d\n",__FUNCTION__,__LINE__)

struct s3c_spi_info 
{
    unsigned int reg_base;    
};



static struct spi_master *create_spi_master_s3c2416(int bus_num, unsigned int reg_base_phy)
{
    struct spi_master *master;
    struct s3c_spi_info *info;

	master = spi_alloc_master(NULL, sizeof(struct s3c_spi_info));
	master->bus_num = bus_num;
	master->num_chipselect = 0xffff;
//	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;

	master->setup    = s3c2416_spi_setup;
	master->transfer = s3c2416_spi_transfer;
	master->cleanup = NULL;

	info = spi_master_get_devdata(master);
	info->reg_base = ioremap(reg_base_phy, 0x18);

	spi_register_master(master);

    return master;
}

static int spi_s3c2416_init(void)
{
    spi0_controller = create_spi_master_s3c2416();

    
    return 0;
}

static void spi_s3c2416_exit(void)
{
    destroy_spi_master_s3c2440(spi0_controller);

}




module_init(spi_s3c2416_init);
module_exit(spi_s3c2416_exit);
MODULE_DESCRIPTION("SPI Controller Driver");
MODULE_AUTHOR("weidongshan@qq.com,www.100ask.net");
MODULE_LICENSE("GPL");

