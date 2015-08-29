//#include <asm/unaligned.h>
//#include "le_byteshift.h"
#include <linux/module.h>


static int unaligned_init(void)
{
	
	return 0;
}

static void unaligned_exit(void)
{
	int var[2] = {0 ,0};
    int *pint;

    pint = (int *)((char *)var + 1);


    *var = 0x11223344;
    *pint = 0x55667788;

    printk("&var = %p\n", var);
    printk("pint = %p\n", pint);

	printk("var[0] = %08x\n", var[0]);
	printk("var[1] = %08x\n", var[1]);

    printk("*pint = %08x\n", *pint);

}

module_init(unaligned_init);
module_exit(unaligned_exit);


MODULE_LICENSE("GPL");
