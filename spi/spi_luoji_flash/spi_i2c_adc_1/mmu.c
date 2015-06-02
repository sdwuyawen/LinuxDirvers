///////////////////////////////////////////////////////////////
// 本程序只供学习使用，未经本公司许可，不得用于其它任何商业用途
// 适用开发板型号:Tiny2416、Mini2451、Tiny2451
// 技术论坛:www.arm9home.net
// 修改日期:2013/7/1
// 版权所有，盗版必究。
// Copyright(C) 广州友善之臂计算机科技有限公司
// All rights reserved							
///////////////////////////////////////////////////////////////

#define REGISTER_USE_CACHE    0
#define MMU_BASE  			  0x31000000					 // 页表基址
#define MMU_FULL_ACCESS       (3 << 10)  					 // 访问权限  
#define MMU_DOMAIN            (0 << 5)   					 // 属于哪个域  
#define MMU_SPECIAL           (1 << 4)   					 // bit 4必须是1  
#define MMU_CACHEABLE         (1 << 3)   					 // 可以使用cache  
#define MMU_BUFFERABLE        (1 << 2)   					 // 可以使用write buffer  
#define MMU_SECDESC           (2)        					 // 表示这是段描述符  
#define ENABLE_DCACHE_ICACHE  1
#define MMU_SECDESC_WB        (MMU_FULL_ACCESS | MMU_DOMAIN | \
	                          MMU_SPECIAL | MMU_CACHEABLE | \
	                          MMU_BUFFERABLE | MMU_SECDESC)
#define MMU_SECDESC_NCNB      (MMU_FULL_ACCESS | MMU_DOMAIN | \
	                          MMU_SPECIAL | MMU_SECDESC)

// 初始化mmu
void mmu_init(void)
{
	int i;
	volatile unsigned long *table = (volatile unsigned long *)MMU_BASE;

	// 建立页表项
	for(i=0; i<=0x102; i++)									 // 寄存器
	{
		table[0x4a0+i] = (0x4a000000 + i * 0x100000) | MMU_SECDESC_NCNB;
	}

	for(i=0; i<0x40; i++)										 // 内存
	{
		table[0x300+i] = (0x30000000 + i * 0x100000) | MMU_SECDESC_WB;
	}
//	table[0x400] = 0x40000000 | MMU_SECDESC_WB;  		   	 // MMU使能前后的代码所在的空间, 保持它们的虚拟地址等于物理地址 
//	table[0x0]   = 0x40000000 | MMU_SECDESC_WB;  			 // 中断向量表映射到0地址

	/* 新增加的 */
	table[0x000]   = 0x00000000 | MMU_SECDESC_WB;  	//映射1M,0x100000
	table[0xC00]   = 0x30000000 | MMU_SECDESC_WB;  
	/* 新增加的 结束 */

	// 启动mmu
	__asm__ (

		"mov    r1, #0\n"
		"mcr    p15, 0, r1, c7, c7, 0\n"    				 // 使无效ICaches和DCaches  

		"mcr    p15, 0, r1, c7, c10, 4\n"   				 // drain write buffer on v4  
		"mcr    p15, 0, r1, c8, c7, 0\n"    				 // 使无效指令、数据TLB  


		"mcr p15, 0, %0, c2, c0, 0\n"						 // write TTB register  
		"mrc p15, 0, r1, c3, c0, 0\n" 						 // read domain 15:0 access permissions  
		"orr r1, r1, #3\n"            						 // domain 0, Accesses are not checked  
		"mcr p15, 0, r1, c3, c0, 0\n" 						 // write domain 15:0 access permissions  

		"mrc p15, 0, r1, c1, c0, 0\n" 						 // Read control register  

#if ENABLE_DCACHE_ICACHE
		"orr r1, r1, #(1<<2)\n"       						 // Data cache enable  
		"orr r1, r1, #(1<<12)\n"      						 // Instruction cache enable  
		"orr r1, r1, #(1<<14)\n"      						 // Round robin replacement  
#endif
		"orr r1, r1, #(1<<0)\n"       						 // MMU enable  

		"mcr p15,0,r1,c1, c0,0\n"     						 // write control register  
		:
		: "r" (table)
		: "r1"
	);
}

