/* spi-s3c2416.c
 *
 * Copyright (C) 2006 Samsung Electronics Co. Ltd.
 *
 * S3C2443 SPI Controller
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

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

#include <asm/arch/regs-s3c2416-clock.h>

#include <linux/spi/spi.h>
#include <asm/arch/spi-gpio.h>
#include <asm/arch/regs-irq.h>


//#include "spi-dev.h"
#include "hspi-s3c2416.h"

#if 0

#if 0
#undef debug
#else
#define debug
#endif

#ifdef debug
#define DBG(x...)       printk(x)
#define DEBUG	printk("%s :: %d\n",__FUNCTION__,__LINE__)
void print_reg(struct s3c_spi *spi)
{
	printk("CH_CFG 	= 0x%08x\n",readl(spi->regs + S3C_CH_CFG));
	printk("CLK_CFG = 0x%08x\n",readl(spi->regs + S3C_CLK_CFG));
	printk("MODE_CFG = 0x%08x\n",readl(spi->regs + S3C_MODE_CFG));
	printk("SLAVE_CFG = 0x%08x\n",readl(spi->regs + S3C_SLAVE_SEL));
	printk("INT_EN 	= 0x%08x\n",readl(spi->regs + S3C_SPI_INT_EN));
	printk("SPI_STATUS = 0x%08x\n",readl(spi->regs + S3C_SPI_STATUS));
	printk("PACKET_CNT = 0x%08x\n",readl(spi->regs + S3C_PACKET_CNT));
	printk("PEND_CLR = 0x%08x\n",readl(spi->regs + S3C_PENDING_CLR));
}
#else
#define DEBUG
#define DBG(x...)       do { } while (0)
void print_reg(struct s3c_spi *spi)
{
}
#endif

static void s3c_spi_free(struct s3c_spi *spi)
{
	DEBUG;

	if (spi->clk != NULL && !IS_ERR(spi->clk)) {
		clk_disable(spi->clk);
		clk_put(spi->clk);
		spi->clk = NULL;
	}

	if (spi->regs != NULL) {
		iounmap(spi->regs);
		spi->regs = NULL;
	}

	if (spi->ioarea != NULL) {
		release_resource(spi->ioarea);
		kfree(spi->ioarea);
		spi->ioarea = NULL;
	}
}

static int s3c_spi_hw_init(struct s3c_spi *spi)
{
	/* program defaults into the registers */
	writel(readl(S3C2443_SCLKCON)|(1<<14), S3C2443_SCLKCON);
	writel(readl(S3C2443_PCLKCON)|(1<<6), S3C2443_PCLKCON);

	writel(readl(S3C24XX_MISCCR)|S3C24XX_MISCCR_SPISEL, S3C24XX_MISCCR);

#ifdef CONFIG_SPICLK_PCLK
	/*Enable PCLK into the HS SPI*/
	writel(readl(S3C2443_PCLKCON)|(1<<6), S3C2443_PCLKCON);

	clk_enable(spi->clk);

#elif defined CONFIG_SPICLK_EPLL
	/* implemetation when use EPLL clock */
	writel(0x800, S3C2443_LOCKCON1);
	writel( (readl( S3C2443_CLKSRC ) | (1 << 6) ), S3C2443_CLKSRC);  // EPLL Output

	writel((48<<16)|(3<<8)|(1<<0) ,S3C2443_EPLLCON);//96MHz

	writel( readl(S3C2443_EPLLCON)& (~(1<<24)) , S3C2443_EPLLCON );  //EPLL On
	writel(( readl(S3C2443_CLKDIV1) & (~(0x3<<24))) | (0x0 << 24) , S3C2443_CLKDIV1 ); // Epll Ratio is 1
	writel(( readl(S3C24XX_MISCCR) & (~(0x7<<8))) | (0x1 << 8) , S3C24XX_MISCCR );

	/* Epll, prescaler = 1					*/
	/* clock =  ( clock source / (2 * ( prescaler + 1)))	*/
	writel(0x501, spi->regs + S3C_CLK_CFG); 	//Use EPLL Clock and Clock On Prescaler 1
#else
#error you must define correct confige file.
#endif

	if (SPI_CHANNEL == 0){
	/* initialize the gpio */
		s3c2410_gpio_cfgpin(S3C2410_GPE11, S3C2410_GPE11_SPIMISO0);
		s3c2410_gpio_cfgpin(S3C2410_GPE12, S3C2410_GPE12_SPIMOSI0);
		s3c2410_gpio_cfgpin(S3C2410_GPE13, S3C2410_GPE13_SPICLK0);

		s3c2410_gpio_cfgpin(S3C2410_GPL13, S3C2410_GPL13_SS0);
	}
	else
	{
		s3c2410_gpio_cfgpin(S3C2410_GPL14, S3C2410_GPL14_SS1);

		s3c2410_gpio_cfgpin(S3C2410_GPL11, S3C2410_GPL11_SPIMOSI1);
		s3c2410_gpio_cfgpin(S3C2410_GPL12, S3C2410_GPL12_SPIMISO1);
		s3c2410_gpio_cfgpin(S3C2410_GPL10, S3C2410_GPL10_SPICLK1);
	}

	/* hspi software restet */
#if 0
	writeb(readb(spi->regs + S3C_CH_CFG) | (1<<5), spi->regs + S3C_CH_CFG);
	writeb(readb(spi->regs + S3C_CH_CFG) & (~(1<<5)), spi->regs + S3C_CH_CFG);
#endif
	DEBUG;

	return 0;
}

static int s3c_spi_dma_init(struct s3c_spi *spi, int mode)
{
	DEBUG;
	// TX
	if(mode == 0) {
		s3c2410_dma_devconfig(spi->dma, S3C2410_DMASRC_MEM, S3C_SPI_DMA_HWCFG, S3C_SPI_TX_DATA_REG);
	}
	// RX
	else if(mode == 1) {
		s3c2410_dma_devconfig(spi->dma, S3C2410_DMASRC_HW, S3C_SPI_DMA_HWCFG, S3C_SPI_RX_DATA_REG);
	}
/* Our BSP only support BYTE mode */
	s3c2410_dma_config(spi->dma, S3C_DMA_XFER_BYTE, S3C_DCON_SPI1);
	s3c2410_dma_setflags(spi->dma, S3C2410_DMAF_AUTOSTART);

	return 0;
}

static inline void s3c_spi_write_fifo(struct s3c_spi *spi)
{
	u32 wdata = 0;

	if (spi->msg->wbuf) {
		wdata = spi->msg->wbuf[spi->msg_ptr++];
	} else {
		spi->msg_ptr++;
		wdata = 0xff;
	}

	DBG("wdata = %x\n",wdata);
	writel(wdata, spi->regs + S3C_SPI_TX_DATA);
}

/* s3c_spi_master_complete
 *
 * complete the message and wake up the caller, using the given return code,
 * or zero to mean ok.
*/
static inline void s3c_spi_master_complete(struct s3c_spi *spi, int ret)
{
	DEBUG;

	spi->msg_ptr = 0;
	spi->msg_rd_ptr = 0;
	spi->msg->flags = 0;
	spi->msg = NULL;
	spi->msg_idx ++;
	spi->msg_num = 0;

	writel(0xff, spi->regs + S3C_PENDING_CLR);


	if (ret)
		spi->msg_idx = ret;
}

static int s3c_spi_done(struct s3c_spi *spi)
{
	u32 spi_clkcfg;
	DEBUG;

	if (SPI_CHANNEL == 0){
	/* initialize the gpio */
		s3c2410_gpio_cfgpin(S3C2410_GPE11, 0);
		s3c2410_gpio_cfgpin(S3C2410_GPE12, 0);
		s3c2410_gpio_cfgpin(S3C2410_GPE13, 0);

		s3c2410_gpio_cfgpin(S3C2410_GPL13, 0);
	}else{
		s3c2410_gpio_cfgpin(S3C2410_GPL14, 0);

		s3c2410_gpio_cfgpin(S3C2410_GPL11, 0);
		s3c2410_gpio_cfgpin(S3C2410_GPL12, 0);
		s3c2410_gpio_cfgpin(S3C2410_GPL10, 0);
	}

	spi_clkcfg = readl( spi->regs + S3C_CLK_CFG);
	spi_clkcfg &= SPI_ENCLK_DISABLE;
	writel( spi_clkcfg , spi->regs + S3C_CLK_CFG);

	return 0;

}

static inline void s3c_spi_stop(struct s3c_spi *spi, int ret)
{
	DEBUG;

	writel(0x0, spi->regs + S3C_SPI_INT_EN);
	writel(0x1f, spi->regs + S3C_PENDING_CLR);
	writel(0x0, spi->regs + S3C_CH_CFG);

	s3c_spi_done(spi);
	spi->state = STATE_IDLE;
	s3c_spi_master_complete(spi, ret);
	print_reg(spi);
        up(&spi->sem);
}

void s3c_spi_dma_cb(struct s3c2410_dma_chan *dma_ch, void *buf_id,
        int size, enum s3c2410_dma_buffresult result)
{
	struct s3c_spi *spi = (struct s3c_spi *)buf_id;
	unsigned long status = 0;
	DEBUG;

	status = readl(spi->regs + S3C_SPI_STATUS);

	pr_debug("DMA call back\n");

	if (spi->msg->wbuf)
		while (!(readl(spi->regs +S3C_SPI_STATUS) & SPI_STUS_TX_DONE)) {}

	s3c_spi_stop(spi, status);
}

/* s3c_spi_message_start
 *
 * configure the spi controler and transmit start of a message onto the bus
*/
static void s3c_spi_message_start(struct s3c_spi *spi)
{
	struct spi_msg *msg = spi->msg;

	u32 spi_chcfg = 0, spi_slavecfg, spi_inten= 0, spi_packet=0;

	u8 prescaler = 5;
	u32 spi_clkcfg = 0, spi_modecfg = 0 ;

	DEBUG;

	/* initialise the spi controller */
	s3c_spi_hw_init(spi);

	/* 1. Set transfer type (CPOL & CPHA set) */
	spi_chcfg = SPI_CH_RISING | SPI_CH_FORMAT_A;

	if (spi->msg->flags & SPI_M_MODE_MASTER) {
		spi_chcfg |= SPI_CH_MASTER;
	} else if(spi->msg->flags & SPI_M_MODE_SLAVE){
		spi_chcfg |= SPI_CH_SLAVE;
	}

	writel( spi_chcfg , spi->regs + S3C_CH_CFG);

	/* 2. Set clock configuration register */
	spi_clkcfg = SPI_ENCLK_ENABLE;

#if defined CONFIG_SPICLK_PCLK
	spi_clkcfg |= SPI_CLKSEL_PCLK;
#elif defined CONFIG_SPICLK_EPLL
	spi_clkcfg |= SPI_CLKSEL_ECLK;
#else
#error you must define correct confige file.
#endif
	writel( spi_clkcfg , spi->regs + S3C_CLK_CFG);

	spi_clkcfg = readl( spi->regs + S3C_CLK_CFG);

	/* SPI clockout = clock source / (2 * (prescaler +1)) */
	spi_clkcfg |= prescaler;
	writel( spi_clkcfg , spi->regs + S3C_CLK_CFG);

	/* 3. Set SPI MODE configuration register */
#ifdef CONFIG_WORD_TRANSIZE
	spi_modecfg = SPI_MODE_CH_TSZ_WORD;
#else
	spi_modecfg = SPI_MODE_CH_TSZ_BYTE;
#endif
	spi_modecfg |= SPI_MODE_TXDMA_OFF| SPI_MODE_SINGLE| SPI_MODE_RXDMA_OFF;

	if (msg->flags & SPI_M_DMA_MODE) {
		spi_modecfg |= SPI_MODE_TXDMA_ON| SPI_MODE_RXDMA_ON;
	}

	if (msg->wbuf)
		spi_modecfg |= ( 0x3f << 5); /* Tx FIFO trigger level in INT mode */
	if (msg->rbuf)
		spi_modecfg |= ( 0x3f << 11); /* Rx FIFO trigger level in INT mode */

	spi_modecfg |= ( 0x3ff << 19);
	writel(spi_modecfg, spi->regs + S3C_MODE_CFG);

	/* 4. Set SPI INT_EN register */

	if (msg->wbuf)
		spi_inten = SPI_INT_TX_FIFORDY_EN|SPI_INT_TX_UNDERRUN_EN|SPI_INT_TX_OVERRUN_EN;
	if (msg->rbuf){
		spi_inten = SPI_INT_RX_FIFORDY_EN|SPI_INT_RX_UNDERRUN_EN|SPI_INT_RX_OVERRUN_EN|SPI_INT_TRAILING_EN;
	}
	writel(spi_inten, spi->regs + S3C_SPI_INT_EN);

	writel(0x1f, spi->regs + S3C_PENDING_CLR);

	/* 5. Set Packet Count configuration register */
	spi_packet = SPI_PACKET_CNT_EN;
	spi_packet |= 0xffff;
	writel(spi_packet, spi->regs + S3C_PACKET_CNT);

	/* 6. Set Tx or Rx Channel on */
	spi_chcfg = readl(spi->regs + S3C_CH_CFG);

	spi_chcfg |= SPI_CH_TXCH_OFF | SPI_CH_RXCH_OFF;

	if (msg->wbuf)
		spi_chcfg |= SPI_CH_TXCH_ON;
	if (msg->rbuf)
		spi_chcfg |= SPI_CH_RXCH_ON;

	writel(spi_chcfg, spi->regs + S3C_CH_CFG);

	if (msg->flags & SPI_M_DMA_MODE) {

		if (msg->wbuf)
			spi->dma = DMACH_SPI_TX;
		if (msg->rbuf)
			spi->dma = DMACH_SPI_RX;

		if (s3c2410_dma_request(spi->dma, &s3c2443spi_dma_client, NULL)) {
			printk(KERN_WARNING  "unable to get DMA channel.\n" );
		}

		s3c2410_dma_set_buffdone_fn(spi->dma, s3c_spi_dma_cb);
		s3c2410_dma_set_opfn(spi->dma, NULL);

		if (msg->wbuf)
			s3c_spi_dma_init(spi, 0);
		if (msg->rbuf)
			s3c_spi_dma_init(spi, 1);

		s3c2410_dma_enqueue(spi->dma, (void *) spi, spi->dmabuf_addr, spi->msg->len);
	}
	/* 7. Set nSS low to start Tx or Rx operation */
	spi_slavecfg = readl(spi->regs + S3C_SLAVE_SEL);
	spi_slavecfg &= SPI_SLAVE_SIG_ACT;
	spi_slavecfg |= (0x3f << 4);
	writel(spi_slavecfg, spi->regs + S3C_SLAVE_SEL);

	print_reg(spi);
}

/* is_msgend
 *
 * returns TRUE if we reached the end of the current message
*/

static inline int tx_msgend(struct s3c_spi *spi)
{
	return spi->msg_ptr >= spi->msg->len;
}

static inline int rx_msgend(struct s3c_spi *spi)
{
	return spi->msg_rd_ptr >= spi->msg->len;
}

/* spi_s3c_irq_nextbyte
 *
 * process an interrupt and work out what to do
 */
static void spi_s3c_irq_nextbyte(struct s3c_spi *spi, unsigned long spsta)
{
	DEBUG;
	DBG("spi->state = %d \n",spi->state);
	switch (spi->state) {
	case STATE_IDLE:
			DBG("%s: called in STATE_IDLE\n", __FUNCTION__);
		break;

	case STATE_STOP:
		udelay(200);
		s3c_spi_stop(spi, 0);
			DBG("%s: called in STATE_STOP\n", __FUNCTION__);
		break;

	case STATE_XFER_TX:
		DEBUG;
		print_reg(spi);
		DBG("msg_ptr = 0x%x, len = 0x%x \n", spi->msg_ptr ,spi->msg->len);
		while(!(tx_msgend(spi)))
		s3c_spi_write_fifo(spi);
		DEBUG;
		print_reg(spi);
		spi->state = STATE_STOP;
		break;

	case STATE_XFER_RX:
		DEBUG;
		print_reg(spi);
		DBG("msg_rd_ptr = 0x%x, len = 0x%x \n", spi->msg_rd_ptr ,spi->msg->len);
		while(!(rx_msgend(spi))){
			spi->msg->rbuf[spi->msg_rd_ptr++] = readl(spi->regs + S3C_SPI_RX_DATA);
			DBG("msg_rd_ptr = 0x%x, len = 0x%x \n", spi->msg_rd_ptr ,spi->msg->len);
			DBG("msg_rbuf = 0x%x\n", spi->msg->rbuf[spi->msg_rd_ptr - 1]);
		}
		DBG("msg_rd_ptr = 0x%x, len = 0x%x \n", spi->msg_rd_ptr ,spi->msg->len);
		DEBUG;
		print_reg(spi);
		s3c_spi_stop(spi, 0);
		break;

	default:
		dev_err(spi->dev, "%s: called with Invalid option\n", __FUNCTION__);
	}

	return;
}

/* s3c_spi_irq
 *
 * top level IRQ servicing routine
*/
static irqreturn_t s3c_spi_irq(int irqno, void *dev_id)
{
	struct s3c_spi *spi = dev_id;
	unsigned long spi_sts;

	DEBUG;

	spi_sts = readl(spi->regs + S3C_SPI_STATUS);

	if (spi_sts & SPI_STUS_RX_OVERRUN_ERR) {
		printk("hspi : Rx overrun error detected\n");
	}

	if (spi_sts & SPI_STUS_RX_UNDERRUN_ERR) {
		printk("hspi : Rx underrun error detected\n");
	}

	if (spi_sts & SPI_STUS_TX_OVERRUN_ERR) {
		printk("hspi : Tx overrun error detected\n");
	}

	if (spi_sts & SPI_STUS_TX_UNDERRUN_ERR) {
		printk("hspi : Tx underrun error detected\n");
	}

	/* pretty much this leaves us with the fact that we've
	 * transmitted or received whatever byte we last sent */

	pr_debug("spi->dmabuf_addr = 0x%x\n",readl(spi->regs + S3C_SPI_STATUS));
	spi_s3c_irq_nextbyte(spi, spi_sts);

	return IRQ_HANDLED;
}

static int s3c_spi_doxfer(struct s3c_spi *spi, struct spi_msg msgs[], int num)
{
	int ret;

	spin_lock_irq(&spi->lock);

	spi->msg     	= msgs;
	spi->msg_num 	= num;
	spi->msg_ptr 	= 0;
	spi->msg_rd_ptr = 0;
	spi->msg_idx 	= 0;

	if (spi->msg->flags & SPI_M_DMA_MODE) {
		spi->dmabuf_addr = spi->spidev.dmabuf;
		pr_debug("spi->dmabuf_addr = 0x%x\n",spi->dmabuf_addr);
	}

	if (spi->msg->wbuf) {
		DEBUG;
		spi->state   = STATE_XFER_TX;
	} else if (spi->msg->rbuf) {
		DEBUG;
		spi->state   = STATE_XFER_RX;
	} else {
		dev_err(spi->dev,"Unknown functionality \n");
		return -ESRCH;
	}
	s3c_spi_message_start(spi);
	DEBUG;

	if (down_interruptible(&spi->sem))
		return -EINTR;

	DEBUG;
	spin_unlock_irq(&spi->lock);

	DEBUG;
	ret = spi->msg_idx;

 	return ret;
}


/* s3c_spi_xfer
 *
 * first port of call from the spi bus code when an message needs
 * transfering across the spi bus.
*/
static int s3c_spi_xfer(struct spi_dev *spi_dev,
			struct spi_msg msgs[], int num)
{
	struct s3c_spi *spi = (struct s3c_spi *)spi_dev->algo_data;
	int retry;
	int ret;

	for (retry = 0; retry < spi_dev->retries; retry++) {

		ret = s3c_spi_doxfer(spi, msgs, num);

		print_reg(spi);
		if (ret != -EAGAIN)
			return ret;
		printk("Retrying transmission (%d)\n", retry);

		udelay(100);
	}

	DEBUG;
	return -EREMOTEIO;
}

static int s3c_spi_close(struct spi_dev *spi_dev)
{
	struct s3c_spi *spi = (struct s3c_spi *)spi_dev->algo_data;
	u32 spi_clkcfg;
	DEBUG;

	s3c2410_dma_free(spi->dma, &s3c2443spi_dma_client);

	if (SPI_CHANNEL == 0){
	/* initialize the gpio */
		s3c2410_gpio_cfgpin(S3C2410_GPE11, 0);
		s3c2410_gpio_cfgpin(S3C2410_GPE12, 0);
		s3c2410_gpio_cfgpin(S3C2410_GPE13, 0);

		s3c2410_gpio_cfgpin(S3C2410_GPL13, 0);
	}else{
		s3c2410_gpio_cfgpin(S3C2410_GPL14, 0);

		s3c2410_gpio_cfgpin(S3C2410_GPL11, 0);
		s3c2410_gpio_cfgpin(S3C2410_GPL12, 0);
		s3c2410_gpio_cfgpin(S3C2410_GPL10, 0);
	}

	spi_clkcfg = readl( spi->regs + S3C_CLK_CFG);
	spi_clkcfg &= SPI_ENCLK_DISABLE;
	writel( spi_clkcfg , spi->regs + S3C_CLK_CFG);

	/* Buffer Clear after finish xfer */
	writel( 0x20, spi->regs + S3C_CH_CFG);
	writel( 0x0, spi->regs + S3C_CH_CFG);

	return 0;

}


/* spi bus registration info */

static struct spi_algorithm s3c_spi_algorithm = {
	.name			= "S3C2443-spi-algorithm",
	.master_xfer		= s3c_spi_xfer,
	.close			= s3c_spi_close,
};

static struct s3c_spi s3c_spi[2] = {
	[0] = {
		.lock	= SPIN_LOCK_UNLOCKED,
		.spidev	= {
			.algo			= &s3c_spi_algorithm,
			.retries		= 2,
			.timeout		= 5,
		}
	},
	[1] = {
		.lock	= SPIN_LOCK_UNLOCKED,
		.spidev	= {
			.algo			= &s3c_spi_algorithm,
			.retries		= 2,
			.timeout		= 5,
		}
	},
};

#endif

/***************************** 参考韦东山SPI视频教程 ********************************/

/* 构造注册spi_master */
static struct spi_master *spi0_controller;

struct s3c_spi_info 
{
	struct s3c2416_hspi_info *devinfo;
	int irq;
	unsigned int reg_base;    
	struct completion	 done;
	struct spi_transfer *cur_t;
	int cur_cnt;
//	struct spi_device *cur_dev;		//for debug
};


#if 1
#undef debug
#else
#define debug
#endif

#ifdef debug
#define DBG(x...)       	printk(x)
#define DEBUG		printk("%s :: %d\n",__FUNCTION__,__LINE__)
void print_reg(struct spi_device *spi)
{
	struct s3c_spi_info *info;
	info = spi_master_get_devdata(spi->master);

	printk("CH_CFG 	= 0x%08x\n",readl(info->reg_base + S3C_CH_CFG));
	printk("CLK_CFG = 0x%08x\n",readl(info->reg_base + S3C_CLK_CFG));
	printk("MODE_CFG = 0x%08x\n",readl(info->reg_base + S3C_MODE_CFG));
	printk("SLAVE_CFG = 0x%08x\n",readl(info->reg_base + S3C_SLAVE_SEL));
	printk("INT_EN 	= 0x%08x\n",readl(info->reg_base + S3C_SPI_INT_EN));
	printk("SPI_STATUS = 0x%08x\n",readl(info->reg_base + S3C_SPI_STATUS));
	printk("PACKET_CNT = 0x%08x\n",readl(info->reg_base + S3C_PACKET_CNT));
	printk("PEND_CLR = 0x%08x\n",readl(info->reg_base + S3C_PENDING_CLR));
}
#else
#define DEBUG
#define DBG(x...)       do { } while (0)
void print_reg(struct spi_device *spi)
{
}
#endif


static int s3c2416_spi_controler_init(int which, struct s3c_spi_info *info)
{
	int ret = 0;

	/* whick clock should I enable? */
#if 0
	
	struct clk *clk = clk_get(NULL, "spi");

	if (IS_ERR(clk)) 
	{
		dev_err(&pdev->dev, "cannot get clock spi\n");
		ret = -ENOENT;
		goto out;
	}

	/* 使能spi controller 0/1的时钟 */
	clk_enable(clk);
#endif

	/* 
	 * copy from 
	 * static int s3c_spi_hw_init(struct s3c_spi *spi)
	 */
	/* Enable HS-SPI_0 (EPLL) clock, Enable SPICLK0 (MPLL) */
	writel(readl(S3C2443_SCLKCON)|(1<<14)|(1 << 19), S3C2443_SCLKCON);
	/* Enable PCLK into the SPI_HS0 */
	writel(readl(S3C2443_PCLKCON)|(1<<6), S3C2443_PCLKCON);
	/* HSSPI_EN2 = 1 */
	writel(readl(S3C24XX_MISCCR)|S3C24XX_MISCCR_SPISEL, S3C24XX_MISCCR);

#ifdef CONFIG_SPICLK_PCLK
	/*Enable PCLK into the HS SPI*/
	writel(readl(S3C2443_PCLKCON)|(1<<6), S3C2443_PCLKCON);

	clk_enable(clk_get(NULL, "spi"));
//	clk_enable(spi->clk);

#elif defined CONFIG_SPICLK_EPLL
	/* implemetation when use EPLL clock */
	writel(0x800, S3C2443_LOCKCON1);
	/* ESYSCLK uses EPLL Output */
	writel( (readl( S3C2443_CLKSRC ) | (1 << 6) ), S3C2443_CLKSRC); 
	/* 96MHz? */
	writel((48<<16)|(3<<8)|(1<<0) ,S3C2443_EPLLCON);

	/* EPLL On */
	writel( readl(S3C2443_EPLLCON)& (~(1<<24)) , S3C2443_EPLLCON ); 
	/* Epll Ratio is 1 */
	writel(( readl(S3C2443_CLKDIV1) & (~(0x3<<24))) | (0x0 << 24) , S3C2443_CLKDIV1 ); 
	writel(( readl(S3C24XX_MISCCR) & (~(0x7<<8))) | (0x1 << 8) , S3C24XX_MISCCR );

	/* Epll, prescaler = 1					*/
	/* clock =  ( clock source / (2 * ( prescaler + 1)))	*/
	writel(0x501, info->reg_base + S3C_CLK_CFG); 	//Use EPLL Clock and Clock On Prescaler 1
#else
#error you must define correct confige file.
#endif

#if 0
	/* GPIO */
	if (which == 0)
	{
		/* SPI controller 0 */
		/*
		* GPE11 SPIMISO   
		* GPE12 SPIMOSI   
		* GPE13 SPICLK    
		*/
		s3c2410_gpio_cfgpin(S3C2410_GPE11, S3C2410_GPE11_SPIMISO0);
		s3c2410_gpio_cfgpin(S3C2410_GPE12, S3C2410_GPE12_SPIMOSI0);
		s3c2410_gpio_cfgpin(S3C2410_GPE13, S3C2410_GPE13_SPICLK0);

		/* set ss pins output mode */
		for(i = 0; i < ARRAY_SIZE(info->devinfo->ss_talbes); i++)
		{
			if(info->devinfo->ss_talbes[i] != 0)
			{
				s3c2410_gpio_cfgpin(info->devinfo->ss_talbes[i], S3C2410_GPIO_OUTPUT);
				s3c2410_gpio_setpin(info->devinfo->ss_talbes[i], 1);
			}
		}
	}
	else if (which == 1)
	{

	}

#endif

//out:
	if (ret < 0)
	{
		
	}
	
	return ret;
}

static int s3c2416_spi_gpio_init(int which, struct s3c_spi_info *info)
{
	int i = 0;
	
	/* GPIO */
	if (which == 0)
	{
		/* SPI controller 0 */
		/*
		* GPE11 SPIMISO   
		* GPE12 SPIMOSI   
		* GPE13 SPICLK    
		*/
		s3c2410_gpio_cfgpin(S3C2410_GPE11, S3C2410_GPE11_SPIMISO0);
		s3c2410_gpio_cfgpin(S3C2410_GPE12, S3C2410_GPE12_SPIMOSI0);
		s3c2410_gpio_cfgpin(S3C2410_GPE13, S3C2410_GPE13_SPICLK0);

		/* set ss pins output mode */
		for(i = 0; i < ARRAY_SIZE(info->devinfo->ss_talbes); i++)
		{
			if(info->devinfo->ss_talbes[i] != 0)
			{
				s3c2410_gpio_cfgpin(info->devinfo->ss_talbes[i], S3C2410_GPIO_OUTPUT);
				s3c2410_gpio_setpin(info->devinfo->ss_talbes[i], 1);
			}
		}
	}
	else if (which == 1)
	{

	}

	return 0;
}

static void flush_fifo(struct spi_device *spi)
{
	struct s3c_spi_info *info;
	unsigned long val;

	info = spi_master_get_devdata(spi->master);
	
	writel(0, info->reg_base + S3C_PACKET_CNT);

	val = readl(info->reg_base + S3C_CH_CFG);
	val &= ~(SPI_CH_RXCH_ON | SPI_CH_TXCH_ON);
	writel(val, info->reg_base + S3C_CH_CFG);

	val = readl(info->reg_base + S3C_CH_CFG);
	val |= SPI_CH_SW_RST;
	writel(val, info->reg_base + S3C_CH_CFG);
#if 0
	/* Flush TxFIFO*/
	do {
		val = readl(regs + S3C64XX_SPI_STATUS);
	} while (TX_FIFO_LVL(val, sci) && loops--);

	if (loops == 0)
		dev_warn(&sdd->pdev->dev, "Timed out flushing TX FIFO\n");

	/* Flush RxFIFO*/
	loops = msecs_to_loops(1);
	do {
		val = readl(regs + S3C64XX_SPI_STATUS);
		if (RX_FIFO_LVL(val, sci))
			readl(regs + S3C64XX_SPI_RX_DATA);
		else
			break;
	} while (loops--);

	if (loops == 0)
		dev_warn(&sdd->pdev->dev, "Timed out flushing RX FIFO\n");
#endif
	val = readl(info->reg_base + S3C_CH_CFG);
	val &= ~SPI_CH_SW_RST;
	writel(val, info->reg_base + S3C_CH_CFG);

	val = readl(info->reg_base + S3C_MODE_CFG);
	val &= ~(SPI_MODE_TXDMA_ON | SPI_MODE_RXDMA_ON);
	writel(val, info->reg_base + S3C_MODE_CFG);
}


static int s3c2416_spi_setup(struct spi_device *spi)
{
	struct s3c_spi_info *info;
	struct clk *clk;
	unsigned long src_clk_rate = 0;

	unsigned int spi_chcfg = 0;
	unsigned int spi_slavecfg =0;
	unsigned int spi_inten= 0;
	unsigned int spi_packet=0;

	unsigned int prescaler = 3;
	unsigned int spi_clkcfg = 0;
	unsigned int spi_modecfg = 0 ;

	DEBUG;

	info = spi_master_get_devdata(spi->master);
//	clk = clk_get(NULL, "plck");

	flush_fifo(spi);

	/* initialise the spi controller */
//	s3c_spi_hw_init(spi);
	/* 硬件初始化，主要是初始化EPLL，和CLK,MISO,MOSI引脚 */
	s3c2416_spi_controler_init(0, info);
//	s3c2416_spi_gpio_init(0, info);

	/* 1. Set transfer type (CPOL & CPHA set) */
	spi_chcfg = 0;
	/* set CPOL */
	if (spi->mode & SPI_CPOL)
	{
		spi_chcfg |= SPI_CH_FALLING;
	}
	else
	{
		spi_chcfg |= SPI_CH_RISING;
	}
	/* set CPHA */
	if (spi->mode & SPI_CPHA)
	{
		spi_chcfg |= SPI_CH_FORMAT_B;
	}
	else
	{
		spi_chcfg |= SPI_CH_FORMAT_A;
	}

	spi_chcfg |= SPI_CH_MASTER;
//	if (spi->msg->flags & SPI_M_MODE_MASTER) {
//		spi_chcfg |= SPI_CH_MASTER;
//	} else if(spi->msg->flags & SPI_M_MODE_SLAVE){
//		spi_chcfg |= SPI_CH_SLAVE;
//	}

	writel( spi_chcfg , info->reg_base + S3C_CH_CFG);

	/* 2. Set clock configuration register */
	spi_clkcfg = SPI_ENCLK_ENABLE;

#if defined CONFIG_SPICLK_PCLK
	spi_clkcfg |= SPI_CLKSEL_PCLK;
	clk = clk_get(NULL, "pclk");	
#elif defined CONFIG_SPICLK_EPLL
	spi_clkcfg |= SPI_CLKSEL_ECLK;
	clk = clk_get(NULL, "epll");
#else
#error you must define correct confige file.
#endif
	writel( spi_clkcfg , info->reg_base + S3C_CLK_CFG);

	spi_clkcfg = readl( info->reg_base + S3C_CLK_CFG);

	/* SPI clockout = clock source / (2 * (prescaler + 1)) */
	/* get HSPI src clock rate */
	src_clk_rate = clk_get_rate(clk);
//	printk("HSPI clk = %d Hz\r\n", src_clk_rate);
	prescaler = DIV_ROUND_UP(clk_get_rate(clk), spi->max_speed_hz * 2) - 1;
	if(prescaler > 255)
	{
		prescaler = 255;
	}
//	printk("HSPI prescaler = %d\r\n", prescaler);
	spi_clkcfg |= prescaler;
	writel( spi_clkcfg , info->reg_base + S3C_CLK_CFG);

	/* 3. Set SPI MODE configuration register */
#ifdef CONFIG_WORD_TRANSIZE
	spi_modecfg = SPI_MODE_CH_TSZ_WORD;
#else
	spi_modecfg = SPI_MODE_CH_TSZ_BYTE;
#endif
	spi_modecfg |= SPI_MODE_TXDMA_OFF| SPI_MODE_SINGLE| SPI_MODE_RXDMA_OFF;

//	if (msg->flags & SPI_M_DMA_MODE) {
//		spi_modecfg |= SPI_MODE_TXDMA_ON| SPI_MODE_RXDMA_ON;
//	}

//	if (msg->wbuf)
		spi_modecfg |= ( 0x1 << 5); /* Tx FIFO trigger level in INT mode */
//	if (msg->rbuf)
		spi_modecfg |= ( 0x1 << 11); /* Rx FIFO trigger level in INT mode */

	spi_modecfg |= ( 0x3ff << 19);
	writel(spi_modecfg, info->reg_base + S3C_MODE_CFG);

	/* 4. Set SPI INT_EN register */

//	if (msg->wbuf)
//		spi_inten = SPI_INT_TX_FIFORDY_EN|SPI_INT_TX_UNDERRUN_EN|SPI_INT_TX_OVERRUN_EN;
//	if (msg->rbuf){
//		spi_inten = SPI_INT_RX_FIFORDY_EN|SPI_INT_RX_UNDERRUN_EN|SPI_INT_RX_OVERRUN_EN|SPI_INT_TRAILING_EN;
//	}

//	spi_inten = SPI_INT_TX_FIFORDY_EN|SPI_INT_RX_FIFORDY_EN|SPI_INT_TX_UNDERRUN_EN|SPI_INT_TX_OVERRUN_EN|SPI_INT_RX_UNDERRUN_EN|SPI_INT_RX_OVERRUN_EN|SPI_INT_TRAILING_EN;
//	spi_inten = SPI_INT_TX_UNDERRUN_EN|SPI_INT_TX_OVERRUN_EN|SPI_INT_RX_UNDERRUN_EN|SPI_INT_RX_OVERRUN_EN|SPI_INT_TRAILING_EN;
//	spi_inten = 0;
	spi_inten = SPI_INT_TX_UNDERRUN_EN|SPI_INT_TX_OVERRUN_EN|SPI_INT_RX_UNDERRUN_EN|SPI_INT_RX_OVERRUN_EN;
	writel(spi_inten, info->reg_base + S3C_SPI_INT_EN);

	writel(0x1f, info->reg_base + S3C_PENDING_CLR);

	/* 5. Set Packet Count configuration register */
	spi_packet = SPI_PACKET_CNT_EN;
	spi_packet |= 0xffff;
	writel(spi_packet, info->reg_base + S3C_PACKET_CNT);

	/* 6. Set Tx or Rx Channel on */
	spi_chcfg = readl(info->reg_base + S3C_CH_CFG);

	spi_chcfg |= SPI_CH_TXCH_OFF | SPI_CH_RXCH_OFF;

//	if (msg->wbuf)
		spi_chcfg |= SPI_CH_TXCH_ON;
//	if (msg->rbuf)
		spi_chcfg |= SPI_CH_RXCH_ON;

	writel(spi_chcfg, info->reg_base + S3C_CH_CFG);
#if 0
	if (msg->flags & SPI_M_DMA_MODE) {

		if (msg->wbuf)
			spi->dma = DMACH_SPI_TX;
		if (msg->rbuf)
			spi->dma = DMACH_SPI_RX;

		if (s3c2410_dma_request(spi->dma, &s3c2443spi_dma_client, NULL)) {
			printk(KERN_WARNING  "unable to get DMA channel.\n" );
		}

		s3c2410_dma_set_buffdone_fn(spi->dma, s3c_spi_dma_cb);
		s3c2410_dma_set_opfn(spi->dma, NULL);

		if (msg->wbuf)
			s3c_spi_dma_init(spi, 0);
		if (msg->rbuf)
			s3c_spi_dma_init(spi, 1);

		s3c2410_dma_enqueue(spi->dma, (void *) spi, spi->dmabuf_addr, spi->msg->len);
	}
#endif

#if 0
	/* 7. Set nSS low to start Tx or Rx operation */
	spi_slavecfg = readl(spi->regs + S3C_SLAVE_SEL);
	spi_slavecfg &= SPI_SLAVE_SIG_ACT;
	spi_slavecfg |= (0x3f << 4);
	writel(spi_slavecfg, spi->regs + S3C_SLAVE_SEL);
#endif

	/* 7. Set nSS low to start Tx or Rx operation */
	spi_slavecfg = readl(info->reg_base + S3C_SLAVE_SEL);
	spi_slavecfg |= SPI_SLAVE_SIG_INACT;		/* 手动控制SS引脚时，SS置1 */
	spi_slavecfg &= ~SPI_SLAVE_AUTO;			/* 手动控制SS引脚 */
	spi_slavecfg |= (0x3f << 4);
	writel(spi_slavecfg, info->reg_base + S3C_SLAVE_SEL);

	/* fatal: 手动模式时，发送数据前必须清0 bit0,否则数据不会发送 */
	spi_slavecfg = readl(info->reg_base + S3C_SLAVE_SEL);
	spi_slavecfg &= ~SPI_SLAVE_SIG_INACT;		/* 手动控制SS引脚时，SS置0 */
	spi_slavecfg &= ~SPI_SLAVE_AUTO;			/* 手动控制SS引脚 */
	writel(spi_slavecfg, info->reg_base + S3C_SLAVE_SEL);

	print_reg(spi);

	return 0;
}


static int s3c2416_spi_enable_txfifolevel_int(struct spi_master *master)
{
	struct s3c_spi_info *info;
	unsigned int regval;

	info = spi_master_get_devdata(master);

	regval = readl(info->reg_base + S3C_SPI_INT_EN);
	regval |= SPI_INT_TX_FIFORDY_EN;

	writel(regval, info->reg_base + S3C_SPI_INT_EN);	

	return 0;
}

static int s3c2416_spi_disable_txfifolevel_int(struct spi_master *master)
{
	struct s3c_spi_info *info;
	unsigned int regval;

	info = spi_master_get_devdata(master);

	regval = readl(info->reg_base + S3C_SPI_INT_EN);
	regval &= ~SPI_INT_TX_FIFORDY_EN;

	writel(regval, info->reg_base + S3C_SPI_INT_EN);	

	return 0;
}

static int s3c2416_spi_enable_rxfifolevel_int(struct spi_master *master)
{
	struct s3c_spi_info *info;
	unsigned int regval;

	info = spi_master_get_devdata(master);

	regval = readl(info->reg_base + S3C_SPI_INT_EN);
	regval |= SPI_INT_RX_FIFORDY_EN;

	writel(regval, info->reg_base + S3C_SPI_INT_EN);

	return 0;
}

static int s3c2416_spi_disable_rxfifolevel_int(struct spi_master *master)
{
	struct s3c_spi_info *info;
	unsigned int regval;

	info = spi_master_get_devdata(master);

	regval = readl(info->reg_base + S3C_SPI_INT_EN);
	regval &= ~SPI_INT_RX_FIFORDY_EN;

	writel(regval, info->reg_base + S3C_SPI_INT_EN);	

	return 0;
}

static int s3c2416_spi_enable_txrxfifolevel_int(struct spi_master *master, int tx, int rx)
{
	struct s3c_spi_info *info;
	unsigned int regval;

	info = spi_master_get_devdata(master);

	regval = readl(info->reg_base + S3C_SPI_INT_EN);

	if(tx)
	{
		regval |= SPI_INT_TX_FIFORDY_EN;
	}
	else
	{
		regval &= ~SPI_INT_TX_FIFORDY_EN;
	}

	if(rx)
	{
		regval |= SPI_INT_RX_FIFORDY_EN;
	}
	else
	{
		regval &= ~SPI_INT_RX_FIFORDY_EN;
	}

	writel(regval, info->reg_base + S3C_SPI_INT_EN);	

	return 0;
}

static int s3c2416_spi_transfer(struct spi_device *spi, struct spi_message *mesg)
{
	struct spi_master *master = spi->master;
	struct s3c_spi_info *info;
	struct spi_transfer	*t = NULL;

	info = spi_master_get_devdata(master);

//	info->cur_dev = spi;		/* for debug */

	DEBUG;

//	/* 0. 发送第1个spi_transfer之前setup，会把master上所有的SS脚置1，所以必须放在片选之前进行 */
//	master->setup(spi);

//	printk("info->devinfo->ss_talbes[spi->chip_select] = %x\r\n", info->devinfo->ss_talbes[spi->chip_select]);

	/* 1. 选中芯片 */
	s3c2410_gpio_setpin(info->devinfo->ss_talbes[spi->chip_select], 0);  /* 默认为低电平选中 */

//	udelay(100);
	/* 2. 发数据 */

	/* 2.1 发送第1个spi_transfer之前setup */
	master->setup(spi);

	/* 2.2 从spi_message中逐个取出spi_transfer,执行它 */
	list_for_each_entry (t, &mesg->transfers, transfer_list) 
	{	
		/* 处理这个spi_transfer */
		info->cur_t = t;
		info->cur_cnt = 0;
		init_completion(&info->done);

		if (t->tx_buf)
		{
			DEBUG;

			/* 开始发送前清空RX FIFO */
			while((readl(info->reg_base + S3C_SPI_STATUS) & (0x7F << 13)) != 0x00)
			{
				DEBUG;
				print_reg(spi);
				/* dummy read */
				readb(info->reg_base + S3C_SPI_RX_DATA);
			}
			
			/* 发送 */
			writeb(((unsigned char *)t->tx_buf)[0], info->reg_base + S3C_SPI_TX_DATA);

			DEBUG;
			
			/* 允许TX FIFO LEVEL中断 */
			s3c2416_spi_enable_txfifolevel_int(master);
			/* 它会触发中断 */

			DEBUG;
			print_reg(spi);

			/* 休眠 */
			wait_for_completion(&info->done);
		}
		else if(t->rx_buf)
		{
			DEBUG;

			/* 开始接收前清空接收FIFO */
			while((readl(info->reg_base + S3C_SPI_STATUS) & (0x7F << 13)) != 0x00)
			{
				DEBUG;
				print_reg(spi);
				/* dummy read */
				readb(info->reg_base + S3C_SPI_RX_DATA);
			}

//			printk("S3C_SPI_STATUS = 0x%08x\n",readl(info->reg_base + S3C_SPI_STATUS));
			
			/* 接收 */
			writeb(0xff, info->reg_base + S3C_SPI_TX_DATA);

			/* 允许RX FIFO LEVEL中断 */
			s3c2416_spi_enable_rxfifolevel_int(master);
			/* 它会触发中断 */

			/* 休眠 */
			wait_for_completion(&info->done);
		}
	}

	DEBUG;
	
	/* 2.3 唤醒等待的进程 */
	mesg->status = 0;
	mesg->complete(mesg->context);    

//	udelay(100);
	
	/* 3. 取消片选 */
	s3c2410_gpio_setpin(info->devinfo->ss_talbes[spi->chip_select], 1);  /* 默认为低电平选中 */

	return 0;
}


/* s3c_spi_irq
 *
 * top level IRQ servicing routine
*/
static irqreturn_t s3c2416_spi_irq(int irqno, void *dev_id)
{
//	struct s3c_spi *spi = dev_id;
	volatile unsigned long spi_sts;
	volatile unsigned long status;
	unsigned long int_en = 0;

	/* dev_id is the master when calling request_irq() */
	struct spi_master *master = (struct spi_master *)dev_id;
	struct s3c_spi_info *info = spi_master_get_devdata(master);
	struct spi_transfer *t = info->cur_t;

	/* save irq enable flags for spi */
	int_en = readl(info->reg_base + S3C_SPI_INT_EN);
	/* disable all irq for spi */
	writel(0, info->reg_base + S3C_SPI_INT_EN);

	DEBUG;
	
	if (!t)
	{
		printk("spi status = 0x%x\n",readl(info->reg_base + S3C_SPI_STATUS));

		/* disable rx and tx int */
//		s3c2416_spi_disable_txfifolevel_int(master);
//		s3c2416_spi_disable_rxfifolevel_int(master);
		
		/* 误触发 */
		return IRQ_HANDLED;            
	}

	DEBUG;

	spi_sts = readl(info->reg_base + S3C_SPI_STATUS);

	if (spi_sts & SPI_STUS_RX_OVERRUN_ERR) {
		printk("hspi : Rx overrun error detected\n");
	}

	if (spi_sts & SPI_STUS_RX_UNDERRUN_ERR) {
		printk("hspi : Rx underrun error detected\n");
	}

	if (spi_sts & SPI_STUS_TX_OVERRUN_ERR) {
		printk("hspi : Tx overrun error detected\n");
	}

	if (spi_sts & SPI_STUS_TX_UNDERRUN_ERR) {
		printk("hspi : Tx underrun error detected\n");
	}

	/* pretty much this leaves us with the fact that we've
	 * transmitted or received whatever byte we last sent */

//	pr_debug("spi status = 0x%x\n",readl(info->reg_base + S3C_SPI_STATUS));
//	spi_s3c_irq_nextbyte(spi, spi_sts);

	DEBUG;

	if (t->tx_buf) /* 是发送 */
	{
//		while((readl(info->reg_base + S3C_SPI_STATUS) & (0x01)) == 0x00)

#if 0
		if((status = (readl(info->reg_base + S3C_SPI_STATUS) & (0x7F << 13))) == 0x00)
		{
			goto out;
		}
		printk("INT0 S3C_SPI_STATUS = 0x%08x\n",status);
#endif		

#if 1
		/* wait until tx fifo is empty */
		while((status = readl(info->reg_base + S3C_SPI_STATUS) & (0x7F << 6)) != 0x00)
		{
//			printk("INT1 S3C_SPI_STATUS = 0x%08x\n",status);
		}
#endif

#if 0
		/* wait until tx fifo is less than half full */
		while(((status = (readl(info->reg_base + S3C_SPI_STATUS) & (0x7F << 6))) >> 6) > 64 / 2)
		{
			printk("INT1 S3C_SPI_STATUS = 0x%08x\n",status);
		}
#endif

#if 0
		while((status = (readl(info->reg_base + S3C_SPI_STATUS) & (0x01))) == 0x00)
		{
			printk("INT2 S3C_SPI_STATUS = 0x%08x\n",status);
		}

		while((status = (readl(info->reg_base + S3C_SPI_STATUS) & (1 << 21))) == 0x00)
		{
			printk("INT3 S3C_SPI_STATUS = 0x%08x\n",status);
		}
#endif
//		printk("tx info->cur_cnt = %d, t->len = %d\r\n", info->cur_cnt, t->len);
		
		info->cur_cnt++;

		if (info->cur_cnt < t->len)/* 没发完? */
		{
			DEBUG;
			/* dummy read to clear rx fifo */
			readb(info->reg_base + S3C_SPI_RX_DATA);
			writeb(((unsigned char *)t->tx_buf)[info->cur_cnt], info->reg_base + S3C_SPI_TX_DATA); 
		}     
		else
		{
			DEBUG;
//			s3c2416_spi_disable_txfifolevel_int(master);		/* disable txfifo int */
			int_en &= ~SPI_INT_TX_FIFORDY_EN;

			/* wait for txfifo becomes empty */
			while((readl(info->reg_base + S3C_SPI_STATUS) & (0x7F << 6)) != 0x00)
			{
				
			}
			/* wait for shift register becomes empty */
			while((readl(info->reg_base + S3C_SPI_STATUS) & (1 << 21)) == 0x00)
			{
				
			}
			/* all bytes have been translated here */
//			print_reg(info->cur_dev);
			complete(&info->done); /* 唤醒 */
		}	
	}
	else /* 接收 */
	{
//		printk("rx info->cur_cnt = %d, t->len = %d\r\n", info->cur_cnt, t->len);
		
		if((status = (readl(info->reg_base + S3C_SPI_STATUS) & (0x7F << 13))) == 0x00)
		{
			goto out;
		}
//		printk("INT4 S3C_SPI_STATUS = 0x%08x\n",status);

		/* 读/存数据 */
		if(info->cur_cnt < t->len)
		{
			((unsigned char *)t->rx_buf)[info->cur_cnt] = readb(info->reg_base + S3C_SPI_RX_DATA);
			info->cur_cnt++;
		}

		if (info->cur_cnt < t->len)/* 没收完? */
		{
			DEBUG;
			writeb(0xff, info->reg_base + S3C_SPI_TX_DATA); 
		}     
		else
		{
			DEBUG;
//			s3c2416_spi_disable_rxfifolevel_int(master);		/* disable rxfifo int */
			int_en &= ~SPI_INT_RX_FIFORDY_EN;
				
			complete(&info->done); /* 唤醒 */
		}
	}

out:
	/* TX FIFO中断在进入中断处理前(清除了中断标志后)，可能
	 * 再次申请了中断，写入了SRCPND和INTPND。所以即使在IRQ中关闭了TX FIFO中断，该SRCPND和
	 * INTPND中的中断申请仍然有效，多进入一次TX FIFO中断
	 * 所以在重新使能SPI外设的中断前，清除SRCPND和INTPND中的中断
	 */
	/* clear interruput pending bits in SRCPND */
	writel(1 << 22, S3C2410_SRCPND);
	/* clear interruput pending bits in INTPND */
	writel(1 << 22, S3C2410_INTPND);
	/* reenable HSPI interruput */
	writel(int_en, info->reg_base + S3C_SPI_INT_EN);
	return IRQ_HANDLED;
}

static struct spi_master *create_spi_master_s3c2416(struct platform_device *pdev, int bus_num, unsigned int reg_base_phy, int irq)
{
	int ret;
	struct spi_master *master;
	struct s3c_spi_info *info;
	int i;

	DEBUG;
	
	master = spi_alloc_master(&pdev->dev, sizeof(struct s3c_spi_info));
	master->bus_num = bus_num;
	master->num_chipselect = 0xffff;
//	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;

	master->setup    = s3c2416_spi_setup;
	master->transfer = s3c2416_spi_transfer;
	master->cleanup = NULL;

	DEBUG;

	info = spi_master_get_devdata(master);
	info->reg_base = (unsigned int)ioremap(reg_base_phy, 0x40);
	info->irq = irq;
	info->devinfo = pdev->dev.platform_data;

#if 0
	/* set SS pin sets */
	memset(info->devinfo->ss_talbes, 0 ,sizeof(info->devinfo->ss_talbes));
	info->devinfo->ss_talbes[0] = S3C2410_GPL13;
	info->devinfo->ss_talbes[1] = S3C2410_GPH10;
	info->devinfo->ss_talbes[2] = 0;
	info->devinfo->ss_talbes[3] = 0;
	info->devinfo->ss_talbes[4] = 0;
	info->devinfo->ss_talbes[5] = 0;
#endif

	DEBUG;

	/* 硬件初始化，主要是初始化EPLL，和CLK,MISO,MOSI引脚 */
	s3c2416_spi_controler_init(bus_num, info);
	s3c2416_spi_gpio_init(0, info);

	DEBUG;

	ret = request_irq(irq, s3c2416_spi_irq, IRQF_DISABLED, "s3c2416_spi", master);

	DEBUG;

	spi_register_master(master);

	/* add spi_device ? */
	/* register the chips to go with the board */

	for (i = 0; i < info->devinfo->board_size; i++) {
		dev_info(&pdev->dev, "registering %p: %s\n",
			 &info->devinfo->board_info[i],
			 info->devinfo->board_info[i].modalias);

		info->devinfo->board_info[i].controller_data = info;

		printk("call  spi_new_device\n");
		spi_new_device(master,  info->devinfo->board_info + i);	/* 根据devs.c中的spi_gpio_cfg创建spi_device，spi_device根据名字去匹配spi_driver */
		printk("return from spi_new_device\n");
	}
	
	return master;
}

static void destroy_spi_master_s3c2416(struct spi_master *master)
{
	struct s3c_spi_info *info = spi_master_get_devdata(master);;

	spi_unregister_master(master);
	free_irq(info->irq, master);
	iounmap((void *)info->reg_base);
	kfree(master);
}

/* s3c_spi_probe
 *
 * called by the bus driver when a suitable device is found
*/

static int s3c_spi_probe(struct platform_device *pdev)
{
#if 0
	struct s3c_spi *spi = &s3c_spi[pdev->id];
	struct resource *res;
	int ret;

	/* find the clock and enable it */
	sema_init(&spi->sem, 0);
	spi->nr = pdev->id;
	spi->dev = &pdev->dev;

	spi->clk = clk_get(&pdev->dev, "spi");

	if (IS_ERR(spi->clk)) {
		dev_err(&pdev->dev, "cannot get clock\n");
		ret = -ENOENT;
		goto out;
	}

	/* map the registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (res == NULL) {
		dev_err(&pdev->dev, "cannot find IO resource\n");
		ret = -ENOENT;
		goto out;
	}

	spi->ioarea = request_mem_region(res->start, (res->end - res->start) + 1, pdev->name);

	if (spi->ioarea == NULL) {
		dev_err(&pdev->dev, "cannot request IO\n");
		ret = -ENXIO;
		goto out;
	}

	printk(KERN_ALERT "resource start : %x\n",res->start);

	spi->regs = ioremap(res->start, (res->end - res->start) + 1);

	if (spi->regs == NULL) {
		dev_err(&pdev->dev, "cannot map IO\n");
		ret = -ENXIO;
		goto out;
	}

	printk(KERN_ALERT "hspi registers %p (%p, %p)\n", spi->regs, spi->ioarea, res);

	/* setup info block for the spi core */

	spi->spidev.algo_data = spi;
	spi->spidev.dev.parent = &pdev->dev;
	spi->spidev.minor = spi->nr;
	init_MUTEX(&spi->spidev.bus_lock);

	/* find the IRQ for this unit (note, this relies on the init call to
	 * ensure no current IRQs pending
	 */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);

	if (res == NULL) {
		printk("hspi cannot find IRQ\n");
		ret = -ENOENT;
		goto out;
	}

	ret = request_irq(res->start, s3c_spi_irq, SA_INTERRUPT,
			pdev->name, spi);

	if (ret != 0) {
		printk("hspi cannot claim IRQ\n");
		goto out;
	}

	ret = spi_attach_spidev(&spi->spidev);

	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add adapter to spi core\n");
		goto out;
	}

	dev_set_drvdata(&pdev->dev, spi);

	dev_info(&pdev->dev, "%s: S3C SPI adapter\n", spi->dev->bus_id);

	printk("%s: S3C SPI adapter\n", spi->dev->bus_id);

out:
	if (ret < 0)
		s3c_spi_free(spi);

	return ret;
#endif

	struct clk *clk;

	printk("s3c_spi_probe\r\n");

	clk = clk_get(&pdev->dev, "spi");

	if (IS_ERR(clk)) 
	{
		dev_err(&pdev->dev, "cannot get spi clock\n");
	}

	clk_enable(clk);

	/* don't try to register spi_master with the same bus_num ! */
	spi0_controller = create_spi_master_s3c2416(pdev, 100, 0x52000000, IRQ_SPI0);

	/* register spi_device according to devs.c */

	return 0;
}

/* s3c_spi_remove
 *
 * called when device is removed from the bus
*/
static int s3c_spi_remove(struct platform_device *pdev)
{
#if 0
	struct s3c_spi *spi = dev_get_drvdata(&pdev->dev);

	DEBUG;

	if (spi != NULL) {
		spi_detach_spidev(&spi->spidev);
		s3c_spi_free(spi);
		dev_set_drvdata(&pdev->dev, NULL);
	}
#endif
	destroy_spi_master_s3c2416(spi0_controller);

	return 0;
}

//#ifdef CONFIG_PM
#if 0
static int s3c_spi_suspend(struct platform_device *pdev, pm_message_t msg)
{
	struct s3c_spi *hw = platform_get_drvdata(pdev);
	clk_disable(hw->clk);
	return 0;
}

static int s3c_spi_resume(struct platform_device *pdev)
{
	struct s3c_spi *hw = platform_get_drvdata(pdev);
	clk_enable(hw->clk);
	return 0;
}
#else
#define s3c_spi_suspend NULL
#define s3c_spi_resume  NULL
#endif

/* device driver for platform bus bits */
static struct platform_driver s3c_spi_driver = {
	.probe		= s3c_spi_probe,
	.remove		= s3c_spi_remove,
//#ifdef CONFIG_PM
#if 0
	.suspend		= s3c_spi_suspend,
	.resume		= s3c_spi_resume,
#endif
	.driver		= 
	{
		.name	= "s3c2416-hspi",
		.owner	= THIS_MODULE,
		.bus    = &platform_bus_type,
	},
};

static int __init s3c_spi_driver_init(void)
{
	printk(KERN_INFO "S3C2416 HSPI Driver \n");

	return platform_driver_register(&s3c_spi_driver);
}

static void __exit s3c_spi_driver_exit(void)
{
	platform_driver_unregister(&s3c_spi_driver);
}

module_init(s3c_spi_driver_init);
module_exit(s3c_spi_driver_exit);

MODULE_DESCRIPTION("S3C2443 SPI Bus driver");
MODULE_AUTHOR("Ryu Euiyoul<steven.ryu@samsung.com>");
MODULE_LICENSE("GPL");
