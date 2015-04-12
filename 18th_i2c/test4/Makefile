i2c.bin : start.o clock.o sdram.o main.o mmu.o nand.o uart.o interrupt.o i2c.o at24cxx.o lcd.o
	arm-linux-ld  -T i2c.lds -o i2c.elf $^
	arm-linux-objcopy -O binary i2c.elf i2c.bin
	arm-linux-objdump -D i2c.elf > i2c.dis

%.o : %.S
	arm-linux-gcc -g -c -O0 -o $@ $^

%.o : %.c
	arm-linux-gcc -g -c -O0 -o $@ $^ -fno-builtin 

clean:
	rm *.o *.elf *.bin *.dis *~ -rf
