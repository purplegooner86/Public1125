# Arm 32 LE integrator Linux 2.4.19 Build

The goal is to build a cross compilation toolchain for Arm32 LE with crosstool. Then to build a Linux 2.4.19 kernel for an emulated integrator board    

This is a much different build than typical ct-ng builds because of how old the kernel is  

<br />

## Containerized Environment

I did this in an Ubuntu 12.04 container  

The command I am using to run the container:
```sh
docker run -it --rm -v "$PWD":/workspace ubuntu:12.04 /bin/bash
```

In 2025, here is an `/etc/apt/sources.list` which works for 1204:  
```sh
deb http://old-releases.ubuntu.com/ubuntu precise main restricted universe multiverse
deb http://old-releases.ubuntu.com/ubuntu precise-updates main restricted universe multiverse
deb http://old-releases.ubuntu.com/ubuntu precise-security main restricted universe multiverse
```

Prepare the container:
```sh
apt-get update

apt-get install -y autoconf automake bison bzip2 cmake curl flex gawk gperf help2man libncurses5-dev libtool make patch pkg-config rsync texinfo unzip wget xz-utils gcc g++ git
```

<br />

## Toolchain Build

All of the build instructions here done in the container  

Support for glibc and gcc old enough to build a 2.4 kernel predates `crosstool-ng`  
Instead, we have to go back to the ancient vanilla `crosstool`  

https://kegel.com/crosstool/  
There is a link for `crosstool-0.43.tar.gz`  

That gz is in a weird format, but the GUI should know how to extract it  

First, modify `demo-arm.sh`:  
```sh
# Change to:
TARBALLS_DIR=/workspace/downloads

# Uncomment this one and comment the rest of the evals:
eval `cat arm.dat gcc-2.95.3-glibc-2.2.5.dat` sh all.sh --notest
```

Modify `all.sh` to let you run as root:
```patch
- case x$USER in
- xroot) abort "Don't run all.sh or crosstool.sh as root, it's dangerous" ;;
-  *) ;;
- esac
```

Run `./demo-arm.sh`  
You will need to help it with the downloads  
For any downloads that it fails, get the file yourself and put it in `/workspace/downloads`  
I think almost everything can be found on the berkely gnu mirror  
  
After everything has downloaded, extracted, and been patched once, the build will still fail because there is more patching we have to do  

But, at this point, add `--nounpack` to the eval line in `demo-arm.sh`  
This prevents crosstool from re-extracting the sources every time which would prevent you from being able to make patches  

The sources all go in `crosstool-0.43/build/arm-unknown-linux-gnu/gcc-2.95.3-glibc-2.2.5`  

**Patches to `gcc-2.95.3/`**:  
```patch
--- a/gcc/config/arm/arm.c
+++ b/gcc/config/arm/arm.c
@@ 530
- arm_prog_mode = TARGET_APCS_32 ? PROG_MODE_PROG32 : PROG_MODE_PROG26;
+ arm_prgmode = TARGET_APCS_32 ? PROG_MODE_PROG32 : PROG_MODE_PROG26;
```

```patch
--- a/gcc/collect2.c
+++ b/gcc/collect2.c
@@ 1762
- redir_handle = open (redir, O_WRONLY | O_TRUNC | O_CREAT);
+ redir_handle = open (redir, O_WRONLY | O_TRUNC | O_CREAT, 0600);
```

Was having issues with several of the `.texi` files which are just being used for building documentation. So I nuked them:  
```sh
cd gcc
for f in *.texi; do > "$f"; done
```

**Transplant Binutils**:  

The `ar` `ranlib` and `addr2line` that crosstool builds will segfault when they are run  
To fix this, I "transplanted" working versions of those programs built for a (close enough) version of arm32le  

I basically repeated the steps in [arm32le_versatile_linux2.6_gcc4.6.md](./arm32le_versatile_linux2.6_gcc4.6.md) inside of my docker container  
The only adjustment I had to make was to remove a running as root check from `~/x-tools/lib/ct-ng.1.21.0/scripts/crosstool-NG.sh` after I built and installed `ct-ng`  

After I built that 2.6 toolchain, I took the three offending tools and put them in `/workspace/transplant_binutils`  
I also renamed them to change `gnueabi` to `gnu`. For example: `arm-unknown-linux-gnu-ar`  

Then, add the following to `crosstool.sh`:  
```sh
# Line 391 (right after Build binutils section):
cp /workspace/transplant_binutils/* /opt/crosstool/gcc-2.95.3-glibc-2.2.5/arm-unknown-linux-gnu/bin
```

After all this is done and `demo-arm.sh` is run one final time, the built toolchain gets put in `/opt/crosstool/gcc-2.95.3-glibc-2.2.5/arm-unknown-linux-gnu/`  

`cp -r` that into workspace  
The toolchain should run no problem on an Ubuntu 22.04 host, which I will be using for the rest of this build  

Unfortunately, the crosstool toolchain uses some absolute paths. So, if you want to move it to a different host, you will have to put it in `/opt/crosstool/gcc-2.95.3-glibc-2.2.5/`  

<br />

## Kernel Build

This is on Ubuntu 22.04 now with the toolchain moved to `/opt`  

Get `linux-2.4.19.tar.gz` from https://www.kernel.org/pub/linux/kernel/v2.4/  

Get `patch-2.4.19-rmk7.gz` from http://ftp.armlinux.org.uk/pub/linux/arm/kernel/v2.4/  

Extract both of them  

```sh
cd linux-2.4.19
patch -p1 < ../patch-2.4.19-rmk7
```

One small extra patch to linux source. Just add `#include <linux/kernel.h>` to the top of `arch/arm/mach-integrator/irq.c`  

```sh
export PATH=/opt/crosstool/gcc-2.95.3-glibc-2.2.5/arm-unknown-linux-gnu/bin:$PATH

arm-unknown-linux-gnu-gcc --version
# 2.95.3

# exporting ARCH and CROSS_COMPILE does not work
# That is why they are prepended to every command...

make ARCH=arm CROSS_COMPILE=arm-unknown-linux-gnu- integrator_config
make ARCH=arm CROSS_COMPILE=arm-unknown-linux-gnu- oldconfig
# Answered 'y' for the first 3, default ([enter]) for the rest

make ARCH=arm CROSS_COMPILE=arm-unknown-linux-gnu- menuconfig
# Filesystems > Added /dev and automount
# Kernel Hacking > added debugging info

make ARCH=arm CROSS_COMPILE=arm-unknown-linux-gnu- dep
make ARCH=arm CROSS_COMPILE=arm-unknown-linux-gnu- zImage

file vmlinux 
# vmlinux: ELF 32-bit LSB executable, ARM, version 1 (ARM) ...

ls -la arch/arm/boot/zImage
```

<br />

## Console Issues Debugging

This section is purely informational. It did not yield any patches to the kernel or Qemu, just a tiny change to the Qemu run command which will be documented below. So ok to skip  

I was having a lot of issues with the console not getting setup correctly and therefore not getting any output despite seeing `printk`'s in gdb  

Based on the kernel config, I knew the kernel was supposed to be using an amba uart console. So, I was passing in `console=ttyAMA0` on my kernel command line  

I was also confirming that my kernel command line was being parsed correctly in `start_kernel` > `parse_options`  

The most helpful debug step was to add a print to the `ambauart_console_init` function  

This is in drivers/serial/amba.c:
```C
// Also had to add this to top:
#include <linux/kernel.h>

void __init ambauart_console_init(void)
{
	struct console *p_console;
	register_console(&amba_console);
	printk("=== registered consoles ===\n");
	for (p_console = console_drivers; p_console; p_console = p_console->next) {
		printk("console: name=%s, index=%d, flags=0x%x ->write=%p\n",
			p_console->name, p_console->index, p_console->flags, p_console->write);
	}
	printk("=== end registered consoles ===\n");
}
```

That helped me figure out that no consoles were being registered correctly  

The issue was that in 2.4 `amba_console` name looks like this:
```c
static struct console amba_console = {
	name:		"ttyAM",
    // ...
}
```

It won't recognize `ttyAMA*` only `ttyAM*`  


<br />

## AP vs CP Issues

Qemu emulates an integratorcp board  
While it is not specified in the 2.4 source, the integrator target is in fact an integratorap target, not an integratorcp target  
It is easiest to verify this by looking at Linux 2.6 source for the integratorap and integratorcp targets and comparing it to the 2.4 source for the integrator target  

The easiest thing to look at is the similarities/differences between the io description tables between the three  

In 2.6 this is:  
arch/arm/mach-integrator/integrator_cp.c > `intcp_io_desc[]`
and  
arch/arm/mach-integrator/integrator_ap.c > `ap_io_desc[]`

In 2.4 this is:  
arch/arm/mach-integrator/mm.c > `integrator_io_desc[]`  

The 2.4 integrator io table is nearly identical to the 2.6 integratorap table  

It is important to understand and address the difference between the ap board and the cp board because our kernel is built for the ap target and qemu is emulating a cp target  

<br />

## PIC and SIC for integratorcp

One of the biggest differences between the two that we have to mimic is the integratorcp's use of a secondary interrupt controller (sic)  

You can see this in the io description for intcp:  
integrator_cp.c:
```c
// Entry in map_desc initcp_io_desc[]:
{
    .virtual	= IO_ADDRESS(0xca000000),
    .pfn		= __phys_to_pfn(0xca000000),
    .length		= SZ_4K,
    .type		= MT_DEVICE
},
```

There is no such entry in the 2.4 or 2.6 ap io desc  

We can find the corresponding io region being setup in the qemu source for integratorcp:  
```c
dev = sysbus_create_varargs(TYPE_INTEGRATOR_PIC, 0x14000000,
                            qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ),
                            qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_FIQ),
                            NULL);
for (i = 0; i < 32; i++) {
    pic[i] = qdev_get_gpio_in(dev, i);
}
sic = sysbus_create_simple(TYPE_INTEGRATOR_PIC, 0xca000000, pic[26]);
```

This creates the primary interrupt controller (PIC) and maps it to `0x14000000`  
It is mapping the output GPIOs of the PIC to the CPU IRQ and CPU FIQ lines   
It then creates the secondary interrupt controller (SIC) and maps it to `0xca000000`  
The SIC and PIC are the same model  
The SIC's output line is connected to the primary PIC input `26`  

The ap kernel will not expect this secondary interrupt controller and it may also cause issues, so we can essentially just delete it from the Qemu source like so:  
```patch
- DeviceState *dev, *sic, *icp;
+ DeviceState *dev, *icp;

- sic = sysbus_create_simple(TYPE_INTEGRATOR_PIC, 0xca000000, pic[26]);

- icp = sysbus_create_simple(TYPE_ICP_CONTROL_REGS, 0xcb000000, qdev_get_gpio_in(sic, 3));
+ icp = sysbus_create_simple(TYPE_ICP_CONTROL_REGS, 0xcb000000, NULL);

```

<br />

## AP vs CP Clock Issue

I was getting stuck here in the boot sequence:
```
Calibrating delay loop...
```

This is because the Kernel's IRQ handler for the clock device was never being called. I confirmed this by adding a print debug to:  
include/asm-arm/arch-integrator/time.h > `integrator_timer_interrupt()`  

Here is the code path that shows how this is wired:

include/asm/arch/platform.h:  
```C
#define INT_TIMERINT0   5
#define INT_TIMERINT1   6
#define INT_TIMERINT2   7
```
include/asm-arm/arch-integrator/irqs.h:  
```C
#define IRQ_TIMERINT0   INT_TIMERINT0
#define IRQ_TIMERINT1   INT_TIMERINT1
#define IRQ_TIMERINT2   INT_TIMERINT2
```
arch/arm/kernel/time.c:
```C
static struct irqaction timer_irq = {
	.name	= "timer",
	.flags	= SA_INTERRUPT,
};
```
include/asm-arm/arch-integrator/time.h:
```C
timer_irq.handler = integrator_timer_interrupt;
setup_arm_irq(IRQ_TIMERINT1, &timer_irq);
```

I also modified the Qemu source to confirm that irq 6 was actually being set by the timer hardware  

I did this by adding a print to:  
hw/arm/integratorcp.c > `icp_pic_set_irq()`

Interestingly the `level` value was always 0 when this was called which seemed wrong to me  

Ultimately, the issue was another AP/CP integrator difference  

In a 2.6 kernel, `integrator_time_init` is defined like so:  
This is from arch/arm/mach-integrator/core.c:
```C
void __init integrator_time_init(unsigned long reload, unsigned int ctrl)
{
	unsigned int timer_ctrl = TIMER_CTRL_ENABLE | TIMER_CTRL_PERIODIC; // = 0xc0
    // ...
    timer_ctrl |= ctrl;
    //...
    writel(timer_ctrl, TIMER1_VA_BASE + TIMER_CTRL);
    // ...
}
```
That function is called differently for the cp board code vs the ap board code:  
arch/arm/mach-integrator/integrator_cp.c:  
```C
#define TIMER_CTRL_IE (1 << 5) /* Interrupt Enable */

static void __init intcp_timer_init(void)
{
	integrator_time_init(1000000 / HZ, TIMER_CTRL_IE);
}
```

arch/arm/mach-integrator/integrator_ap.c:
```C
static void __init ap_init_timer(void)
{
	integrator_time_init(1000000 * TICKS_PER_uSEC / HZ, 0);
}
```

So, for ap `timer_ctrl` remains `0xc0` but for cp `timer_ctrl` gets or'd with 32 which makes it equal to `0xe0`  

I added prints to Qemu's:  
hw/timer/arm_timer.c > `icp_pit_write`  
This confirmed that the value being written to the `TIMER_CTRL` offset was in face `0xc0` instead of `0xe0`  
For an integratorcp board (or an emulated one) this means the board interprets the enable bit as being unset  

In the 2.4 Kernel, the corresponding code is in:  
include/asm-arm/arch-integrator/time.h:  
```C
// (this is defined elsewhere)
#define TIMER_CTRL	0x80

static inline void setup_timer(void)
{
    // ...
	timer1->TimerControl = TIMER_CTRL | 0x40;	/* periodic */
    // ...
}
```

So, like the 2.6 ap board code, `0xc0` is being used instead of `0xe0`  

I patched the kernel like so:
```patch
- timer1->TimerControl = TIMER_CTRL | 0x40;	/* periodic */
+ timer1->TimerControl = 0xE0;
```

<br />

## Running

If you are using gef, the `file` command will fail because of how old this vmlinux is  
To fix this, modify `.gef.py` and add the following to the bottom of the `OsAbi` class:  
```python
OLDARMLINUX = 97
```  

Qemu Run command (WIP (no rootfs yet)):
```sh
qemu-system-arm \
    -M integratorcp \
    -kernel /path/to/arch/arm/boot/zImage \
    -append "console=ttyAM0" \
    -monitor unix:/tmp/qemu-monitor,server,nowait \
    -serial mon:stdio -nographic
```
