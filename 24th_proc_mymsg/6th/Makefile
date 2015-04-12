KERN_DIR = /home/wu/workspace/kernel/linux-2.6.22.with_yaffs20070816

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= mymsg.o first_drv.o
