#KERN_DIR = /home/wu/workspace/kernel/linux-2.6.22.with_yaffs20070816
#KERN_DIR = /home/wu/workspace/kernel/s3c-linux.jyx.3.2.50
KERN_DIR = /home/wu/workspace/kernel/test/kernel2416

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= forth_drv_3.o
