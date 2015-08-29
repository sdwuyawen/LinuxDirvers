#uvcvideo-objs  := uvc_driver.o uvc_queue.o uvc_v4l2.o uvc_video.o uvc_ctrl.o \
		  uvc_status.o uvc_isight.o

KERN_DIR = /home/wu/workspace/kernel/linux-2.6.22.with_yaffs20070816

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

#obj-$(CONFIG_USB_VIDEO_CLASS) += uvcvideo.o
obj-m += unaligned.o
