obj-m := bridgefilter_drop.o

KERNEL_DIR = /path/to/linux-2.6.32.16
PWD = $(shell pwd)

CROSS_COMPILE=/path/to/arm-unknown-linux-gnueabi/bin/arm-unknown-linux-gnueabi-
ARCH=arm

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean