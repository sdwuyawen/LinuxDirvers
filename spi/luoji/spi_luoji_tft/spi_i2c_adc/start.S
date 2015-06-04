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
//******************************************************************************       
// 中断向量，本程序中，除Reset和HandleIRQ外，其它异常都没有使用
//******************************************************************************       
    b   Reset

// 0x04: 未定义指令中止模式的向量地址
HandleUndef:
    b   HandleUndef 
 
// 0x08: 管理模式的向量地址，通过SWI指令进入此模式
HandleSWI:
    b   HandleSWI

// 0x0c: 指令预取终止导致的异常的向量地址
HandlePrefetchAbort:
    b   HandlePrefetchAbort

// 0x10: 数据访问终止导致的异常的向量地址
HandleDataAbort:
    b   HandleDataAbort

// 0x14: 保留
HandleNotUsed:
    b   HandleNotUsed

// 0x18: 中断模式的向量地址
    b   HandleIRQ

// 0x1c: 快中断模式的向量地址
HandleFIQ:
    b   HandleFIQ

Reset:
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
	sub r2, r2, r1			//要复制的长度
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

	msr cpsr_c, #0xd2       // 进入中断模式
	ldr sp, =0x31000000     // 设置中断模式栈指针

	msr cpsr_c, #0xdf       // 进入系统模式
	ldr sp, =0x34000000     // 设置系统模式栈指针，

	ldr lr, =ret_initirq    // 设置返回地址    
	ldr pc, =init_irq       // 调用中断初始化函数
ret_initirq:
	msr cpsr_c, #0x5f       // 设置I-bit=0，开IRQ中断

	// 初始化mmu
	bl mmu_init

	// 跳转
on_ddr:	
//	ldr sp, =0x34000000
	ldr lr, =halt		// 设置返回地址
	ldr pc, =main			// 调用main函数
	
halt:
	b halt	


HandleIRQ:
    sub lr, lr, #4                  // 计算返回地址
    stmdb   sp!,    { r0-r12,lr }   // 保存使用到的寄存器
                                    // 注意，此时的sp是中断模式的sp
                                    // 初始值是上面设置的4096
    
    ldr lr, =int_return             // 设置调用IRQ_Handle函数后的返回地址  
    ldr pc, =IRQ_Handle             // 调用中断分发函数，在interrupt.c中
int_return:
    ldmia   sp!,    { r0-r12,pc }^  // 中断返回, ^表示将spsr的值复制到cpsr
    