KERN_DIR = /home/wu/workspace/kernel/linux-2.6.22.with_yaffs20070816

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= hspi_flash_drv.o
#obj-m	+= hspi-s3c2416.o
#obj-m	+= spi_mcp2515_drv.o
