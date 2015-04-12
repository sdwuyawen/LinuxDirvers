/* 参考 
 * drivers\mtd\nand\s3c2410.c
 * drivers\mtd\nand\at91_nand.c
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/sched.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>
#include <asm/mach-types.h>

#include <asm/arch/nand.h>
#include <asm/arch/regs-nand.h>

/* S3C2416的nand寄存器定义 */
struct s3c_nand_regs {
	unsigned long nfconf  ;
	unsigned long nfcont  ;
	unsigned long nfcmmd   ;
	unsigned long nfaddr  ;
	unsigned long nfdata  ;
	unsigned long nfeccd0 ;
	unsigned long nfeccd1 ;
	unsigned long nfeccd  ;
	unsigned long nfsblk;
	unsigned long nfeblk;
	unsigned long nfstat  ;
	unsigned long nfeccerr0;
	unsigned long nfeccerr1;
	unsigned long nfmecc0 ;
	unsigned long nfmecc1 ;
	unsigned long nfsecc  ;
};
	
static struct nand_chip *s3c_nand;
static struct mtd_info *s3c_mtd;
static struct s3c_nand_regs *s3c_nand_regs;

static void s3c2416_select_chip(struct mtd_info *mtd, int chipnr)
{
	if (chipnr == -1)
	{
		/* 取消选中: NFCONT[1]设为1 */
		s3c_nand_regs->nfcont |= (1 << 1);
	}
	else
	{
		/* 选中: NFCONT[1]设为0 */
		s3c_nand_regs->nfcont &= ~(1 << 1);
	}
}

static void s3c2416_cmd_ctrl(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	if (ctrl & NAND_CLE)
	{
		/* 发命令: NFCMMD=dat */
		s3c_nand_regs->nfcmmd = cmd;
	}
	else
	{
		/* 发地址: NFADDR=dat */
		s3c_nand_regs->nfaddr = cmd;
	}
}

/*
 * 返回1表示ready,0表示busy
 */
int s3c2416_dev_ready(struct mtd_info *mtd)
{
	return (s3c_nand_regs->nfstat & (1<<0));
}

static int s3c_nand_init(void)
{
	struct clk *clk;
	
	/* 1. 分配一个nand_chip结构体 */
	s3c_nand = kzalloc(sizeof(struct nand_chip), GFP_KERNEL);
	s3c_nand_regs = ioremap(0x4E000000, sizeof(struct s3c_nand_regs));
	
	/* 2. 设置nand_chip */
	/* 设置nand_chip是给nand_scan函数使用的, 如果不知道怎么设置, 先看nand_scan怎么使用 
	 * 它应该提供:选中,发命令,发地址,发数据,读数据,判断状态的功能
	 */
	s3c_nand->select_chip = s3c2416_select_chip;
	s3c_nand->cmd_ctrl = s3c2416_cmd_ctrl;
	s3c_nand->IO_ADDR_R = &s3c_nand_regs->nfdata;
	s3c_nand->IO_ADDR_W = &s3c_nand_regs->nfdata;
	s3c_nand->dev_ready = s3c2416_dev_ready;
	s3c_nand->ecc.mode = NAND_ECC_SOFT;	/* enable ECC，使能ECC，否则insmod时有警告 */
										/* NAND_ECC_NONE selected by board driver. This is not recommended !! */
	
	/* 3. 硬件相关的设置: 根据NAND FLASH的手册设置时间参数 */
	/* 使能NAND FLASH控制器的时钟 */
	clk = clk_get(NULL, "nand");
	clk_enable(clk);              /* clock.c中"nand"的enable函数是空的???? */
	
	/* HCLK=133MHz
	 * TACLS:  发出CLE/ALE之后多长时间才发出nWE信号, 从NAND手册可知CLE/ALE与nWE可以同时发出,所以TACLS=0
	 * TWRPH0: nWE的脉冲宽度, HCLK x ( TWRPH0 + 1 ), 从NAND手册(Tcls)可知它要>=15ns, 所以TWRPH0>=1
	 * TWRPH1: nWE变为高电平后多长时间CLE/ALE才能变为低电平, 从NAND手册(Tclh)可知它要>=5ns, 所以TWRPH1>=0
	 */
#define TACLS   	0x02	//0x00
#define TWRPH0  0x0f	//0x01
#define TWRPH1  0x07	//0x00
	s3c_nand_regs->nfconf = (TACLS<<12) | (TWRPH0<<8) | (TWRPH1<<4);

	/* NFCONT: 
	 * BIT1-设为1, 取消片选 
	 * BIT0-设为1, 使能NAND FLASH控制器
	 */
	s3c_nand_regs->nfcont = (1<<1) | (1<<0);

	/* 4. 使用: nand_scan */
	s3c_mtd = kzalloc(sizeof(struct mtd_info), GFP_KERNEL);
	
	s3c_nand->priv = s3c_mtd;		/* link the private data structures */
	s3c_mtd->priv = s3c_nand;
	s3c_mtd->owner = THIS_MODULE;
	
	nand_scan(s3c_mtd, 1);		/* 识别NAND FLASH, 构造mtd_info */
	
	/* 5. add_mtd_partitions */

	return 0;
}


static void s3c_nand_exit(void)
{
	kfree(s3c_mtd);
	iounmap(s3c_nand_regs);
	kfree(s3c_nand);
}

module_init(s3c_nand_init);
module_exit(s3c_nand_exit);

MODULE_LICENSE("GPL");