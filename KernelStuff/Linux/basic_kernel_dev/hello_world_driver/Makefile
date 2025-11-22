obj-m += hello_world.o

ccflags-remove-y += -Werror=date-time
ccflags-y += -Wno-sequence-point
ccflags-y += -fno-inline

KERNEL_DIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
