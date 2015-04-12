//#include <asm/unaligned.h>
#include "le_byteshift.h"
#include <linux/module.h>


static int unaligned_init(void)
{
	
	return 0;
}

static void unaligned_exit(void)
{
	unsigned char array[4];
	unsigned long val;
	
	put_unaligned_le32(1234, array);
	val = get_unaligned_le32(array);
	val = get_unaligned_le16(array);
}

module_init(unaligned_init);
module_exit(unaligned_exit);


MODULE_LICENSE("GPL");
