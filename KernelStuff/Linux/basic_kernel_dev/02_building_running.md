# Building and Running Linux Kernel Modules

## Building

For the most part, we can build kernel modules with a simple makefile like the following:  

```Makefile
obj-m += sample_module.o

ccflags-remove-y += -Werror=date-time
ccflags-y += -Wno-sequence-point
ccflags-y += -fno-inline

KERNEL_DIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
```

This would work as a makefile for a kernel module with 1 source file: `sample_module.c`  


<br />

## Insmod and Rmmod

To install a module:  
- `sudo insmod sample_module.ko`  

To remove a module:  
- `sudo rmmod sample_module`  

<br />

## Getting output  

You can print with `pr_info` and get output to dmesg (`sudo dmesg -w`)  

Define a `pr_fmt` to format the dms logs

<br />

## Hello World Module

See a basic hello world kernel module here: [hello_world](./hello_world_driver/hello_world.c)








