/* 复制的helper2416的 */
/*
 * smc911x.c
 * This is a driver for SMSC's LAN911{5,6,7,8} single-chip Ethernet devices.
 *
 * Copyright (C) 2005 Sensoria Corp
 *	   Derived from the unified SMC91x driver by Nicolas Pitre
 *	   and the smsc911x.c reference driver by SMSC
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
 *
 * Arguments:
 *	 watchdog  = TX watchdog timeout
 *	 tx_fifo_kb = Size of TX FIFO in KB
 *
 * History:
 *	  04/16/05	Dustin McIntire		 Initial version
 */
static const char version[] =
	 "smc911x.c: v1.0 04-16-2005 by Dustin McIntire <dustin@sensoria.com>\n";

/* Debugging options */
#define ENABLE_SMC_DEBUG_RX		0
#define ENABLE_SMC_DEBUG_TX		0
#define ENABLE_SMC_DEBUG_DMA		0
#define ENABLE_SMC_DEBUG_PKTS		0
#define ENABLE_SMC_DEBUG_MISC		0
#define ENABLE_SMC_DEBUG_FUNC		0

#define SMC_DEBUG_RX		((ENABLE_SMC_DEBUG_RX	? 1 : 0) << 0)
#define SMC_DEBUG_TX		((ENABLE_SMC_DEBUG_TX	? 1 : 0) << 1)
#define SMC_DEBUG_DMA		((ENABLE_SMC_DEBUG_DMA	? 1 : 0) << 2)
#define SMC_DEBUG_PKTS		((ENABLE_SMC_DEBUG_PKTS ? 1 : 0) << 3)
#define SMC_DEBUG_MISC		((ENABLE_SMC_DEBUG_MISC ? 1 : 0) << 4)
#define SMC_DEBUG_FUNC		((ENABLE_SMC_DEBUG_FUNC ? 1 : 0) << 5)

#ifndef SMC_DEBUG
#define SMC_DEBUG	 ( SMC_DEBUG_RX	  | \
			   SMC_DEBUG_TX	  | \
			   SMC_DEBUG_DMA  | \
			   SMC_DEBUG_PKTS | \
			   SMC_DEBUG_MISC | \
			   SMC_DEBUG_FUNC   \
			 )
#endif

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/crc32.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/workqueue.h>
#include <linux/irq.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/io.h>
#include <asm/irq.h>

#include "smc911x.h"

/*
 * Transmit timeout, default 5 seconds.
 */
static int watchdog = 5000;
module_param(watchdog, int, 0400);
MODULE_PARM_DESC(watchdog, "transmit timeout in milliseconds");

static int tx_fifo_kb=8;
module_param(tx_fifo_kb, int, 0400);
MODULE_PARM_DESC(tx_fifo_kb,"transmit FIFO size in KB (1<x<15)(default=8)");

MODULE_LICENSE("GPL");

/*
 * The internal workings of the driver.  If you are changing anything
 * here with the SMC stuff, you should have the datasheet and know
 * what you are doing.
 */
#define CARDNAME "smc911x"

/*
 * Use power-down feature of the chip
 */
#define POWER_DOWN		 1


/* store this information for the driver.. */
struct smc911x_local {
	/*
	 * If I have to wait until the DMA is finished and ready to reload a
	 * packet, I will store the skbuff here. Then, the DMA will send it
	 * out and free it.
	 */
	struct sk_buff *pending_tx_skb;

	/*
	 * these are things that the kernel wants me to keep, so users
	 * can find out semi-useless statistics of how well the card is
	 * performing
	 */
	struct net_device_stats stats;

	/* version/revision of the SMC911x chip */
	u16 version;
	u16 revision;

	/* FIFO sizes */
	int tx_fifo_kb;
	int tx_fifo_size;
	int rx_fifo_size;
	int afc_cfg;

	/* Contains the current active receive/phy mode */
	int ctl_rfduplx;
	int ctl_rspeed;

	u32 msg_enable;
	u32 phy_type;
	struct mii_if_info mii;

	/* work queue */
	struct work_struct phy_configure;
	int work_pending;

	int tx_throttle;
	spinlock_t lock;

	struct net_device *netdev;

#ifdef SMC_USE_DMA
	/* DMA needs the physical address of the chip */
	u_long physaddr;
	int rxdma;
	int txdma;
	int rxdma_active;
	int txdma_active;
	struct sk_buff *current_rx_skb;
	struct sk_buff *current_tx_skb;
	struct device *dev;
#endif
};

#if SMC_DEBUG > 0
#define DBG(n, args...)				 \
	do {					 \
		if (SMC_DEBUG & (n))		 \
			printk(args);		 \
	} while (0)

#define PRINTK(args...)   printk(args)
#else
#define DBG(n, args...)   do { } while (0)
#define PRINTK(args...)   printk(KERN_DEBUG args)
#endif

#if SMC_DEBUG_PKTS > 0
static void PRINT_PKT(u_char *buf, int length)
{
	int i;
	int remainder;
	int lines;

	lines = length / 16;
	remainder = length % 16;

	for (i = 0; i < lines ; i ++) {
		int cur;
		for (cur = 0; cur < 8; cur++) {
			u_char a, b;
			a = *buf++;
			b = *buf++;
			printk("%02x%02x ", a, b);
		}
		printk("\n");
	}
	for (i = 0; i < remainder/2 ; i++) {
		u_char a, b;
		a = *buf++;
		b = *buf++;
		printk("%02x%02x ", a, b);
	}
	printk("\n");
}
#else
#define PRINT_PKT(x...)  do { } while (0)
#endif


/* this enables an interrupt in the interrupt mask register */
#define SMC_ENABLE_INT(x) do {				\
	unsigned int  __mask;				\
	unsigned long __flags;				\
	spin_lock_irqsave(&lp->lock, __flags);		\
	__mask = SMC_GET_INT_EN();			\
	__mask |= (x);					\
	SMC_SET_INT_EN(__mask);				\
	spin_unlock_irqrestore(&lp->lock, __flags);	\
} while (0)

/* this disables an interrupt from the interrupt mask register */
#define SMC_DISABLE_INT(x) do {				\
	unsigned int  __mask;				\
	unsigned long __flags;				\
	spin_lock_irqsave(&lp->lock, __flags);		\
	__mask = SMC_GET_INT_EN();			\
	__mask &= ~(x);					\
	SMC_SET_INT_EN(__mask);				\
	spin_unlock_irqrestore(&lp->lock, __flags);	\
} while (0)

/*
 * this does a soft reset on the device
 */
static void smc911x_reset(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int reg, timeout=0, resets=1;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);
	
	//DBG("reset now by dxl");
	/*	 Take out of PM setting first */
	if ((SMC_GET_PMT_CTRL() & PMT_CTRL_READY_) == 0) {
		/* Write to the bytetest will take out of powerdown */
		SMC_SET_BYTE_TEST(0);
		timeout=10;
printk("timeout \n");
		do {
			udelay(10);
			reg = SMC_GET_PMT_CTRL() & PMT_CTRL_READY_;
		} while ( timeout-- && !reg);
		if (timeout == 0) {
			printk("DXL3 ");
			PRINTK("%s: smc911x_reset timeout waiting for PM restore\n", dev->name);
			return;
		}
	}

	/* Disable all interrupts */
	spin_lock_irqsave(&lp->lock, flags);
	SMC_SET_INT_EN(0);
	spin_unlock_irqrestore(&lp->lock, flags);

	while (resets--) {
		SMC_SET_HW_CFG(HW_CFG_SRST_);						/* 进行软件复位，必须在PHY完全唤醒之后进行 */
		timeout=10;
		do {
			udelay(10);
			reg = SMC_GET_HW_CFG();							/* .bit0 = 1 */
			/* If chip indicates reset timeout then try again */
			if (reg & HW_CFG_SRST_TO_) {					/* .bit1 == 1? */
				PRINTK("%s: chip reset timeout, retrying...\n", dev->name);
				resets++;
				break;
			}
		} while ( timeout-- && (reg & HW_CFG_SRST_));
	}
	if (timeout == 0) {
		PRINTK("%s: smc911x_reset timeout waiting for reset\n", dev->name);
		return;
	}

	/* make sure EEPROM has finished loading before setting GPIO_CFG */
	timeout=1000;
	while ( timeout-- && (SMC_GET_E2P_CMD() & E2P_CMD_EPC_BUSY_)) {
		udelay(10);
	}
	if (timeout == 0){
		PRINTK("%s: smc911x_reset timeout waiting for EEPROM busy\n", dev->name);
		return;
	}

	/* Initialize interrupts */
	SMC_SET_INT_EN(0);								/* 关闭所有中断 */
	SMC_ACK_INT(-1);								/* 清除所有中断 */

	/* Reset the FIFO level and flow control settings */
	SMC_SET_HW_CFG((lp->tx_fifo_kb & 0xF) << 16);
//TODO: Figure out what appropriate pause time is
	SMC_SET_FLOW(FLOW_FCPT_ | FLOW_FCEN_);
	SMC_SET_AFC_CFG(lp->afc_cfg);


	/* Set to LED outputs */
	SMC_SET_GPIO_CFG(0x70070000);

	/*
	 * Deassert IRQ for 1*10us for edge type interrupts
	 * and drive IRQ pin push-pull
	 */
	SMC_SET_IRQ_CFG( (1 << 24) | INT_CFG_IRQ_EN_ | INT_CFG_IRQ_TYPE_ );

	/* clear anything saved */
	if (lp->pending_tx_skb != NULL) {
		dev_kfree_skb (lp->pending_tx_skb);
		lp->pending_tx_skb = NULL;
		lp->stats.tx_errors++;
		lp->stats.tx_aborted_errors++;
	}
}

/*
 * Enable Interrupts, Receive, and Transmit
 */
static void smc911x_enable(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned mask, cfg, cr;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);

	SMC_SET_MAC_ADDR(dev->dev_addr);

	/* Enable TX */
	cfg = SMC_GET_HW_CFG();
	cfg &= HW_CFG_TX_FIF_SZ_ | 0xFFF;
	cfg |= HW_CFG_SF_;
	SMC_SET_HW_CFG(cfg);
	SMC_SET_FIFO_TDA(0xFF);
	/* Update TX stats on every 64 packets received or every 1 sec */
	SMC_SET_FIFO_TSL(64);
	SMC_SET_GPT_CFG(GPT_CFG_TIMER_EN_ | 10000);

	spin_lock_irqsave(&lp->lock, flags);
	SMC_GET_MAC_CR(cr);
	cr |= MAC_CR_TXEN_ | MAC_CR_HBDIS_;
	SMC_SET_MAC_CR(cr);
	SMC_SET_TX_CFG(TX_CFG_TX_ON_);
	spin_unlock_irqrestore(&lp->lock, flags);

	/* Add 2 byte padding to start of packets */
	SMC_SET_RX_CFG((2<<8) & RX_CFG_RXDOFF_);			/* Rx包头部增加2字节的填充 */

	/* Turn on receiver and enable RX */
	if (cr & MAC_CR_RXEN_)
		DBG(SMC_DEBUG_RX, "%s: Receiver already enabled\n", dev->name);

	spin_lock_irqsave(&lp->lock, flags);
	SMC_SET_MAC_CR( cr | MAC_CR_RXEN_ );
	spin_unlock_irqrestore(&lp->lock, flags);

	/* Interrupt on every received packet */
	SMC_SET_FIFO_RSA(0x01);
	SMC_SET_FIFO_RSL(0x00);

	/* now, enable interrupts */
	mask = INT_EN_TDFA_EN_ | INT_EN_TSFL_EN_ | INT_EN_RSFL_EN_ |
		INT_EN_GPT_INT_EN_ | INT_EN_RXDFH_INT_EN_ | INT_EN_RXE_EN_ |
		INT_EN_PHY_INT_EN_;
	if (IS_REV_A(lp->revision))					/* 判断ID_REV寄存器低16位 */
		mask|=INT_EN_RDFL_EN_;
	else {
		mask|=INT_EN_RDFO_EN_;
	}
	SMC_ENABLE_INT(mask);						/* 使能发送和接收中断 */
}

/*
 * this puts the device in an inactive state
 */
static void smc911x_shutdown(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned cr;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", CARDNAME, __FUNCTION__);

	/* Disable IRQ's */
	SMC_SET_INT_EN(0);						/* 关闭所有中断 */

	/* Turn of Rx and TX */
	spin_lock_irqsave(&lp->lock, flags);
	SMC_GET_MAC_CR(cr);
	cr &= ~(MAC_CR_TXEN_ | MAC_CR_RXEN_ | MAC_CR_HBDIS_);
	SMC_SET_MAC_CR(cr);
	SMC_SET_TX_CFG(TX_CFG_STOP_TX_);
	spin_unlock_irqrestore(&lp->lock, flags);
}

static inline void smc911x_drop_pkt(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int fifo_count, timeout, reg;

	DBG(SMC_DEBUG_FUNC | SMC_DEBUG_RX, "%s: --> %s\n", CARDNAME, __FUNCTION__);
	fifo_count = SMC_GET_RX_FIFO_INF() & 0xFFFF;			/* 获取RX FIFO已用大小 */
	if (fifo_count <= 4) {
		/* Manually dump the packet data */
		while (fifo_count--)
			SMC_GET_RX_FIFO();							/* 读取4次RX_DATA_FIFO */
	} else	 {
		/* Fast forward through the bad packet */
		SMC_SET_RX_DP_CTRL(RX_DP_CTRL_FFWD_BUSY_);	/* RX data FIFO移动到下一个包，快速略过这个有问题的包 */
		timeout=50;
		do {												/* 等待RX data FIFO移动到下一个包头 */
			udelay(10);
			reg = SMC_GET_RX_DP_CTRL() & RX_DP_CTRL_FFWD_BUSY_;
		} while ( timeout-- && reg);
		if (timeout == 0) {
			PRINTK("%s: timeout waiting for RX fast forward\n", dev->name);
		}
	}
}

/*
 * This is the procedure to handle the receipt of a packet.
 * It should be called after checking for packet presence in
 * the RX status FIFO.	 It must be called with the spin lock
 * already held.
 */
static inline void	 smc911x_rcv(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	unsigned int pkt_len, status;
	struct sk_buff *skb;
	unsigned char *data;

	DBG(SMC_DEBUG_FUNC | SMC_DEBUG_RX, "%s: --> %s\n",
		dev->name, __FUNCTION__);
	status = SMC_GET_RX_STS_FIFO();							/* 读取RX Status FIFO Port */
	DBG(SMC_DEBUG_RX, "%s: Rx pkt len %d status 0x%08x \n",
		dev->name, (status & 0x3fff0000) >> 16, status & 0xc000ffff);
	pkt_len = (status & RX_STS_PKT_LEN_) >> 16;					/* RX Status Format中，bit16-29是len */
	if (status & RX_STS_ES_) {									/* 数据包错误 */
		/* Deal with a bad packet */
		lp->stats.rx_errors++;									/* 错误包数+1 */
		if (status & RX_STS_CRC_ERR_)							/* 包CRC错误 */
			lp->stats.rx_crc_errors++;
		else {
			if (status & RX_STS_LEN_ERR_)						/* 包长度错误 */
				lp->stats.rx_length_errors++;
			if (status & RX_STS_MCAST_)						/* ????多播，这个标志不属于RX_STS_ES_.???? */
				lp->stats.multicast++;
		}
		/* Remove the bad packet data from the RX FIFO */
		smc911x_drop_pkt(dev);
	} else {													/* 数据包无错误 */
		/* Receive a valid packet */
		/* Alloc a buffer with extra room for DMA alignment */
		skb=dev_alloc_skb(pkt_len+32);							/* 分配skb */
		if (unlikely(skb == NULL)) {								/* 分配skb失败，则丢弃该数据包 */
			PRINTK( "%s: Low memory, rcvd packet dropped.\n",
				dev->name);
			lp->stats.rx_dropped++;
			smc911x_drop_pkt(dev);
			return;
		}
		/* Align IP header to 32 bits
		 * Note that the device is configured to add a 2
		 * byte padding to the packet start, so we really
		 * want to write to the orignal data pointer */
		data = skb->data;
		skb_reserve(skb, 2);									/* 保留头部2字节 */
		skb_put(skb,pkt_len-4);								/* 放到缓冲区尾端，读取的pkt_len包含MAC的4字节CRC? */
#ifdef SMC_USE_DMA
		{
		unsigned int fifo;
		/* Lower the FIFO threshold if possible */
		fifo = SMC_GET_FIFO_INT();
		if (fifo & 0xFF) fifo--;
		DBG(SMC_DEBUG_RX, "%s: Setting RX stat FIFO threshold to %d\n",
			dev->name, fifo & 0xff);
		SMC_SET_FIFO_INT(fifo);
		/* Setup RX DMA */
		SMC_SET_RX_CFG(RX_CFG_RX_END_ALGN16_ | ((2<<8) & RX_CFG_RXDOFF_));
		lp->rxdma_active = 1;
		lp->current_rx_skb = skb;
		SMC_PULL_DATA(data, (pkt_len+2+15) & ~15);
		/* Packet processing deferred to DMA RX interrupt */
		}
#else
		SMC_SET_RX_CFG(RX_CFG_RX_END_ALGN4_ | ((2<<8) & RX_CFG_RXDOFF_));	/* 在准备读取RX Data FIFO Port前，修改包长对齐和偏移量有作用吗? */
																			/* 偏移2字节，+3字节是为什么?。这里和skb_put(skb,pkt_len-4)不对应，应该是pkt_len+2-4???? */
		SMC_PULL_DATA(data, pkt_len+2+3);									/* 从RX Data FIFO Port读取数据到data */										

		DBG(SMC_DEBUG_PKTS, "%s: Received packet\n", dev->name);
		PRINT_PKT(data, ((pkt_len - 4) <= 64) ? pkt_len - 4 : 64);
		dev->last_rx = jiffies;
		skb->dev = dev;
		skb->protocol = eth_type_trans(skb, dev);
		netif_rx(skb);														/* 提交skb到内核 */
		lp->stats.rx_packets++;												/* 接收包数+1 */
		lp->stats.rx_bytes += pkt_len-4;									/* 接收字节数，不包括MAC的CRC */
#endif
	}
}

/*
 * This is called to actually send a packet to the chip.
 */
static void smc911x_hardware_send_pkt(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	struct sk_buff *skb;
	unsigned int cmdA, cmdB, len;
	unsigned char *buf;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC | SMC_DEBUG_TX, "%s: --> %s\n", dev->name, __FUNCTION__);
	BUG_ON(lp->pending_tx_skb == NULL);

	skb = lp->pending_tx_skb;
	lp->pending_tx_skb = NULL;

	/* cmdA {25:24] data alignment [20:16] start offset [10:0] buffer length */
	/* cmdB {31:16] pkt tag [10:0] length */
#ifdef SMC_USE_DMA
	/* 16 byte buffer alignment mode */
	buf = (char*)((u32)(skb->data) & ~0xF);
	len = (skb->len + 0xF + ((u32)skb->data & 0xF)) & ~0xF;
	cmdA = (1<<24) | (((u32)skb->data & 0xF)<<16) |
			TX_CMD_A_INT_FIRST_SEG_ | TX_CMD_A_INT_LAST_SEG_ |
			skb->len;
#else
	buf = (char*)((u32)skb->data & ~0x3);						/* 数据起始地址4字节对齐 */
	len = (skb->len + 3 + ((u32)skb->data & 3)) & ~0x3;			/* 对齐后长度 = 原数据长度
																 * 		   + 数据起始地址对齐补齐的长度
																 * 然后再4字节对齐		
																 */
	cmdA = (((u32)skb->data & 0x3) << 16) |						/* 起始数据偏移量 */
			TX_CMD_A_INT_FIRST_SEG_ | TX_CMD_A_INT_LAST_SEG_ |	/* 该包仅一个数据段  */	
			skb->len;											/* 有效数据长度 */
#endif
	/* tag is packet length so we can use this in stats update later */
	cmdB = (skb->len  << 16) | (skb->len & 0x7FF);				/* 高16位是packet tag，用于让控制器识别是哪个包，LAN9220并不需要这个tag */

	DBG(SMC_DEBUG_TX, "%s: TX PKT LENGTH 0x%04x (%d) BUF 0x%p CMDA 0x%08x CMDB 0x%08x\n",
		 dev->name, len, len, buf, cmdA, cmdB);
	SMC_SET_TX_FIFO(cmdA);
	SMC_SET_TX_FIFO(cmdB);

	DBG(SMC_DEBUG_PKTS, "%s: Transmitted packet\n", dev->name);
	PRINT_PKT(buf, len <= 64 ? len : 64);

	/* Send pkt via PIO or DMA */
#ifdef SMC_USE_DMA
	lp->current_tx_skb = skb;
	SMC_PUSH_DATA(buf, len);
	/* DMA complete IRQ will free buffer and set jiffies */
#else
	SMC_PUSH_DATA(buf, len);				/* 输出对齐后的buf到TX DATA FIFO PORT
												不使用DMA */
	dev->trans_start = jiffies;					/* 保存时间标签 */
	dev_kfree_skb(skb);						/* 释放发送skb */
#endif
	spin_lock_irqsave(&lp->lock, flags);
	if (!lp->tx_throttle) {						/* 如果设置了节流标志，则清除节流标志，并通知内核启动队列 */
		netif_wake_queue(dev);
	}
	spin_unlock_irqrestore(&lp->lock, flags);
	SMC_ENABLE_INT(INT_EN_TDFA_EN_ | INT_EN_TSFL_EN_);		/* 使能TX FIFO Available中断和TX FIFO LEVEL中断*/
}

/*
 * Since I am not sure if I will have enough room in the chip's ram
 * to store the packet, I call this routine which either sends it
 * now, or set the card to generates an interrupt when ready
 * for the packet.
 */
static int smc911x_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	unsigned int free;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC | SMC_DEBUG_TX, "%s: --> %s\n",
		dev->name, __FUNCTION__);

	BUG_ON(lp->pending_tx_skb != NULL);

	free = SMC_GET_TX_FIFO_INF() & TX_FIFO_INF_TDFREE_;				/* 获取TX FIFO剩余空间 */
	DBG(SMC_DEBUG_TX, "%s: TX free space %d\n", dev->name, free);

	/* Turn off the flow when running out of space in FIFO */
	if (free <= SMC911X_TX_FIFO_LOW_THRESHOLD) {						/* TX FIFO剩余空间小于阈值 */
		DBG(SMC_DEBUG_TX, "%s: Disabling data flow due to low FIFO space (%d)\n",
			dev->name, free);
		spin_lock_irqsave(&lp->lock, flags);
		/* Reenable when at least 1 packet of size MTU present */
		SMC_SET_FIFO_TDA((SMC911X_TX_FIFO_LOW_THRESHOLD)/64);	/* 设置TX Data Available Level中断 */
		lp->tx_throttle = 1;											/* TX节流标志置1 */
		netif_stop_queue(dev);										/* 通知内核停止队列 */
		spin_unlock_irqrestore(&lp->lock, flags);
	}

	/* Drop packets when we run out of space in TX FIFO
	 * Account for overhead required for:
	 *
	 *	  Tx command words			 8 bytes
	 *	  Start offset				 15 bytes
	 *	  End padding				 15 bytes
	 */
	/* TX FIFO剩余空间不足一帧，则丢弃当前请求发送的skb */
	if (unlikely(free < (skb->len + 8 + 15 + 15))) {					/* skb->len包括MAC头，IP数据包，不包括以太网帧4字节CRC */
		printk("%s: No Tx free space %d < %d\n",
			dev->name, free, skb->len);
		lp->pending_tx_skb = NULL;								/* 丢弃当前skb */
		lp->stats.tx_errors++;
		lp->stats.tx_dropped++;
		dev_kfree_skb(skb);										/* 释放当前skb */
		return 0;
	}

#ifdef SMC_USE_DMA												/* 没有使用DMA */
	{
		/* If the DMA is already running then defer this packet Tx until
		 * the DMA IRQ starts it
		 */
		spin_lock_irqsave(&lp->lock, flags);
		if (lp->txdma_active) {
			DBG(SMC_DEBUG_TX | SMC_DEBUG_DMA, "%s: Tx DMA running, deferring packet\n", dev->name);
			lp->pending_tx_skb = skb;
			netif_stop_queue(dev);
			spin_unlock_irqrestore(&lp->lock, flags);
			return 0;
		} else {
			DBG(SMC_DEBUG_TX | SMC_DEBUG_DMA, "%s: Activating Tx DMA\n", dev->name);
			lp->txdma_active = 1;
		}
		spin_unlock_irqrestore(&lp->lock, flags);
	}
#endif
	lp->pending_tx_skb = skb;									/* 把请求发送的skb挂到priv结构中 */
	smc911x_hardware_send_pkt(dev);						/* 调用硬件发送，实际把数据写入TX FIFO */

	return 0;
}

/*
 * This handles a TX status interrupt, which is only called when:
 * - a TX error occurred, or
 * - TX of a packet completed.
 */
/* TX Status FIFO Level中断，表示TX Error发生或者TX Complete发生 */
static void smc911x_tx(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int tx_status;

	DBG(SMC_DEBUG_FUNC | SMC_DEBUG_TX, "%s: --> %s\n",
		dev->name, __FUNCTION__);

	/* Collect the TX status */
	/* 读取TX Status FIFO的状态数据，并处理 */
	while (((SMC_GET_TX_FIFO_INF() & TX_FIFO_INF_TSUSED_) >> 16) != 0) {		/* TX Status FIFO Used Space不为0，则读取TX Status FIFO */
		DBG(SMC_DEBUG_TX, "%s: Tx stat FIFO used 0x%04x\n",
			dev->name,
			(SMC_GET_TX_FIFO_INF() & TX_FIFO_INF_TSUSED_) >> 16);
		tx_status = SMC_GET_TX_STS_FIFO();									/* 读取TX Status FIFO Port */
		lp->stats.tx_packets++;												/**/
		lp->stats.tx_bytes += tx_status>>16;									/* 读取TX Status。smc911x_hardware_send_pkt()里向Packet Tag写入的是skb->len */
		DBG(SMC_DEBUG_TX, "%s: Tx FIFO tag 0x%04x status 0x%04x\n",
			dev->name, (tx_status & 0xffff0000) >> 16,
			tx_status & 0x0000ffff);
		/* count Tx errors, but ignore lost carrier errors when in
		 * full-duplex mode */
		if ((tx_status & TX_STS_ES_) && !(lp->ctl_rfduplx &&	/* 全双工模式下，不关心No Carrier和Loss of Carrier。????手册上只说明不关心No Carrier，没有说明不关心Loss of Carrier */
		    !(tx_status & 0x00000306))) {					/* Error Status置位 */
			lp->stats.tx_errors++;
		}
		if (tx_status & TX_STS_MANY_COLL_) {				/* 连续16次的发送冲突 */
			lp->stats.collisions+=16;
			lp->stats.tx_aborted_errors++;
		} else {
			lp->stats.collisions+=(tx_status & TX_STS_COLL_CNT_) >> 3;	/* 发送完成前，冲突计数 */
		}
		/* carrier error only has meaning for half-duplex communication */
		if ((tx_status & (TX_STS_LOC_ | TX_STS_NO_CARR_)) &&			/* 只在半双工模式关心No Carrier和Loss of Carrier */
		    !lp->ctl_rfduplx) {
			lp->stats.tx_carrier_errors++;
		}
		if (tx_status & TX_STS_LATE_COLL_) {
			lp->stats.collisions++;
			lp->stats.tx_aborted_errors++;
		}
	}
}


/*---PHY CONTROL AND CONFIGURATION-----------------------------------------*/
/*
 * Reads a register from the MII Management serial interface
 */

static int smc911x_phy_read(struct net_device *dev, int phyaddr, int phyreg)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int phydata;

	SMC_GET_MII(phyreg, phyaddr, phydata);

	DBG(SMC_DEBUG_MISC, "%s: phyaddr=0x%x, phyreg=0x%02x, phydata=0x%04x\n",
		__FUNCTION__, phyaddr, phyreg, phydata);
	return phydata;
}


/*
 * Writes a register to the MII Management serial interface
 */
static void smc911x_phy_write(struct net_device *dev, int phyaddr, int phyreg,
			int phydata)
{
	unsigned long ioaddr = dev->base_addr;

	DBG(SMC_DEBUG_MISC, "%s: phyaddr=0x%x, phyreg=0x%x, phydata=0x%x\n",
		__FUNCTION__, phyaddr, phyreg, phydata);

	SMC_SET_MII(phyreg, phyaddr, phydata);
}

/*
 * Finds and reports the PHY address (115 and 117 have external
 * PHY interface 118 has internal only
 */
static void smc911x_phy_detect(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	struct smc911x_local *lp = netdev_priv(dev);
	int phyaddr;
	unsigned int cfg, id1, id2;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);

	lp->phy_type = 0;

	/*
	 * Scan all 32 PHY addresses if necessary, starting at
	 * PHY#1 to PHY#31, and then PHY#0 last.
	 */
	switch(lp->version) {
		case 0x115:
		case 0x117:
			cfg = SMC_GET_HW_CFG();
			if (cfg & HW_CFG_EXT_PHY_DET_) {
				cfg &= ~HW_CFG_PHY_CLK_SEL_;
				cfg |= HW_CFG_PHY_CLK_SEL_CLK_DIS_;
				SMC_SET_HW_CFG(cfg);
				udelay(10); /* Wait for clocks to stop */

				cfg |= HW_CFG_EXT_PHY_EN_;
				SMC_SET_HW_CFG(cfg);
				udelay(10); /* Wait for clocks to stop */

				cfg &= ~HW_CFG_PHY_CLK_SEL_;
				cfg |= HW_CFG_PHY_CLK_SEL_EXT_PHY_;
				SMC_SET_HW_CFG(cfg);
				udelay(10); /* Wait for clocks to stop */

				cfg |= HW_CFG_SMI_SEL_;
				SMC_SET_HW_CFG(cfg);

				for (phyaddr = 1; phyaddr < 32; ++phyaddr) {

					/* Read the PHY identifiers */
					SMC_GET_PHY_ID1(phyaddr & 31, id1);		
					SMC_GET_PHY_ID2(phyaddr & 31, id2);

					/* Make sure it is a valid identifier */
					if (id1 != 0x0000 && id1 != 0xffff &&
					    id1 != 0x8000 && id2 != 0x0000 &&
					    id2 != 0xffff && id2 != 0x8000) {
						/* Save the PHY's address */
						lp->mii.phy_id = phyaddr & 31;
						lp->phy_type = id1 << 16 | id2;
						break;
					}
				}
			}
		default:
			/* Internal media only */
			/* LAN9220的lp->version是0x0000
			 * PHY寄存器需要通过MAC寄存器的MII_ACC和MII_DATA访问
			 * 而MII_ACC和MII_DATA需要通过MAC_CSR_CMD和MAC_CSR_DATA寄存器访问
			 */
			SMC_GET_PHY_ID1(1, id1);
			SMC_GET_PHY_ID2(1, id2);
			/* Save the PHY's address */
			lp->mii.phy_id = 1;										/* phy_id是1表示内部phy */
			lp->phy_type = id1 << 16 | id2;
			printk("LAN9220 lp->phy_type = 0x%08x\n", lp->phy_type);		/* lp->phy_type = 0x0007c0c3 */
	}

	DBG(SMC_DEBUG_MISC, "%s: phy_id1=0x%x, phy_id2=0x%x phyaddr=0x%d\n",
		dev->name, id1, id2, lp->mii.phy_id);
}

/*
 * Sets the PHY to a configuration as determined by the user.
 * Called with spin_lock held.
 */
static int smc911x_phy_fixed(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	int phyaddr = lp->mii.phy_id;
	int bmcr;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);

	/* Enter Link Disable state */
	SMC_GET_PHY_BMCR(phyaddr, bmcr);
	bmcr |= BMCR_PDOWN;
	SMC_SET_PHY_BMCR(phyaddr, bmcr);

	/*
	 * Set our fixed capabilities
	 * Disable auto-negotiation
	 */
	bmcr &= ~BMCR_ANENABLE;
	if (lp->ctl_rfduplx)
		bmcr |= BMCR_FULLDPLX;

	if (lp->ctl_rspeed == 100)
		bmcr |= BMCR_SPEED100;

	/* Write our capabilities to the phy control register */
	SMC_SET_PHY_BMCR(phyaddr, bmcr);

	/* Re-Configure the Receive/Phy Control register */
	bmcr &= ~BMCR_PDOWN;
	SMC_SET_PHY_BMCR(phyaddr, bmcr);

	return 1;
}

/*
 * smc911x_phy_reset - reset the phy
 * @dev: net device
 * @phy: phy address
 *
 * Issue a software reset for the specified PHY and
 * wait up to 100ms for the reset to complete.	 We should
 * not access the PHY for 50ms after issuing the reset.
 *
 * The time to wait appears to be dependent on the PHY.
 *
 */
static int smc911x_phy_reset(struct net_device *dev, int phy)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	int timeout;
	unsigned long flags;
	unsigned int reg;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s()\n", dev->name, __FUNCTION__);

	spin_lock_irqsave(&lp->lock, flags);
	reg = SMC_GET_PMT_CTRL();
	reg &= ~0xfffff030;
	reg |= PMT_CTRL_PHY_RST_;
	SMC_SET_PMT_CTRL(reg);
	spin_unlock_irqrestore(&lp->lock, flags);
	for (timeout = 2; timeout; timeout--) {
		msleep(50);
		spin_lock_irqsave(&lp->lock, flags);
		reg = SMC_GET_PMT_CTRL();
		spin_unlock_irqrestore(&lp->lock, flags);
		if (!(reg & PMT_CTRL_PHY_RST_)) {
			/* extra delay required because the phy may
			 * not be completed with its reset
			 * when PHY_BCR_RESET_ is cleared. 256us
			 * should suffice, but use 500us to be safe
			 */
			udelay(500);
		break;
		}
	}

	return reg & PMT_CTRL_PHY_RST_;
}

/*
 * smc911x_phy_powerdown - powerdown phy
 * @dev: net device
 * @phy: phy address
 *
 * Power down the specified PHY
 */
static void smc911x_phy_powerdown(struct net_device *dev, int phy)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int bmcr;

	/* Enter Link Disable state */
	SMC_GET_PHY_BMCR(phy, bmcr);
	bmcr |= BMCR_PDOWN;
	SMC_SET_PHY_BMCR(phy, bmcr);
}

/* 新增加的 */
/*
 * smc911x_phy_poweron - poweron phy
 * @dev: net device
 * @phy: phy address
 *
 * Power on the specified PHY
 */
/* smc911x_phy_powerdown会设置PHY的Basic Control Register，PHY进入掉电模式，
 * 却没有函数退出掉电模式。增加smc911x_phy_poweron()
 */
static void smc911x_phy_poweron(struct net_device *dev, int phy)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int bmcr;

	printk("smc911x_phy_poweron\n");
	/* Enter Link Disable state */
	SMC_GET_PHY_BMCR(phy, bmcr);
	bmcr &= ~BMCR_PDOWN;
	SMC_SET_PHY_BMCR(phy, bmcr);

	bmcr = (1 << 15);
	SMC_SET_PHY_BMCR(phy, bmcr);				/* software reset PHY,设置bit15会自清除 */
	while((bmcr & (1 << 15)) != 0)					/* 等待复位完成 */
	{
		udelay(500);
		SMC_GET_PHY_BMCR(phy, bmcr);
	}

	/* Enter Link Disable state */
	SMC_GET_PHY_BMCR(phy, bmcr);
	bmcr &= ~BMCR_PDOWN;
	SMC_SET_PHY_BMCR(phy, bmcr);

	SMC_GET_PHY_BMCR(phy, bmcr);
	printk("PHY_BMCR = 0x%08x\n", bmcr);

	mdelay(100);									/* 等待PHY完全唤醒 */
}
/* 新增加的 结束*/

/*
 * smc911x_phy_check_media - check the media status and adjust BMCR
 * @dev: net device
 * @init: set true for initialisation
 *
 * Select duplex mode depending on negotiation state.	This
 * also updates our carrier state.
 */
static void smc911x_phy_check_media(struct net_device *dev, int init)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	int phyaddr = lp->mii.phy_id;
	unsigned int bmcr, cr;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);

	if (mii_check_media(&lp->mii, netif_msg_link(lp), init)) {
		/* duplex state has changed */
		SMC_GET_PHY_BMCR(phyaddr, bmcr);
		SMC_GET_MAC_CR(cr);
		if (lp->mii.full_duplex) {
			DBG(SMC_DEBUG_MISC, "%s: Configuring for full-duplex mode\n", dev->name);
			bmcr |= BMCR_FULLDPLX;
			cr |= MAC_CR_RCVOWN_;
		} else {
			DBG(SMC_DEBUG_MISC, "%s: Configuring for half-duplex mode\n", dev->name);
			bmcr &= ~BMCR_FULLDPLX;
			cr &= ~MAC_CR_RCVOWN_;
		}
		SMC_SET_PHY_BMCR(phyaddr, bmcr);
		SMC_SET_MAC_CR(cr);
	}
}

/*
 * Configures the specified PHY through the MII management interface
 * using Autonegotiation.
 * Calls smc911x_phy_fixed() if the user has requested a certain config.
 * If RPC ANEG bit is set, the media selection is dependent purely on
 * the selection by the MII (either in the MII BMCR reg or the result
 * of autonegotiation.)  If the RPC ANEG bit is cleared, the selection
 * is controlled by the RPC SPEED and RPC DPLX bits.
 */
static void smc911x_phy_configure(struct work_struct *work)
{
	struct smc911x_local *lp = container_of(work, struct smc911x_local,
						phy_configure);
	struct net_device *dev = lp->netdev;
	unsigned long ioaddr = dev->base_addr;
	int phyaddr = lp->mii.phy_id;
	int my_phy_caps; /* My PHY capabilities */
	int my_ad_caps; /* My Advertised capabilities */
	int status;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s()\n", dev->name, __FUNCTION__);

	/*
	 * We should not be called if phy_type is zero.
	 */
	if (lp->phy_type == 0)
		 goto smc911x_phy_configure_exit_nolock;

	/* 这里好像缺少对PHY的Basic Control Register中Power Down位的清零操作 */
	/* 应该执行smc911x_phy_powerdown()的相反操作 */
//	smc911x_phy_poweron(dev, lp->mii.phy_id);

	if (smc911x_phy_reset(dev, phyaddr)) {					/* 复位PHY */
		printk("%s: PHY reset timed out\n", dev->name);
		goto smc911x_phy_configure_exit_nolock;
	}
	
	spin_lock_irqsave(&lp->lock, flags);

	/*
	 * Enable PHY Interrupts (for register 18)
	 * Interrupts listed here are enabled
	 */
	SMC_SET_PHY_INT_MASK(phyaddr, PHY_INT_MASK_ENERGY_ON_ |
		 PHY_INT_MASK_ANEG_COMP_ | PHY_INT_MASK_REMOTE_FAULT_ |
		 PHY_INT_MASK_LINK_DOWN_);

	/* If the user requested no auto neg, then go set his request */
	if (lp->mii.force_media) {
		smc911x_phy_fixed(dev);
		goto smc911x_phy_configure_exit;
	}

//#if 0/* if eth0 up 时，Auto negotiation NOT supported */
	/* Copy our capabilities from MII_BMSR to MII_ADVERTISE */
	SMC_GET_PHY_BMSR(phyaddr, my_phy_caps);
	if (!(my_phy_caps & BMSR_ANEGCAPABLE)) {
//		printk(KERN_INFO "Auto negotiation NOT supported\n");
		printk("Auto negotiation NOT supported\n");
		smc911x_phy_fixed(dev);
		goto smc911x_phy_configure_exit;
	}
	else
	{
		printk("Auto negotiation supported\n");
	}
//#endif

	/* CSMA capable w/ both pauses */
	my_ad_caps = ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;

	if (my_phy_caps & BMSR_100BASE4)
		my_ad_caps |= ADVERTISE_100BASE4;
	if (my_phy_caps & BMSR_100FULL)
		my_ad_caps |= ADVERTISE_100FULL;
	if (my_phy_caps & BMSR_100HALF)
		my_ad_caps |= ADVERTISE_100HALF;
	if (my_phy_caps & BMSR_10FULL)
		my_ad_caps |= ADVERTISE_10FULL;
	if (my_phy_caps & BMSR_10HALF)
		my_ad_caps |= ADVERTISE_10HALF;

	/* Disable capabilities not selected by our user */
	if (lp->ctl_rspeed != 100)
		my_ad_caps &= ~(ADVERTISE_100BASE4|ADVERTISE_100FULL|ADVERTISE_100HALF);

	 if (!lp->ctl_rfduplx)
		my_ad_caps &= ~(ADVERTISE_100FULL|ADVERTISE_10FULL);

	/* Update our Auto-Neg Advertisement Register */
	SMC_SET_PHY_MII_ADV(phyaddr, my_ad_caps);
	lp->mii.advertising = my_ad_caps;

	/*
	 * Read the register back.	 Without this, it appears that when
	 * auto-negotiation is restarted, sometimes it isn't ready and
	 * the link does not come up.
	 */
	udelay(10);
	SMC_GET_PHY_MII_ADV(phyaddr, status);

	DBG(SMC_DEBUG_MISC, "%s: phy caps=0x%04x\n", dev->name, my_phy_caps);
	DBG(SMC_DEBUG_MISC, "%s: phy advertised caps=0x%04x\n", dev->name, my_ad_caps);

	/* Restart auto-negotiation process in order to advertise my caps */
	SMC_SET_PHY_BMCR(phyaddr, BMCR_ANENABLE | BMCR_ANRESTART);

	smc911x_phy_check_media(dev, 1);

smc911x_phy_configure_exit:
	spin_unlock_irqrestore(&lp->lock, flags);
smc911x_phy_configure_exit_nolock:
	lp->work_pending = 0;
}

/*
 * smc911x_phy_interrupt
 *
 * Purpose:  Handle interrupts relating to PHY register 18. This is
 *	 called from the "hard" interrupt handler under our private spinlock.
 */
static void smc911x_phy_interrupt(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	int phyaddr = lp->mii.phy_id;					/* lp->mii.phy_id是1，表示内部PHY */
	int status;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);

	if (lp->phy_type == 0)							/* lp->phy_type = 0x0007c0c3 */
		return;

	smc911x_phy_check_media(dev, 0);
	/* read to clear status bits */
	SMC_GET_PHY_INT_SRC(phyaddr,status);
	DBG(SMC_DEBUG_MISC, "%s: PHY interrupt status 0x%04x\n",
		dev->name, status & 0xffff);
	DBG(SMC_DEBUG_MISC, "%s: AFC_CFG 0x%08x\n",
		dev->name, SMC_GET_AFC_CFG());
}

/*--- END PHY CONTROL AND CONFIGURATION-------------------------------------*/

/*
 * This is the main routine of the driver, to handle the device when
 * it needs some attention.
 */
static irqreturn_t smc911x_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	unsigned long ioaddr = dev->base_addr;
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int status, mask, timeout;
	unsigned int rx_overrun=0, cr, pkts;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);

	spin_lock_irqsave(&lp->lock, flags);			/* 自旋锁 */

	/* Spurious interrupt check */
	/* 检查是否是真的中断，即有中断标志且总中断输出是允许的 */
	if ((SMC_GET_IRQ_CFG() & (INT_CFG_IRQ_INT_ | INT_CFG_IRQ_EN_)) !=
		(INT_CFG_IRQ_INT_ | INT_CFG_IRQ_EN_)) {
		spin_unlock_irqrestore(&lp->lock, flags);
		return IRQ_NONE;
	}

	mask = SMC_GET_INT_EN();				/* 读取当前中断使能状态 */
	SMC_SET_INT_EN(0);						/* 失能所有中断 */

	/* set a timeout value, so I don't stay here forever */
	timeout = 8;


	do {
		status = SMC_GET_INT();				/* 读取当前中断状态 */

		DBG(SMC_DEBUG_MISC, "%s: INT 0x%08x MASK 0x%08x OUTSIDE MASK 0x%08x\n",
			dev->name, status, mask, status & ~mask);

		status &= mask;						/* 屏蔽未使能的中断 */
		if (!status)							/* 没有失能的中断，则返回 */
			break;

		/* Handle SW interrupt condition */
		if (status & INT_STS_SW_INT_) {			/* 软件中断 */
			SMC_ACK_INT(INT_STS_SW_INT_);	/* SMC_ACK_INT向INT_STS对应位写入1，清除该中断状态 */
			mask &= ~INT_EN_SW_INT_EN_;
		}
		/* Handle various error conditions */
		if (status & INT_STS_RXE_) {			/* 接收错误 */
			SMC_ACK_INT(INT_STS_RXE_);
			lp->stats.rx_errors++;
		}
		if (status & INT_STS_RXDFH_INT_) {					/* 丢弃数据包数溢出 */
			SMC_ACK_INT(INT_STS_RXDFH_INT_);
			lp->stats.rx_dropped+=SMC_GET_RX_DROP();	/* 获取已丢弃的数据包个数 */
		 }
		/* Undocumented interrupt-what is the right thing to do here? */
		if (status & INT_STS_RXDF_INT_) {
			SMC_ACK_INT(INT_STS_RXDF_INT_);
		}

		/* Rx Data FIFO exceeds set level */
		if (status & INT_STS_RDFL_) {						/* ????中断，手册上并没有定义 */
			if (IS_REV_A(lp->revision)) {
				rx_overrun=1;
				SMC_GET_MAC_CR(cr);
				cr &= ~MAC_CR_RXEN_;
				SMC_SET_MAC_CR(cr);
				DBG(SMC_DEBUG_RX, "%s: RX overrun\n", dev->name);
				lp->stats.rx_errors++;
				lp->stats.rx_fifo_errors++;
			}
			SMC_ACK_INT(INT_STS_RDFL_);
		}
		if (status & INT_STS_RDFO_) {						/* ????中断 */
			if (!IS_REV_A(lp->revision)) {
				SMC_GET_MAC_CR(cr);
				cr &= ~MAC_CR_RXEN_;
				SMC_SET_MAC_CR(cr);
				rx_overrun=1;
				DBG(SMC_DEBUG_RX, "%s: RX overrun\n", dev->name);
				lp->stats.rx_errors++;
				lp->stats.rx_fifo_errors++;
			}
			SMC_ACK_INT(INT_STS_RDFO_);
		}
		/* Handle receive condition */
		if ((status & INT_STS_RSFL_) || rx_overrun) {			/* RX FIFO到达阈值 */
			unsigned int fifo;
			DBG(SMC_DEBUG_RX, "%s: RX irq\n", dev->name);
			fifo = SMC_GET_RX_FIFO_INF();
			pkts = (fifo & RX_FIFO_INF_RXSUSED_) >> 16;		/* 获取RX FIFO中数据包个数 */
			DBG(SMC_DEBUG_RX, "%s: Rx FIFO pkts %d, bytes %d\n",
				dev->name, pkts, fifo & 0xFFFF );
			if (pkts != 0) {								/* RX FIFO中有数据包未读 */
#ifdef SMC_USE_DMA
				unsigned int fifo;
				if (lp->rxdma_active){
					DBG(SMC_DEBUG_RX | SMC_DEBUG_DMA,
						"%s: RX DMA active\n", dev->name);
					/* The DMA is already running so up the IRQ threshold */
					fifo = SMC_GET_FIFO_INT() & ~0xFF;
					fifo |= pkts & 0xFF;
					DBG(SMC_DEBUG_RX,
						"%s: Setting RX stat FIFO threshold to %d\n",
						dev->name, fifo & 0xff);
					SMC_SET_FIFO_INT(fifo);
				} else
#endif
				smc911x_rcv(dev);						/* 从RX Data FIFO Port读取数据包并提交给内核 */
			}
			SMC_ACK_INT(INT_STS_RSFL_);					/* 清除RX FIFO达到阈值中断标志 */
		}
		/* Handle transmit FIFO available */
		if (status & INT_STS_TDFA_) {						/* TX Data FIFO Available Interrupt，可以继续发送数据 */
			DBG(SMC_DEBUG_TX, "%s: TX data FIFO space available irq\n", dev->name);
			SMC_SET_FIFO_TDA(0xFF);					/* 为什么把TX Data FIFO Available Interrupt的阈值设置成最大值?这样就无法产生该中断了 */
			lp->tx_throttle = 0;							/* 清除节流标志 */
#ifdef SMC_USE_DMA
			if (!lp->txdma_active)
#endif
				netif_wake_queue(dev);					/* 通知内核可以启动队列 */
			SMC_ACK_INT(INT_STS_TDFA_);				/* 清除TX Data FIFO Available Interrupt中断标志 */
		}
		/* Handle transmit done condition */
#if 1
		if (status & (INT_STS_TSFL_ | INT_STS_GPT_INT_)) {	/* TX Status FIFO Level Interrupt或者GP Timer中断 */
			DBG(SMC_DEBUG_TX | SMC_DEBUG_MISC,
				"%s: Tx stat FIFO limit (%d) /GPT irq\n",
				dev->name, (SMC_GET_FIFO_INT() & 0x00ff0000) >> 16);	/* 打印TX Status Level */
			smc911x_tx(dev);												/* 处理TX Error和TX Complete中断 */
			SMC_SET_GPT_CFG(GPT_CFG_TIMER_EN_ | 10000);				/* 使能GP Timer */
			SMC_ACK_INT(INT_STS_TSFL_);									/* 清除TX Status FIFO Level中断标志 */
			SMC_ACK_INT(INT_STS_TSFL_ | INT_STS_GPT_INT_);				/* 清除TX Status FIFO Level中断标志、GP Timer中断标志 */
		}
#else
		if (status & INT_STS_TSFL_) {
			DBG(SMC_DEBUG_TX, "%s: TX status FIFO limit (%d) irq \n", dev->name, );
			smc911x_tx(dev);
			SMC_ACK_INT(INT_STS_TSFL_);
		}

		if (status & INT_STS_GPT_INT_) {
			DBG(SMC_DEBUG_RX, "%s: IRQ_CFG 0x%08x FIFO_INT 0x%08x RX_CFG 0x%08x\n",
				dev->name,
				SMC_GET_IRQ_CFG(),
				SMC_GET_FIFO_INT(),
				SMC_GET_RX_CFG());
			DBG(SMC_DEBUG_RX, "%s: Rx Stat FIFO Used 0x%02x "
				"Data FIFO Used 0x%04x Stat FIFO 0x%08x\n",
				dev->name,
				(SMC_GET_RX_FIFO_INF() & 0x00ff0000) >> 16,
				SMC_GET_RX_FIFO_INF() & 0xffff,
				SMC_GET_RX_STS_FIFO_PEEK());
			SMC_SET_GPT_CFG(GPT_CFG_TIMER_EN_ | 10000);
			SMC_ACK_INT(INT_STS_GPT_INT_);
		}
#endif

		/* Handle PHY interupt condition */
		if (status & INT_STS_PHY_INT_) {									/* 处理PHY_INT */
			DBG(SMC_DEBUG_MISC, "%s: PHY irq\n", dev->name);
			smc911x_phy_interrupt(dev);
			SMC_ACK_INT(INT_STS_PHY_INT_);								/* 清除PHY_INT标志 */								
		}
	} while (--timeout);

	/* restore mask state */
	SMC_SET_INT_EN(mask);												/* 恢复中断使能状态 */

	DBG(SMC_DEBUG_MISC, "%s: Interrupt done (%d loops)\n",
		dev->name, 8-timeout);

	spin_unlock_irqrestore(&lp->lock, flags);									/* 自旋锁解锁 */

	DBG(3, "%s: Interrupt done (%d loops)\n", dev->name, 8-timeout);

	return IRQ_HANDLED;
}

#ifdef SMC_USE_DMA
static void
smc911x_tx_dma_irq(int dma, void *data)
{
	struct net_device *dev = (struct net_device *)data;
	struct smc911x_local *lp = netdev_priv(dev);
	struct sk_buff *skb = lp->current_tx_skb;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);

	DBG(SMC_DEBUG_TX | SMC_DEBUG_DMA, "%s: TX DMA irq handler\n", dev->name);
	/* Clear the DMA interrupt sources */
	SMC_DMA_ACK_IRQ(dev, dma);
	BUG_ON(skb == NULL);
	dma_unmap_single(NULL, tx_dmabuf, tx_dmalen, DMA_TO_DEVICE);
	dev->trans_start = jiffies;
	dev_kfree_skb_irq(skb);
	lp->current_tx_skb = NULL;
	if (lp->pending_tx_skb != NULL)
		smc911x_hardware_send_pkt(dev);
	else {
		DBG(SMC_DEBUG_TX | SMC_DEBUG_DMA,
			"%s: No pending Tx packets. DMA disabled\n", dev->name);
		spin_lock_irqsave(&lp->lock, flags);
		lp->txdma_active = 0;
		if (!lp->tx_throttle) {
			netif_wake_queue(dev);
		}
		spin_unlock_irqrestore(&lp->lock, flags);
	}

	DBG(SMC_DEBUG_TX | SMC_DEBUG_DMA,
		"%s: TX DMA irq completed\n", dev->name);
}
static void
smc911x_rx_dma_irq(int dma, void *data)
{
	struct net_device *dev = (struct net_device *)data;
	unsigned long ioaddr = dev->base_addr;
	struct smc911x_local *lp = netdev_priv(dev);
	struct sk_buff *skb = lp->current_rx_skb;
	unsigned long flags;
	unsigned int pkts;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);
	DBG(SMC_DEBUG_RX | SMC_DEBUG_DMA, "%s: RX DMA irq handler\n", dev->name);
	/* Clear the DMA interrupt sources */
	SMC_DMA_ACK_IRQ(dev, dma);
	dma_unmap_single(NULL, rx_dmabuf, rx_dmalen, DMA_FROM_DEVICE);
	BUG_ON(skb == NULL);
	lp->current_rx_skb = NULL;
	PRINT_PKT(skb->data, skb->len);
	dev->last_rx = jiffies;
	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);
	netif_rx(skb);
	lp->stats.rx_packets++;
	lp->stats.rx_bytes += skb->len;

	spin_lock_irqsave(&lp->lock, flags);
	pkts = (SMC_GET_RX_FIFO_INF() & RX_FIFO_INF_RXSUSED_) >> 16;
	if (pkts != 0) {
		smc911x_rcv(dev);
	}else {
		lp->rxdma_active = 0;
	}
	spin_unlock_irqrestore(&lp->lock, flags);
	DBG(SMC_DEBUG_RX | SMC_DEBUG_DMA,
		"%s: RX DMA irq completed. DMA RX FIFO PKTS %d\n",
		dev->name, pkts);
}
#endif	 /* SMC_USE_DMA */

#ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Polling receive - used by netconsole and other diagnostic tools
 * to allow network i/o with interrupts disabled.
 */
static void smc911x_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	smc911x_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif

/* Our watchdog timed out. Called by the networking layer */
/*
 * 超时后直接丢弃所有已经写入LAN9220的TX FIFO，重新开始
 */
static void smc911x_timeout(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	int status, mask;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);

	spin_lock_irqsave(&lp->lock, flags);
	status = SMC_GET_INT();							/* 读取中断标志 */
	mask = SMC_GET_INT_EN();						/* 读取中断使能标志 */
	spin_unlock_irqrestore(&lp->lock, flags);
	DBG(SMC_DEBUG_MISC, "%s: INT 0x%02x MASK 0x%02x \n",
		dev->name, status, mask);

	/* Dump the current TX FIFO contents and restart */
	/* 丢弃当前TX FIFO所有内容 */
	mask = SMC_GET_TX_CFG();										/* 读取TX_CFG寄存器 */
	SMC_SET_TX_CFG(mask | TX_CFG_TXS_DUMP_ | TX_CFG_TXD_DUMP_);	/* Force TX Status Discard, Force TX Data Discard */
	/*
	 * Reconfiguring the PHY doesn't seem like a bad idea here, but
	 * smc911x_phy_configure() calls msleep() which calls schedule_timeout()
	 * which calls schedule().	 Hence we use a work queue.
	 */
	if (lp->phy_type != 0) {											/* lp->phy_type = 0x0007c0c3 */
		if (schedule_work(&lp->phy_configure)) {						/* 提交工作给一个工作队列，返回非0表示该工作已经在工作队列中，smc911x_phy_configure */
			lp->work_pending = 1;	
		}
	}

	/* We can accept TX packets again */
	dev->trans_start = jiffies;
	netif_wake_queue(dev);											/* 通知内核启动网络队列 */
}

/*
 * This routine will, depending on the values passed to it,
 * either make it accept multicast packets, go into
 * promiscuous mode (for TCPDUMP and cousins) or accept
 * a select set of multicast packets
 */
static void smc911x_set_multicast_list(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	unsigned int multicast_table[2];
	unsigned int mcr, update_multicast = 0;
	unsigned long flags;
	/* table for flipping the order of 5 bits */
	static const unsigned char invert5[] =
		{0x00, 0x10, 0x08, 0x18, 0x04, 0x14, 0x0C, 0x1C,
		 0x02, 0x12, 0x0A, 0x1A, 0x06, 0x16, 0x0E, 0x1E,
		 0x01, 0x11, 0x09, 0x19, 0x05, 0x15, 0x0D, 0x1D,
		 0x03, 0x13, 0x0B, 0x1B, 0x07, 0x17, 0x0F, 0x1F};


	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);

	spin_lock_irqsave(&lp->lock, flags);
	SMC_GET_MAC_CR(mcr);
	spin_unlock_irqrestore(&lp->lock, flags);

	if (dev->flags & IFF_PROMISC) {

		DBG(SMC_DEBUG_MISC, "%s: RCR_PRMS\n", dev->name);
		mcr |= MAC_CR_PRMS_;
	}
	/*
	 * Here, I am setting this to accept all multicast packets.
	 * I don't need to zero the multicast table, because the flag is
	 * checked before the table is
	 */
	else if (dev->flags & IFF_ALLMULTI || dev->mc_count > 16) {
		DBG(SMC_DEBUG_MISC, "%s: RCR_ALMUL\n", dev->name);
		mcr |= MAC_CR_MCPAS_;
	}

	/*
	 * This sets the internal hardware table to filter out unwanted
	 * multicast packets before they take up memory.
	 *
	 * The SMC chip uses a hash table where the high 6 bits of the CRC of
	 * address are the offset into the table.	If that bit is 1, then the
	 * multicast packet is accepted.  Otherwise, it's dropped silently.
	 *
	 * To use the 6 bits as an offset into the table, the high 1 bit is
	 * the number of the 32 bit register, while the low 5 bits are the bit
	 * within that register.
	 */
	else if (dev->mc_count)  {
		int i;
		struct dev_mc_list *cur_addr;

		/* Set the Hash perfec mode */
		mcr |= MAC_CR_HPFILT_;

		/* start with a table of all zeros: reject all */
		memset(multicast_table, 0, sizeof(multicast_table));

		cur_addr = dev->mc_list;
		for (i = 0; i < dev->mc_count; i++, cur_addr = cur_addr->next) {
			int position;

			/* do we have a pointer here? */
			if (!cur_addr)
				break;
			/* make sure this is a multicast address -
				shouldn't this be a given if we have it here ? */
			if (!(*cur_addr->dmi_addr & 1))
				 continue;

			/* only use the low order bits */
			position = crc32_le(~0, cur_addr->dmi_addr, 6) & 0x3f;

			/* do some messy swapping to put the bit in the right spot */
			multicast_table[invert5[position&0x1F]&0x1] |=
				(1<<invert5[(position>>1)&0x1F]);
		}

		/* be sure I get rid of flags I might have set */
		mcr &= ~(MAC_CR_PRMS_ | MAC_CR_MCPAS_);

		/* now, the table can be loaded into the chipset */
		update_multicast = 1;
	} else	 {
		DBG(SMC_DEBUG_MISC, "%s: ~(MAC_CR_PRMS_|MAC_CR_MCPAS_)\n",
			dev->name);
		mcr &= ~(MAC_CR_PRMS_ | MAC_CR_MCPAS_);

		/*
		 * since I'm disabling all multicast entirely, I need to
		 * clear the multicast list
		 */
		memset(multicast_table, 0, sizeof(multicast_table));
		update_multicast = 1;
	}

	spin_lock_irqsave(&lp->lock, flags);
	SMC_SET_MAC_CR(mcr);
	if (update_multicast) {
		DBG(SMC_DEBUG_MISC,
			"%s: update mcast hash table 0x%08x 0x%08x\n",
			dev->name, multicast_table[0], multicast_table[1]);
		SMC_SET_HASHL(multicast_table[0]);
		SMC_SET_HASHH(multicast_table[1]);
	}
	spin_unlock_irqrestore(&lp->lock, flags);
}


/*
 * Open and Initialize the board
 *
 * Set up everything, reset the card, etc..
 */
static int
smc911x_open(struct net_device *dev)
{
	unsigned int reg, timeout=0;
	unsigned long ioaddr = dev->base_addr;
		
	struct smc911x_local *lp = netdev_priv(dev);

	printk("smc911x_open\n");
	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);

	/*
	 * Check that the address is valid.  If its not, refuse
	 * to bring the device up.	 The user must specify an
	 * address using ifconfig eth0 hw ether xx:xx:xx:xx:xx:xx
	 */
	if (!is_valid_ether_addr(dev->dev_addr)) {
		PRINTK("%s: no valid ethernet hw addr\n", __FUNCTION__);
		return -EINVAL;
	}

	smc911x_phy_poweron(dev, lp->mii.phy_id);
	printk("%s %d\n", __FUNCTION__, __LINE__);

//	mdelay(10);											/* 根据手册HW_CFG寄存器的描述，只有PHY已经完成唤醒，才能向bit0写入1复位 */				
	/* reset the hardware */									
	smc911x_reset(dev);									/* HW_CFG寄存器的bit0写入1，复位MAC CSRS */
	
	printk("%s %d\n", __FUNCTION__, __LINE__);

	/* Configure the PHY, initialize the link state */
	smc911x_phy_configure(&lp->phy_configure);
	
	printk("%s %d\n", __FUNCTION__, __LINE__);
	/* Turn on Tx + Rx */
	smc911x_enable(dev);							/* 使能发送和接收中断 */
	printk("%s %d\n", __FUNCTION__, __LINE__);
	
	/* 调用内核函数，启动传输队列 */
	netif_start_queue(dev);

	return 0;
}

/*
 * smc911x_close
 *
 * this makes the board clean up everything that it can
 * and not talk to the outside world.	 Caused by
 * an 'ifconfig ethX down'
 */
static int smc911x_close(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);

	printk("smc911x_close\n");
	
	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);

	netif_stop_queue(dev);				/* 通知内核，停止队列 */
	netif_carrier_off(dev);

	/* clear everything */
	smc911x_shutdown(dev);

	if (lp->phy_type != 0) {
		/* We need to ensure that no calls to
		 * smc911x_phy_configure are pending.

		 * flush_scheduled_work() cannot be called because we
		 * are running with the netlink semaphore held (from
		 * devinet_ioctl()) and the pending work queue
		 * contains linkwatch_event() (scheduled by
		 * netif_carrier_off() above). linkwatch_event() also
		 * wants the netlink semaphore.
		 */
		while (lp->work_pending)
			schedule();
		smc911x_phy_powerdown(dev, lp->mii.phy_id);			/* PHY掉电 */
	}

	if (lp->pending_tx_skb) {
		dev_kfree_skb(lp->pending_tx_skb);
		lp->pending_tx_skb = NULL;
	}

	return 0;
}

/*
 * Get the current statistics.
 * This may be called with the card open or closed.
 */
static struct net_device_stats *smc911x_query_statistics(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);


	return &lp->stats;
}

/*
 * Ethtool support
 */
static int
smc911x_ethtool_getsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	int ret, status;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);
	cmd->maxtxpkt = 1;
	cmd->maxrxpkt = 1;

	if (lp->phy_type != 0) {
		spin_lock_irqsave(&lp->lock, flags);
		ret = mii_ethtool_gset(&lp->mii, cmd);
		spin_unlock_irqrestore(&lp->lock, flags);
	} else {
		cmd->supported = SUPPORTED_10baseT_Half |
				SUPPORTED_10baseT_Full |
				SUPPORTED_TP | SUPPORTED_AUI;

		if (lp->ctl_rspeed == 10)
			cmd->speed = SPEED_10;
		else if (lp->ctl_rspeed == 100)
			cmd->speed = SPEED_100;

		cmd->autoneg = AUTONEG_DISABLE;
		if (lp->mii.phy_id==1)
			cmd->transceiver = XCVR_INTERNAL;
		else
			cmd->transceiver = XCVR_EXTERNAL;
		cmd->port = 0;
		SMC_GET_PHY_SPECIAL(lp->mii.phy_id, status);
		cmd->duplex =
			(status & (PHY_SPECIAL_SPD_10FULL_ | PHY_SPECIAL_SPD_100FULL_)) ?
				DUPLEX_FULL : DUPLEX_HALF;
		ret = 0;
	}

	return ret;
}

static int
smc911x_ethtool_setsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int ret;
	unsigned long flags;

	if (lp->phy_type != 0) {
		spin_lock_irqsave(&lp->lock, flags);
		ret = mii_ethtool_sset(&lp->mii, cmd);
		spin_unlock_irqrestore(&lp->lock, flags);
	} else {
		if (cmd->autoneg != AUTONEG_DISABLE ||
			cmd->speed != SPEED_10 ||
			(cmd->duplex != DUPLEX_HALF && cmd->duplex != DUPLEX_FULL) ||
			(cmd->port != PORT_TP && cmd->port != PORT_AUI))
			return -EINVAL;

		lp->ctl_rfduplx = cmd->duplex == DUPLEX_FULL;

		ret = 0;
	}

	return ret;
}

static void
smc911x_ethtool_getdrvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	strncpy(info->driver, CARDNAME, sizeof(info->driver));
	strncpy(info->version, version, sizeof(info->version));
	strncpy(info->bus_info, dev->dev.parent->bus_id, sizeof(info->bus_info));
}

static int smc911x_ethtool_nwayreset(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int ret = -EINVAL;
	unsigned long flags;

	if (lp->phy_type != 0) {
		spin_lock_irqsave(&lp->lock, flags);
		ret = mii_nway_restart(&lp->mii);
		spin_unlock_irqrestore(&lp->lock, flags);
	}

	return ret;
}

static u32 smc911x_ethtool_getmsglevel(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	return lp->msg_enable;
}

static void smc911x_ethtool_setmsglevel(struct net_device *dev, u32 level)
{
	struct smc911x_local *lp = netdev_priv(dev);
	lp->msg_enable = level;
}

static int smc911x_ethtool_getregslen(struct net_device *dev)
{
	/* System regs + MAC regs + PHY regs */
	return (((E2P_CMD - ID_REV)/4 + 1) +
			(WUCSR - MAC_CR)+1 + 32) * sizeof(u32);
}

static void smc911x_ethtool_getregs(struct net_device *dev,
										 struct ethtool_regs* regs, void *buf)
{
	unsigned long ioaddr = dev->base_addr;
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned long flags;
	u32 reg,i,j=0;
	u32 *data = (u32*)buf;

	regs->version = lp->version;
	for(i=ID_REV;i<=E2P_CMD;i+=4) {
		data[j++] = SMC_inl(ioaddr,i);
	}
	for(i=MAC_CR;i<=WUCSR;i++) {
		spin_lock_irqsave(&lp->lock, flags);
		SMC_GET_MAC_CSR(i, reg);
		spin_unlock_irqrestore(&lp->lock, flags);
		data[j++] = reg;
	}
	for(i=0;i<=31;i++) {
		spin_lock_irqsave(&lp->lock, flags);
		SMC_GET_MII(i, lp->mii.phy_id, reg);
		spin_unlock_irqrestore(&lp->lock, flags);
		data[j++] = reg & 0xFFFF;
	}
}

static int smc911x_ethtool_wait_eeprom_ready(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int timeout;
	int e2p_cmd;

	e2p_cmd = SMC_GET_E2P_CMD();
	for(timeout=10;(e2p_cmd & E2P_CMD_EPC_BUSY_) && timeout; timeout--) {
		if (e2p_cmd & E2P_CMD_EPC_TIMEOUT_) {
			PRINTK("%s: %s timeout waiting for EEPROM to respond\n",
				dev->name, __FUNCTION__);
			return -EFAULT;
		}
		mdelay(1);
		e2p_cmd = SMC_GET_E2P_CMD();
	}
	if (timeout == 0) {
		PRINTK("%s: %s timeout waiting for EEPROM CMD not busy\n",
			dev->name, __FUNCTION__);
		return -ETIMEDOUT;
	}
	return 0;
}

static inline int smc911x_ethtool_write_eeprom_cmd(struct net_device *dev,
													int cmd, int addr)
{
	unsigned long ioaddr = dev->base_addr;
	int ret;

	if ((ret = smc911x_ethtool_wait_eeprom_ready(dev))!=0)
		return ret;
	SMC_SET_E2P_CMD(E2P_CMD_EPC_BUSY_ |
		((cmd) & (0x7<<28)) |
		((addr) & 0xFF));
	return 0;
}

static inline int smc911x_ethtool_read_eeprom_byte(struct net_device *dev,
													u8 *data)
{
	unsigned long ioaddr = dev->base_addr;
	int ret;

	if ((ret = smc911x_ethtool_wait_eeprom_ready(dev))!=0)
		return ret;
	*data = SMC_GET_E2P_DATA();
	return 0;
}

static inline int smc911x_ethtool_write_eeprom_byte(struct net_device *dev,
													 u8 data)
{
	unsigned long ioaddr = dev->base_addr;
	int ret;

	if ((ret = smc911x_ethtool_wait_eeprom_ready(dev))!=0)
		return ret;
	SMC_SET_E2P_DATA(data);
	return 0;
}

static int smc911x_ethtool_geteeprom(struct net_device *dev,
									  struct ethtool_eeprom *eeprom, u8 *data)
{
	u8 eebuf[SMC911X_EEPROM_LEN];
	int i, ret;

	for(i=0;i<SMC911X_EEPROM_LEN;i++) {
		if ((ret=smc911x_ethtool_write_eeprom_cmd(dev, E2P_CMD_EPC_CMD_READ_, i ))!=0)
			return ret;
		if ((ret=smc911x_ethtool_read_eeprom_byte(dev, &eebuf[i]))!=0)
			return ret;
		}
	memcpy(data, eebuf+eeprom->offset, eeprom->len);
	return 0;
}

static int smc911x_ethtool_seteeprom(struct net_device *dev,
									   struct ethtool_eeprom *eeprom, u8 *data)
{
	int i, ret;

	/* Enable erase */
	if ((ret=smc911x_ethtool_write_eeprom_cmd(dev, E2P_CMD_EPC_CMD_EWEN_, 0 ))!=0)
		return ret;
	for(i=eeprom->offset;i<(eeprom->offset+eeprom->len);i++) {
		/* erase byte */
		if ((ret=smc911x_ethtool_write_eeprom_cmd(dev, E2P_CMD_EPC_CMD_ERASE_, i ))!=0)
			return ret;
		/* write byte */
		if ((ret=smc911x_ethtool_write_eeprom_byte(dev, *data))!=0)
			 return ret;
		if ((ret=smc911x_ethtool_write_eeprom_cmd(dev, E2P_CMD_EPC_CMD_WRITE_, i ))!=0)
			return ret;
		}
	 return 0;
}

static int smc911x_ethtool_geteeprom_len(struct net_device *dev)
{
	 return SMC911X_EEPROM_LEN;
}

static const struct ethtool_ops smc911x_ethtool_ops = {
	.get_settings	 = smc911x_ethtool_getsettings,
	.set_settings	 = smc911x_ethtool_setsettings,
	.get_drvinfo	 = smc911x_ethtool_getdrvinfo,
	.get_msglevel	 = smc911x_ethtool_getmsglevel,
	.set_msglevel	 = smc911x_ethtool_setmsglevel,
	.nway_reset = smc911x_ethtool_nwayreset,
	.get_link	 = ethtool_op_get_link,
	.get_regs_len	 = smc911x_ethtool_getregslen,
	.get_regs	 = smc911x_ethtool_getregs,
	.get_eeprom_len = smc911x_ethtool_geteeprom_len,
	.get_eeprom = smc911x_ethtool_geteeprom,
	.set_eeprom = smc911x_ethtool_seteeprom,
};

/*
 * smc911x_findirq
 *
 * This routine has a simple purpose -- make the SMC chip generate an
 * interrupt, so an auto-detect routine can detect it, and find the IRQ,
 */
static int __init smc911x_findirq(unsigned long ioaddr)
{
	int timeout = 20;
	unsigned long cookie;

	DBG(SMC_DEBUG_FUNC, "--> %s\n", __FUNCTION__);

	cookie = probe_irq_on();

	/*
	 * Force a SW interrupt
	 */

	SMC_SET_INT_EN(INT_EN_SW_INT_EN_);

	/*
	 * Wait until positive that the interrupt has been generated
	 */
	do {
		int int_status;
		udelay(10);
		int_status = SMC_GET_INT_EN();
		if (int_status & INT_EN_SW_INT_EN_)
			 break;		/* got the interrupt */
	} while (--timeout);

	/*
	 * there is really nothing that I can do here if timeout fails,
	 * as autoirq_report will return a 0 anyway, which is what I
	 * want in this case.	 Plus, the clean up is needed in both
	 * cases.
	 */

	/* and disable all interrupts again */
	SMC_SET_INT_EN(0);

	/* and return what I found */
	return probe_irq_off(cookie);
}

/*
 * Function: smc911x_probe(unsigned long ioaddr)
 *
 * Purpose:
 *	 Tests to see if a given ioaddr points to an SMC911x chip.
 *	 Returns a 0 on success
 *
 * Algorithm:
 *	 (1) see if the endian word is OK
 *	 (1) see if I recognize the chip ID in the appropriate register
 *
 * Here I do typical initialization tasks.
 *
 * o  Initialize the structure if needed
 * o  print out my vanity message if not done so already
 * o  print out what type of hardware is detected
 * o  print out the ethernet address
 * o  find the IRQ
 * o  set up my private data
 * o  configure the dev structure with my subroutines
 * o  actually GRAB the irq.
 * o  GRAB the region
 */
static int __init smc911x_probe(struct net_device *dev, unsigned long ioaddr)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int i, retval;
	unsigned int val, chip_id, revision;
	const char *version_string;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __FUNCTION__);

	printk("smc911x:ioaddr=0x%08x\n", ioaddr);
	/* First, see if the endian word is recognized */
	val = SMC_GET_BYTE_TEST();						/* 读取BYTE_TEST寄存器, offset=0x64 */
	DBG(SMC_DEBUG_MISC, "%s: endian probe returned 0x%04x\n", CARDNAME, val);
	if (val != 0x87654321) {
		printk(KERN_ERR "Invalid chip endian 0x08%x\n",val);
		retval = -ENODEV;
		goto err_out;
	}

	/*
	 * check if the revision register is something that I
	 * recognize.	These might need to be added to later,
	 * as future revisions could be added.
	 */
	chip_id = SMC_GET_PN();							/* 读取ID_REV寄存器高16位, offset=0x50 */
	//chip_id = 0x2405;
	DBG(SMC_DEBUG_MISC, "%s: id probe returned 0x%04x\n", CARDNAME, chip_id);
	for(i=0;chip_ids[i].id != 0; i++) {					/* LAN9220的ID_REV寄存器值是0x9220 */
		if (chip_ids[i].id == chip_id) break;
	}
	if (!chip_ids[i].id) {
		printk(KERN_ERR "Unknown chip ID %04x\n", chip_id);
		retval = -ENODEV;
		goto err_out;
	}
	version_string = chip_ids[i].name;

	revision = SMC_GET_REV();						/* 读取ID_REV寄存器低16位, offset=0x50 */
	DBG(SMC_DEBUG_MISC, "%s: revision = 0x%04x\n", CARDNAME, revision);

	/* At this point I'll assume that the chip is an SMC911x. */
	DBG(SMC_DEBUG_MISC, "%s: Found a %s\n", CARDNAME, chip_ids[i].name);

	/* Validate the TX FIFO size requested */
	if ((tx_fifo_kb < 2) || (tx_fifo_kb > 14)) {
		printk(KERN_ERR "Invalid TX FIFO size requested %d\n", tx_fifo_kb);
		retval = -EINVAL;
		goto err_out;
	}

	/* fill in some of the fields */
	dev->base_addr = ioaddr;								/* 保存映射后的io地址 */
	lp->version = chip_ids[i].id;								/* 0x9220 */
	lp->revision = revision;
	printk("LAN9220 lp->version = 0x%04x\n", lp->version);		/* 打印芯片ID，是0x9220 */
	printk("LAN9220 lp->revision = 0x%04x\n", lp->revision);	/* 打印芯片revision，是0x0000 */
	lp->tx_fifo_kb = tx_fifo_kb;
	/* Reverse calculate the RX FIFO size from the TX */
	lp->tx_fifo_size=(lp->tx_fifo_kb<<10) - 512;			/* tx_fifo_size,8k-512B，为什么保留512字节? */
	lp->rx_fifo_size= ((0x4000 - 512 - lp->tx_fifo_size) / 16) * 15;

	/* 新增加的 */
//	smc911x_phy_poweron(dev, 1);

	/* Set the automatic flow control values */
	switch(lp->tx_fifo_kb) {
		/*
		 *	 AFC_HI is about ((Rx Data Fifo Size)*2/3)/64
		 *	 AFC_LO is AFC_HI/2
		 *	 BACK_DUR is about 5uS*(AFC_LO) rounded down
		 */
		case 2:/* 13440 Rx Data Fifo Size */
			lp->afc_cfg=0x008C46AF;break;
		case 3:/* 12480 Rx Data Fifo Size */
			lp->afc_cfg=0x0082419F;break;
		case 4:/* 11520 Rx Data Fifo Size */
			lp->afc_cfg=0x00783C9F;break;
		case 5:/* 10560 Rx Data Fifo Size */
			lp->afc_cfg=0x006E374F;break;
		case 6:/* 9600 Rx Data Fifo Size */
			lp->afc_cfg=0x0064328F;break;
		case 7:/* 8640 Rx Data Fifo Size */
			lp->afc_cfg=0x005A2D7F;break;
		case 8:/* 7680 Rx Data Fifo Size */
			lp->afc_cfg=0x0050287F;break;				/* 8 * 1024 / 16 * 15 = 7680 */
		case 9:/* 6720 Rx Data Fifo Size */
			lp->afc_cfg=0x0046236F;break;
		case 10:/* 5760 Rx Data Fifo Size */
			lp->afc_cfg=0x003C1E6F;break;
		case 11:/* 4800 Rx Data Fifo Size */
			lp->afc_cfg=0x0032195F;break;
		/*
		 *	 AFC_HI is ~1520 bytes less than RX Data Fifo Size
		 *	 AFC_LO is AFC_HI/2
		 *	 BACK_DUR is about 5uS*(AFC_LO) rounded down
		 */
		case 12:/* 3840 Rx Data Fifo Size */
			lp->afc_cfg=0x0024124F;break;
		case 13:/* 2880 Rx Data Fifo Size */
			lp->afc_cfg=0x0015073F;break;
		case 14:/* 1920 Rx Data Fifo Size */
			lp->afc_cfg=0x0006032F;break;
		 default:
			 PRINTK("%s: ERROR -- no AFC_CFG setting found",
				dev->name);
			 break;
	}

	DBG(SMC_DEBUG_MISC | SMC_DEBUG_TX | SMC_DEBUG_RX,
		"%s: tx_fifo %d rx_fifo %d afc_cfg 0x%08x\n", CARDNAME,
		lp->tx_fifo_size, lp->rx_fifo_size, lp->afc_cfg);

	spin_lock_init(&lp->lock);

	/* 设置网卡MAC地址 */
#if defined(CONFIG_MACH_SMDK6410) || defined(CONFIG_MACH_SMDK2450)|| defined(CONFIG_MACH_SMDK2416)
	dev->dev_addr[0] = 0x00;
	dev->dev_addr[1] = 0x09;
	dev->dev_addr[2] = 0xc0;
	dev->dev_addr[3] = 0xff;
	dev->dev_addr[4] = 0xec;
	dev->dev_addr[5] = 0x48;

	SMC_SET_MAC_ADDR(dev->dev_addr);
#endif

	/* Get the MAC address */
	SMC_GET_MAC_ADDR(dev->dev_addr);

	/* now, reset the chip, and put it into a known state */
	smc911x_reset(dev);

	/*
	 * If dev->irq is 0, then the device has to be banged on to see
	 * what the IRQ is.
	 *
	 * Specifying an IRQ is done with the assumption that the user knows
	 * what (s)he is doing.  No checking is done!!!!
	 */
	if (dev->irq < 1) {
		int trials;

		trials = 3;
		while (trials--) {
			dev->irq = smc911x_findirq(ioaddr);
			if (dev->irq)
				break;
			/* kick the card and try again */
			smc911x_reset(dev);
		}
	}
	if (dev->irq == 0) {
		printk("%s: Couldn't autodetect your IRQ. Use irq=xx.\n",
			dev->name);
		retval = -ENODEV;
		goto err_out;
	}
	dev->irq = irq_canonicalize(dev->irq);

	/* Fill in the fields of the device structure with ethernet values. */
	ether_setup(dev);

	dev->open = smc911x_open;
	dev->stop = smc911x_close;
	dev->hard_start_xmit = smc911x_hard_start_xmit;
	dev->tx_timeout = smc911x_timeout;
	dev->watchdog_timeo = msecs_to_jiffies(watchdog);
	dev->get_stats = smc911x_query_statistics;
	dev->set_multicast_list = smc911x_set_multicast_list;
	dev->ethtool_ops = &smc911x_ethtool_ops;		/* ethtool 是一个实用工具, 设计来给系统管理员以大量的控制网络接口的操作. 用 ethtool, 
												 * 可能来控制各种接口参数, 包括速度, 介质类型, 双工模式, DMA 环设置, 硬件校验和, LAN 唤醒操作 */
#ifdef CONFIG_NET_POLL_CONTROLLER
	dev->poll_controller = smc911x_poll_controller;
#endif

	/* lp->phy_configure是struct work_struct  */
	INIT_WORK(&lp->phy_configure, smc911x_phy_configure);	/* 初始化工作队列 */
	lp->mii.phy_id_mask = 0x1f;
	lp->mii.reg_num_mask = 0x1f;
	lp->mii.force_media = 0;
	lp->mii.full_duplex = 0;
	lp->mii.dev = dev;
	lp->mii.mdio_read = smc911x_phy_read;
	lp->mii.mdio_write = smc911x_phy_write;

	/*
	 * Locate the phy, if any.
	 */
	smc911x_phy_detect(dev);								/* 检查芯片挂接的PHY，包括内部的PHY */

	/* Set default parameters */
	lp->msg_enable = NETIF_MSG_LINK;
	lp->ctl_rfduplx = 1;
	lp->ctl_rspeed = 100;

	/* Grab the IRQ */
	retval = request_irq(dev->irq, &smc911x_interrupt, IRQF_SHARED, dev->name, dev);	/* 申请中断 */
	if (retval)
		goto err_out;

	set_irq_type(dev->irq, IRQT_FALLING);											/* 设置中断触发类型 */

#ifdef SMC_USE_DMA
	lp->rxdma = SMC_DMA_REQUEST(dev, smc911x_rx_dma_irq);
	lp->txdma = SMC_DMA_REQUEST(dev, smc911x_tx_dma_irq);
	lp->rxdma_active = 0;
	lp->txdma_active = 0;
	dev->dma = lp->rxdma;
#endif

	retval = register_netdev(dev);													/* 注册网络设备 */

	if (retval == 0) {
		/* now, print out the card info, in a short format.. */
		printk("%s: %s (rev %d) at %#lx IRQ %d",
			dev->name, version_string, lp->revision,
			dev->base_addr, dev->irq);

#ifdef SMC_USE_DMA
		if (lp->rxdma != -1)
			printk(" RXDMA %d ", lp->rxdma);

		if (lp->txdma != -1)
			printk("TXDMA %d", lp->txdma);
#endif
		printk("\n");
		//if (!is_valid_ether_addr(dev->dev_addr)) {
if (0) {
			printk("%s: Invalid ethernet MAC address. Please "
					"set using ifconfig\n", dev->name);
		} else {
			/* Print the Ethernet address */
			printk("%s: Ethernet addr: ", dev->name);
			for (i = 0; i < 5; i++)
				printk("%2.2x:", dev->dev_addr[i]);
			printk("%2.2x\n", dev->dev_addr[5]);
		}

		if (lp->phy_type == 0) {										/* lp->phy_type = 0x7C0C3 */
			PRINTK("%s: No PHY found\n", dev->name);
		} else if ((lp->phy_type & ~0xff) == LAN911X_INTERNAL_PHY_ID) {	/* 进入这里 */
			PRINTK("%s: LAN911x Internal PHY\n", dev->name);
		} else {
			PRINTK("%s: External PHY 0x%08x\n", dev->name, lp->phy_type);
		}
	}

err_out:
#ifdef SMC_USE_DMA
	if (retval) {
		if (lp->rxdma != -1) {
			SMC_DMA_FREE(dev, lp->rxdma);
		}
		if (lp->txdma != -1) {
			SMC_DMA_FREE(dev, lp->txdma);
		}
	}
#endif
	return retval;
}

/*
 * smc911x_init(void)
 *
 *	  Output:
 *	 0 --> there is a device
 *	 anything else, error
 */
static int smc911x_drv_probe(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct resource *res;
	struct smc911x_local *lp;
	unsigned int *addr;
	int ret;

	DBG(SMC_DEBUG_FUNC, "--> %s\n",  __FUNCTION__);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto out;
	}

	/*
	 * Request the regions.
	 */
	/* 申请0x08000000开始的0x100字节内存，对应SMC的nRCS1 */
	/* request_mem_region函数并没有做实际性的映射工作，只是告诉内核要使用一块内存地址，声明占有 */
	if (!request_mem_region(res->start, SMC911X_IO_EXTENT, CARDNAME)) {
		 ret = -EBUSY;
		 goto out;
	}

	ndev = alloc_etherdev(sizeof(struct smc911x_local));
	if (!ndev) {
		printk("%s: could not allocate device.\n", CARDNAME);
		ret = -ENOMEM;
		goto release_1;
	}
	SET_MODULE_OWNER(ndev);
	SET_NETDEV_DEV(ndev, &pdev->dev);			/* (ndev)->dev.parent = (&pdev->dev) */

	ndev->dma = (unsigned char)-1;
	ndev->irq = platform_get_irq(pdev, 0);			/* 设置中断号 */
	lp = netdev_priv(ndev);
	lp->netdev = ndev;

	addr = ioremap(res->start, SMC911X_IO_EXTENT);	/* 映射nRCS1的地址 */
	if (!addr) {
		ret = -ENOMEM;
		goto release_both;
	}

	platform_set_drvdata(pdev, ndev);				/* pdev->dev->driver_data = ndev */
	ret = smc911x_probe(ndev, (unsigned long)addr);	/* struct net_device *ndev, ioremap后的nRCS1的地址 */
	if (ret != 0) {
		platform_set_drvdata(pdev, NULL);
		iounmap(addr);
release_both:
		free_netdev(ndev);
release_1:
		release_mem_region(res->start, SMC911X_IO_EXTENT);
out:
		printk("%s: not found (%d).\n", CARDNAME, ret);
	}
#ifdef SMC_USE_DMA
	else {
		lp->physaddr = res->start;
		lp->dev = &pdev->dev;
	}
#endif

	return ret;
}

static int smc911x_drv_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct resource *res;

	DBG(SMC_DEBUG_FUNC, "--> %s\n", __FUNCTION__);
	platform_set_drvdata(pdev, NULL);

	unregister_netdev(ndev);

	free_irq(ndev->irq, ndev);

#ifdef SMC_USE_DMA
	{
		struct smc911x_local *lp = netdev_priv(ndev);
		if (lp->rxdma != -1) {
			SMC_DMA_FREE(dev, lp->rxdma);
		}
		if (lp->txdma != -1) {
			SMC_DMA_FREE(dev, lp->txdma);
		}
	}
#endif
	iounmap((void *)ndev->base_addr);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, SMC911X_IO_EXTENT);

	free_netdev(ndev);
	return 0;
}

static int smc911x_drv_suspend(struct platform_device *dev, pm_message_t state)
{
	struct net_device *ndev = platform_get_drvdata(dev);
	unsigned long ioaddr = ndev->base_addr;

	DBG(SMC_DEBUG_FUNC, "--> %s\n", __FUNCTION__);
	if (ndev) {
		if (netif_running(ndev)) {
			netif_device_detach(ndev);
			smc911x_shutdown(ndev);
#if POWER_DOWN
			/* Set D2 - Energy detect only setting */
			SMC_SET_PMT_CTRL(2<<12);
#endif
		}
	}
	return 0;
}

static int smc911x_drv_resume(struct platform_device *dev)
{
	struct net_device *ndev = platform_get_drvdata(dev);

	DBG(SMC_DEBUG_FUNC, "--> %s\n", __FUNCTION__);
	if (ndev) {
		struct smc911x_local *lp = netdev_priv(ndev);

		if (netif_running(ndev)) {
			smc911x_reset(ndev);
			smc911x_enable(ndev);
			if (lp->phy_type != 0)
				smc911x_phy_configure(&lp->phy_configure);
			netif_device_attach(ndev);
		}
	}
	return 0;
}

static struct platform_driver smc911x_driver = {
	.probe		 = smc911x_drv_probe,
	.remove	 = smc911x_drv_remove,
	.suspend	 = smc911x_drv_suspend,
	.resume	 = smc911x_drv_resume,
	.driver	 = {
		.name	 = CARDNAME,
	},
};

static int __init smc911x_init(void)
{
	return platform_driver_register(&smc911x_driver);
}

static void __exit smc911x_cleanup(void)
{
	platform_driver_unregister(&smc911x_driver);
}

module_init(smc911x_init);
module_exit(smc911x_cleanup);

