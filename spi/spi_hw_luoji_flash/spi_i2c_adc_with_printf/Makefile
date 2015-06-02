CC      = arm-linux-gcc
LD      = arm-linux-ld
AR      = arm-linux-ar
OBJCOPY = arm-linux-objcopy
OBJDUMP = arm-linux-objdump

INCLUDEDIR 	:= $(shell pwd)/include
#CFLAGS 		:= -Wall -O0 -fno-builtin
#CFLAGS 		:= -Wall -O0
CPPFLAGS   	:= -nostdinc -I$(INCLUDEDIR)
#CPPFLAGS   	:= 

export 	CC LD OBJCOPY OBJDUMP INCLUDEDIR CFLAGS CPPFLAGS AR
#export 	CC LD OBJCOPY OBJDUMP CFLAGS  AR

objs := start.o clock.o sdram.o main.o mmu.o nand.o uart.o interrupt.o i2c.o at24cxx.o lcd.o gpio_spi.o spi_tft.o spi_flash.o s3c2416_spi.o lib/libc.a

spi.bin : $(objs) 
	${LD}  -T spi.lds -o spi.elf $^
	${OBJCOPY} -O binary spi.elf spi.bin
	${OBJDUMP} -D spi.elf > spi.dis

.PHONY : lib/libc.a
lib/libc.a:	
	cd lib; make; cd ..

%.o : %.S
	${CC} ${CPPFLAGS} ${CFLAGS} -c -o $@ $<
#	arm-linux-gcc -g -c -O0 -o $@ $^

%.o : %.c
	${CC} ${CPPFLAGS} ${CFLAGS} -c -o $@ $<
#	arm-linux-gcc -g -c -O0 -o $@ $^ -fno-builtin 

clean:
	rm *.o *.elf *.bin *.dis *~ -rf;
#	cd lib; make clean; make; cd ..
