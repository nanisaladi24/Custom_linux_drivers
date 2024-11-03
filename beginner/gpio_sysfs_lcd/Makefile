obj-m := gpio_sysfs.o
ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-
KERN_DIR=/home/nani/Nani/Learn/beaglebone_ldd/src_5.10_DT/linux/
HOST_KERN_DIR=/lib/modules/$(shell uname -r)/build/

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) modules
clean:
	make -C $(KERN_DIR) M=$(PWD) clean
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) clean
	rm -rf *.o *.elf
help:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) help
host:
	make -C $(HOST_KERN_DIR) M=$(PWD) modules

copy-dtb:
	scp /home/nani/Nani/Learn/beaglebone_ldd/src_5.10_DT/linux/arch/arm/boot/dts/am335x-boneblack.dtb debian@10.0.0.31:/home/debian/drivers
copy-drv:
	scp *.ko lcd_app debian@10.0.0.31:/home/debian/drivers

#app: lcd_app.o lcd.o gpio.o lcd_app.elf

# lcd_app.o: lcd_app.c
# 	$(CROSS_COMPILE)gcc -c -o $@ $^

# lcd.o: lcd.c
# 	$(CROSS_COMPILE)gcc -c -o $@ $^

# gpio.o: gpio.c
# 	$(CROSS_COMPILE)gcc -c -o $@ $^

# lcd_app.elf: lcd_app.o lcd.o gpio.o
# 	$(CROSS_COMPILE)gcc -o $@ $^

#### STATIC LINKING #####
app: lcd_app.o lcd.o gpio.o
	$(CROSS_COMPILE)gcc -static -o lcd_app $^

lcd_app.o: lcd_app.c
	$(CROSS_COMPILE)gcc -c -o $@ $^

lcd.o: lcd.c
	$(CROSS_COMPILE)gcc -c -o $@ $^

gpio.o: gpio.c
	$(CROSS_COMPILE)gcc -c -o $@ $^




