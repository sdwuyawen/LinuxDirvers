/*
 * =====================================================================================
 *
 *       Filename:  mymsg.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/08/2015 08:58:13 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <linux/proc_fs.h>

struct proc_dir_entry *myentry;

const struct file_operations proc_mymsg_operations = {
};

static int mymsg_init(void)
{
	myentry = create_proc_entry("mymsg", S_IRUSR, &proc_root);
	if (myentry)
		myentry->proc_fops = &proc_mymsg_operations;
	return 0;
}

static void mymsg_exit(void)
{
	remove_proc_entry("mymsg", &proc_root);
}

module_init(mymsg_init);
module_exit(mymsg_exit);

MODULE_LICENSE("GPL");

