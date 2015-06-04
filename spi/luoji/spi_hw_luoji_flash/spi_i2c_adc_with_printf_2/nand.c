///////////////////////////////////////////////////////////////
// 本程序只供学习使用，未经本公司许可，不得用于其它任何商业用途
// 适用开发板型号:Tiny2416、Mini2451、Tiny2451
// 技术论坛:www.arm9home.net
// 修改日期:2013/7/1
// 版权所有，盗版必究。
// Copyright(C) 广州友善之臂计算机科技有限公司
// All rights reserved							
///////////////////////////////////////////////////////////////

#define __REG(x)    			(*((volatile unsigned int *)(x)))
#define __REGb(x)				(*(volatile unsigned char *)(x))
#define NFCONF_REG				__REG(0x4e000000)
#define NFCONT_REG				__REG(0x4e000004)
#define NFCMD_REG				__REG(0x4e000008)
#define NFADDR_REG				__REG(0x4e00000c)
#define NFDATA_REG				__REG(0x4e000010)
#define NFDATA8_REG				__REGb(0x4e000010)
#define NFMECCDATA0_REG			__REG(0x4e000014)
#define NFMECCDATA1_REG			__REG(0x4e000018)
#define NFSECCDATA0_REG			__REG(0x4e00001c)
#define NFSBLK_REG				__REG(0x4e000020)
#define NFEBLK_REG				__REG(0x4e000024)
#define NFSTAT_REG				__REG(0x4e000028)
#define NFESTAT0_REG			__REG(0x4e00002c)
#define NFESTAT1_REG			__REG(0x4e000030)
#define NFMECC0_REG				__REG(0x4e000034)
#define NFMECC1_REG				__REG(0x4e000038)
#define NFSECC_REG				__REG(0x4e00003c)
#define NFMLCBITPT_REG			__REG(0x4e000040)
#define NFCONF_ECC_MLC			(1<<24)
#define NFCONF_ECC_1BIT			(0<<23)
#define NFCONF_ECC_4BIT			(2<<23)
#define NFCONF_ECC_8BIT			(1<<23)
#define NFCONT_ECC_ENC			(1<<18)
#define NFCONT_WP				(1<<16)
#define NFCONT_MECCLOCK			(1<<7)
#define NFCONT_SECCLOCK			(1<<6)
#define NFCONT_INITMECC			(1<<5)
#define NFCONT_INITSECC			(1<<4)
#define NFCONT_INITECC			(NFCONT_INITMECC | NFCONT_INITSECC)
#define NFCONT_CS_ALT			(1<<1)
#define NFCONT_CS				(1<<1)
#define NFSTAT_ECCENCDONE		(1<<7)
#define NFSTAT_ECCDECDONE		(1<<6)
#define NFSTAT_RnB				(1<<0)
#define NFESTAT0_ECCBUSY		(1<<31)
#define NAND_DISABLE_CE(nand)	(NFCONT_REG |= (1 << 1))
#define NAND_ENABLE_CE(nand)	(NFCONT_REG &= ~(1 << 1))
#define NF_TRANSRnB()			do { while(!(NFSTAT_REG & (1 << 0))); } while(0)

#define NAND_CMD_READ0			0
#define NAND_CMD_READ1			1
#define NAND_CMD_PAGEPROG		0x10
#define NAND_CMD_READOOB			0x50
#define NAND_CMD_ERASE1			0x60
#define NAND_CMD_STATUS			0x70
#define NAND_CMD_SEQIN			0x80
#define NAND_CMD_READID			0x90
#define NAND_CMD_ERASE2			0xd0
#define NAND_CMD_RESET			0xff
#define NAND_CMD_READSTART		0x30
#define NAND_CMD_RNDOUTSTART	0xE0
#define NAND_CMD_CACHEDPROG		0x15

void nand_init(void)
{
	// 设置NAND Flash控制器
	NFCONF_REG = ( (0x2<<12)|(0xf<<8)|(0x7<<4) );
	NFCONT_REG |= (0x3<<0);
}

// 读一页，即2048byte
static int nandll_read_page (unsigned char *buf, unsigned long addr)
{

	int i;
	int page_size = 2048;

	// 发片选
	NAND_ENABLE_CE();

	// 发读命令：0x00
	NFCMD_REG = NAND_CMD_READ0;
	// 发地址
	NFADDR_REG = 0;
	NFADDR_REG = 0;
	NFADDR_REG = (addr) & 0xff;
	NFADDR_REG = (addr >> 8) & 0xff;
	NFADDR_REG = (addr >> 16) & 0xff;
	// 发读命令：0x30
	NFCMD_REG = NAND_CMD_READSTART;

	// 等待数据
	NF_TRANSRnB();

	// 连续读2048个字节
	for(i=0; i < page_size; i++)
	{
		*buf++ = NFDATA8_REG;
	}

	// 取消片选
	NAND_DISABLE_CE();

	return 0;
}


// 从NAND中拷贝代码到DRAM
int copy2ddr(unsigned int nand_start, unsigned int ddr_start, unsigned int len)
{
	unsigned char *buf = (unsigned char *)ddr_start;
	int i;
	unsigned int page_shift = 11;

	// 发片选
	NAND_ENABLE_CE();

	// 使len为2048的整数倍
	len = (len/2048+1)*2048;

	// 循环拷贝，每次拷贝一页数据
	for (i = 0; i < (len>>page_shift); i++, buf+=(1<<page_shift))
	{
		// 读一页，即2048byte
		nandll_read_page(buf, i);
	}

	return 0;
}
