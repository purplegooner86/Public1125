obj-m += test_vuln.o

ccflags-remove-y += -Werror=date-time
ccflags-y += -Wno-sequence-point
ccflags-y += -fno-inline

KERNEL_DIR = /home/purple/Documents/fuzzing_research/test_kernel_build/linux-2.6.32.16
PWD = $(shell pwd)

ARCH := arm
CROSS_COMPILE := /home/purple/x-tools-1.21/arm-unknown-linux-gnueabi/bin/arm-unknown-linux-gnueabi-

all:
	make -C $(KERNEL_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
clean:
	make -C $(KERNEL_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean