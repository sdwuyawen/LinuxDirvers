KERN_DIR = /home/wu/workspace/kernel/linux-2.6.22.with_yaffs20070816
#KERN_DIR = /home/wu/workspace/new_partation/linux-2.6.22.with_yaffs20070816.bak9

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= usbmouse_as_key.o
