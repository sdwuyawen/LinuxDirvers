spi.bin : start.o clock.o sdram.o main.o mmu.o nand.o uart.o interrupt.o i2c.o at24cxx.o lcd.o gpio_spi.o spi_tft.o
	arm-linux-ld  -T spi.lds -o spi.elf $^
	arm-linux-objcopy -O binary spi.elf spi.bin
	arm-linux-objdump -D spi.elf > spi.dis

%.o : %.S
	arm-linux-gcc -g -c -O0 -o $@ $^

%.o : %.c
	arm-linux-gcc -g -c -O0 -o $@ $^ -fno-builtin 

clean:
	rm *.o *.elf *.bin *.dis *~ -rf
