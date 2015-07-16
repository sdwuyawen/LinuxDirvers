#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>

unsigned char *vm_addr;
unsigned char *km_addr;

static int mem_test_init(void)
{
//	s3c_lcd = framebuffer_alloc(0, NULL);	/* 参数0是额外分配的大小，设置为0 */

	vm_addr = vmalloc(128);
	printk("vmalloc = %08x\n", vm_addr);

	km_addr = kmalloc(128, GFP_KERNEL);
	printk("km_addr = %08x\n", km_addr);	
	
	return 0;
}

static void mem_test_exit(void)
{
	vfree(vm_addr);
	kfree(km_addr);	
}

module_init(mem_test_init);
module_exit(mem_test_exit); 

MODULE_LICENSE("GPL");
