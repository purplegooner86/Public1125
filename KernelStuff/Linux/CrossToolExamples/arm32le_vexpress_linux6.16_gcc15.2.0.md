# ARM 32 LE vexpress Build (ct-ng and Buildroot)

The goal is to build a cross-compilation toolchain for Arm32 LE cortex-a9 with crosstool-ng. Then, to build a Linux 6.16 kernel for an emulated vexpress board. Finally, to build a simple rootfs for that kernel with buildroot  

This was all done in an Ubuntu 22.04 VM  

In all of these steps, I may have forgotten to include things that I did. I did not write this writeup concurrently while I was doing this  

<br />

## Building crosstool-ng 1.28.0

1.28.0 is the most recent version of ct-ng at the time I am doing this  

Install dependencies:
```sh
sudo apt install -y autoconf automake bison \
    bzip2 cmake curl flex gawk gperf help2man \
    libncurses5-dev libtool libtool-bin make patch \
    pkg-config rsync texinfo unzip wget xz-utils \
    gcc g++ git
```

Download and Extract from:  
https://github.com/crosstool-ng/crosstool-ng/releases  

```sh
cd crosstool-ng-1.28.0
./bootstrap
./configure --prefix=$HOME/x-tools

make
make install
ls ~/x-tools/bin/ct-ng
```

This version of crosstool-ng did not require any patches to build  

<br />

## Building Toolchain

I just let ctng download Linux 6.16 sources for me  

```sh
cd ~/Documents
mkdir ct_ng_arm_toolchain
cd ct_ng_arm_toolchain

export PATH=$HOME/x-tools/bin:$PATH

ls -la ~/Downloads/crosstool-ng-1.28.0/samples/ | grep arm | grep a9
ct-ng arm-cortexa9_neon-linux-gnueabihf
ct-ng menuconfig
```

ftp.gnu.org was in a good mood when I was doing this so I didn't have to use a mirror  

See [mips32be_malta_linux2.6_gcc4.6.3.md](./mips32be_malta_linux2.6_gcc4.6.3.md) for menuconfig changes and instructions for mirror use  

Menuconfig changes:
```
Target Options > Use specific FPU () (** deleted neon)
Target Options > Floating point: (software (no FPU))
Debug facilities > [] gdb (** unselect it...)
```

Build:  
```sh
ct-ng build
```

glibc build will fail during glibc configure  
```sh
code ./.build/src/glibc-2.42/configure
# Added libc_cv_compiler_ok=yes after esac where that check happens
# Added ac_verc_fail=no after esac on all three of the $LD version checks
# (There were three of them)
```

Build again:  
```sh
ct-ng build
```

```sh
~/x-tools/arm-cortexa9_neon-linux-gnueabi/bin/arm-cortexa9_neon-linux-gnueabi-gcc --version
# arm-cortexa9_neon-linux-gnueabi-gcc (crosstool-NG 1.28.0) 15.2.0
```

<br />

## Building Kernel

```sh
cd path/to/linux-6.16

export ARCH=arm
export CROSS_COMPILE=/home/user/x-tools/arm-cortexa9_neon-linux-gnueabi/bin/arm-cortexa9_neon-linux-gnueabi-

ls arch/arm/configs/ | grep vexpress
make vexpress_defconfig

make menuconfig
```

Menuconfig changes:
```
Device Drivers > Generic Driver Options > [*] Automount devtmpfs at /dev, after the kernel mounted the rootfs  
```

I am pretty sure I did not actually have to make any FPU/neon related changes in the kernel config  

Build:
```sh
make
```

```sh
file vmlinux
# vmlinux: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), ...

ls -la ./arch/arm/boot/zImage
```

<br />

## Build Rootfs with Buildroot

https://buildroot.org/downloads/

I downloaded `buildroot-2025.08.tar.gz`  

```sh
tar -xzvf buildroot-2025.08.tar.gz
cd buildroot-2025.08

ls configs | grep vexpress
make qemu_arm_vexpress_defconfig

make menuconfig
```

Menuconfig changes:
```
Target Options > Target ABI (EABI) (**Not HF)
Toolchain > Toolchain type (External toolchain)
Toolchain > Toolchain (Custom toolchain)

Set Toolchain path to /home/user/x-tools/arm-cortexa9_neon-linux-gnueabi/  

Toolchain > External toolchain kernel headers series (6.16.x or later)
Toolchain > [] Toolchain has RPC support? (**unselect it...)
Toolchain > [*] Toolchain has C++ support?

Kernel > [] Linux kernel (**unselect it...)
```

Build:
```sh
make
```

```sh
ls output/images/rootfs.ext2
```

<br />

## Qemu Run Command

```sh
/home/user/Documents/qemu-10.1.1/build/qemu-system-arm \
	-M vexpress-a9 -nographic \
	-dtb /home/user/Documents/linux-6.16/arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb \
	-kernel /home/user/Documents/linux-6.16/arch/arm/boot/zImage \
	-append "root=/dev/mmcblk0 console=ttyAMA0" \
	-sd /home/user/Documents/buildroot-2025.08/output/images/rootfs.ext2 \
	-serial mon:stdio
```