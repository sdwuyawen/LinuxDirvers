CROSS_COMPILE =/usr/local/arm/4.3.2/bin/arm-linux-
KERNELDIR =
CFLAGS = -I$(KERNELDIR)/include -O -Wall -static
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

ALL : test_usb_camera

test_usb_camera:  testcam.c
	$(CC) -o  test_usb_camera  testcam.c $(CFLAGS) -L/usr/local/arm/4.3.2/arm-none-linux-gnueabi/libc/lib/  -I/usr/local/arm/4.3.2/arm-none-linux-gnueabi/libc/usr/include 
 	
clean :
	rm -rf  test_usb_camera
