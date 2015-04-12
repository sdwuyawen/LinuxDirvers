///////////////////////////////////////////////////////////////
// 本程序只供学习使用，未经本公司许可，不得用于其它任何商业用途
// 适用开发板型号:Tiny2416、Mini2451、Tiny2451
// 技术论坛:www.arm9home.net
// 修改日期:2013/7/1
// 版权所有，盗版必究。
// Copyright(C) 广州友善之臂计算机科技有限公司
// All rights reserved							
///////////////////////////////////////////////////////////////



.global _start

_start:
	// 异常向量表
	b reset                				 /* 复位时,cpu跳到0地址 */
	b halt  	 						 /* cpu遇到不能识别的指令时 */
	b halt             	 		 		 /* swi异常，进入svc模式 */
	b halt							     /* 预取中止异常 */
	b halt     						     /* 数据访问异常 */
	b halt  				       	     /* 没用到 */
	b halt			           		     /* 中断异常 */
	b halt     				             /* 快中断异常 */

reset:
	// 关看门狗
	ldr r0, =0x53000000
	mov r1, #0
	str r1, [r0] 

	// 设置栈
	ldr sp, =0x00002000

	// 设置时钟
	bl clock_init

	// 初始化sdram
	bl sdr_ctrl_asm_init

	// 初始化NAND Flash
	bl nand_init
	
//	bl main

	// 重定位，把程序的代码段、数据段复制到它的链接地址去
	adr r0, _start   			
	ldr r1, =_start  			
	ldr r2, =bss_start      	
	sub r2, r2, r1
	cmp r0,r1
	beq clean_bss
	bl copy2ddr					
	cmp r0, #0
	bne halt
		
	// 清BSS，把BSS段对应的内存清零
clean_bss:
	ldr r0, =bss_start
	ldr r1, =bss_end
	mov r3, #0
	cmp r0, r1
	beq on_ddr
clean_loop:
	str r3, [r0], #4
	cmp r0, r1	
	bne clean_loop	

	// 初始化mmu
	bl mmu_init

	// 跳转
on_ddr:	
//	ldr sp, =(0x32000000+0x100000)
	ldr sp, =0x34000000
	ldr pc, =main
	
halt:
	b halt	


