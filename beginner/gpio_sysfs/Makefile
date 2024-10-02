obj-m := gpio_sysfs.o #final output
ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-
KERN_DIR=/home/nani/Nani/Learn/beaglebone_ldd/src_5.10_DT/linux/
HOST_KERN_DIR=/lib/modules/$(shell uname -r)/build/

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) modules
clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) clean
help:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) help
host:
	make -C $(HOST_KERN_DIR) M=$(PWD) modules
copy-dtb:
	scp /home/nani/Nani/Learn/beaglebone_ldd/src_5.10_DT/linux/arch/arm/boot/dts/am335x-boneblack.dtb debian@10.0.0.31:/home/debian/drivers
copy-drv:
	scp *.ko debian@10.0.0.31:/home/debian/drivers

