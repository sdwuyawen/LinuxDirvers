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

/* dma_alloc_writecombine()声明 */
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <asm/dma.h>
#include <asm/io.h>
/* class_create()声明 */
#include <linux/device.h>
/* request_irq()声明 */
#include <linux/interrupt.h>

static char *src;
static u32 src_phys;

static char *dst;
static u32 dst_phys;

int major = 0;
struct class *cls;

#define MEM_CPY_NO_DMA  0
#define MEM_CPY_DMA     	1

#define BUF_SIZE  (512*1024)

#define DMA0_BASE_ADDR  0x4B000000
#define DMA1_BASE_ADDR  0x4B000100
#define DMA2_BASE_ADDR  0x4B000200
#define DMA3_BASE_ADDR  0x4B000300
#define DMA4_BASE_ADDR  0x4B000400
#define DMA5_BASE_ADDR  0x4B000500

struct s3c_dma_regs {
	unsigned long disrc;
	unsigned long disrcc;
	unsigned long didst;
	unsigned long didstc;
	unsigned long dcon;
	unsigned long dstat;
	unsigned long dcsrc;
	unsigned long dcdst;
	unsigned long dmasktrig;
	unsigned long dmareqsel;
};

static volatile struct s3c_dma_regs *dma_regs;

static DECLARE_WAIT_QUEUE_HEAD(dma_waitq);
/* 中断事件标志, 中断服务程序将它置1，ioctl将它清0 */
static volatile int ev_dma = 0;

int s3c_dma_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int i = 0;
	
	switch (cmd)
	{
		case MEM_CPY_NO_DMA :
		{
			memset(src, 0x12, BUF_SIZE);
			memset(dst, 0x34, BUF_SIZE);
			
			for(i = 0; i < BUF_SIZE; i++)
			{
				*(dst+i) = *(src+i);
			}

			if (memcmp(src, dst, BUF_SIZE) == 0)
			{
				printk("MEM_CPY_NO_DMA OK\n");
			}
			else
			{
				printk("MEM_CPY_DMA ERROR\n");
			}
			
			break;
		}

		case MEM_CPY_DMA :
		{
			memset(src, 0x56, BUF_SIZE);
			memset(dst, 0x78, BUF_SIZE);
			
			ev_dma = 0;

			/* 把源,目的,长度告诉DMA */
			dma_regs->disrc      = src_phys;        /* 源的物理地址 */
			dma_regs->disrcc     = (0<<1) | (0<<0); /* 源位于AHB总线, 源地址递增 */
			dma_regs->didst      = dst_phys;        /* 目的的物理地址 */
			dma_regs->didstc     = (0<<2) | (0<<1) | (0<<0); /* 目的位于AHB总线, 目的地址递增 */
			/* dcon.bit27在软件触发时必须设置为1，否则完成一次atomic传输之后就停止了
			 * dcon.bit23是SW request mode,用软件触发必须设置为1 
			 */
			dma_regs->dcon       = (1<<30)|(1<<29)|(0<<28)|(1<<27)|(0<<24)|(1<<22)|(0<<20)|(BUF_SIZE<<0) | (1<<23);  /* 使能中断,单个传输,软件触发, */

			dma_regs->dmareqsel = (0<<0) | (0<<1);
			/* 启动DMA */
			dma_regs->dmasktrig  = (1<<1) | (1<<0);

			/* 休眠 */
			wait_event_interruptible(dma_waitq, ev_dma);	/* 在第二个参数为0时才会休眠 */
#if 0
			while(dma_regs->dstat & 0xFFFFF != 0)
			{
				printk("dma_regs->dstat=%08x\n", dma_regs->dstat);
			}
#endif
			if (memcmp(src, dst, BUF_SIZE) == 0)
			{
				printk("MEM_CPY_DMA OK\n");
			}
			else
			{
				printk("MEM_CPY_DMA ERROR\n");
			}
			
			break;
		}
	}	
	return 0;
}

static struct file_operations dma_fops = {
	.owner  = THIS_MODULE,
	.ioctl  = s3c_dma_ioctl,
};

static irqreturn_t s3c_dma_irq(int irq, void *devid)
{
	/* 唤醒 */
	ev_dma = 1;
	wake_up_interruptible(&dma_waitq);   /* 唤醒休眠的进程 */
	printk("s3c_dma_irq entered\n");
	return IRQ_HANDLED;
}

static int s3c_dma_init(void)
{
	if(request_irq(IRQ_S3C2443_DMA0, s3c_dma_irq, 0 ,"s3c_dma", (void *)1) != 0)
	{
		printk("can't request_irq for DMA\n");
		return -EBUSY;
	}
	/* 分配SRC, DST对应的缓冲区 */
	src = dma_alloc_writecombine(NULL, BUF_SIZE, &src_phys, GFP_KERNEL);
	if (NULL == src)
	{
		printk("can't alloc buffer for src\n");
		free_irq(IRQ_S3C2443_DMA0, (void *)1);
		return -ENOMEM;
	}
	printk("src = %08x\n", src);

	dst = dma_alloc_writecombine(NULL, BUF_SIZE, &dst_phys, GFP_KERNEL);
	if (NULL == dst)
	{
		printk("can't alloc buffer for dst\n");
		free_irq(IRQ_S3C2443_DMA0, (void *)1);
		dma_free_writecombine(NULL, BUF_SIZE, src, src_phys);
		return -ENOMEM;
	}
	printk("dst = %08x\n", dst);
	
	major = register_chrdev(0, "s3c_dma", &dma_fops);			/* /proc/s3c_dma */

	/* 为了自动创建设备节点 */
	cls = class_create(THIS_MODULE, "s3c_dma");
	class_device_create(cls, NULL, MKDEV(major, 0), NULL, "dma"); /* /dev/dma */

	dma_regs = ioremap(DMA0_BASE_ADDR, sizeof(struct s3c_dma_regs));

	return 0;
}


static void s3c_dma_exit(void)
{
	class_device_destroy(cls, MKDEV(major, 0));
	class_destroy(cls);
	unregister_chrdev(MKDEV(major, 0), "s3c_dma");

	dma_free_writecombine(NULL, BUF_SIZE, dst, dst_phys);
	dma_free_writecombine(NULL, BUF_SIZE, src, src_phys);

	free_irq(IRQ_S3C2443_DMA0, (void *)1);
}

module_init(s3c_dma_init);
module_exit(s3c_dma_exit);

MODULE_LICENSE("GPL");
