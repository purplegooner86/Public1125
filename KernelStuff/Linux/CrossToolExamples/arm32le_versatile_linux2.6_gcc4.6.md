# Arm 32 LE versatilepb Linux 2.6.32 Build

The goal is to build a cross-compilation toolchain for Arm32 LE cortex-a9 with crosstool-ng. Then, to build a Linux 2.6.32 kernel for an emulated vexpress board. Finally, to build a simple rootfs for that kernel with buildroot  

This was all done in an Ubuntu 22.04 VM  

<br />

## Building crosstool-ng 1.21.0

Using this old version because it supports required deps for 2.6 kernel  

Install dependencies:
```sh
sudo apt install -y autoconf automake bison \
    bzip2 cmake curl flex gawk gperf help2man \
    libncurses5-dev libtool libtool-bin make patch \
    pkg-config rsync texinfo unzip wget xz-utils \
    gcc g++ git
```

Download:  

https://github.com/crosstool-ng/crosstool-ng/releases

Downloaded `crosstool-ng-1.21.0.tar.bz2`  

```sh
bzip2 -d crosstool-ng-1.21.0.tar.bz2
tar -xvf crosstool-ng-1.21.0.tar
cd crosstool-ng-1.21.0

./bootstrap
./configure --prefix=$HOME/x-tools --with-bash=/usr/bin/bash
```

There is one small patch required to get this version of crosstool-ng to build:
```patch
--- a/kconfig/zconf.hash.c
+++ b/kconfig/zconf.hash.c
@@ -167,7 +167,7 @@ static struct kconf_id_strings_t kconf_id_strings_contents =
   };
 #define kconf_id_strings ((const char *) &kconf_id_strings_contents)
 struct kconf_id *
-kconf_id_lookup (register const char *str, register size_t len)
+kconf_id_lookup (register const char *str, register unsigned int len)
```

Build:
```sh
make
make install
ls ~/x-tools/bin/ct-ng
```

<br />

## Building Toolchain

I downloaded linux 2.6.32.16 sources from:  
https://www.kernel.org/pub/linux/kernel/v2.6/

Extract them:
```sh
bzip2 -d linux-2.6.32.16.tar.bz2
tar -xvf linux-2.6.32.16.tar
```

Building toolchain:
```sh
cd ~/Documents
mkdir arm_linux_2.6_ctng_build
cd arm_linux_2.6_ctng_build

export PATH=$HOME/x-tools/bin:$PATH

ls -la ~/x-tools-1.21/lib/ct-ng.1.21.0/samples | grep arm

ct-ng arm-unknown-linux-gnueabi
ct-ng menuconfig
```

ftp.gnu.org was in a good mood when I was doing this so I didn't have to use a mirror  

See [mips32be_malta_linux2.6_gcc4.6.3.md](./mips32be_malta_linux2.6_gcc4.6.3.md) for menuconfig changes and instructions for mirror use  

Menuconfig changes:
```
Operating System > Linux kernel version (custom tarball or directory)
Operating System > Path to custom source... **Put path to kernel src
Binary utilities > binutils version (2.24)
Binary utilities > Linkers to enable (ld)
C-library > C library (glibc)
C-library > glibc version (2.10.1)
C compiler > gcc version (4.3.6)
Debug facilities > (** Unselect all of them)
```

Build:  
```sh
ct-ng build
```

Both gcc-4.3.6 and glibc-2.10.1 need a small amount of patching to make them compilable  

Patches should be made to the src directories in `.build/src` before re-running the build command  

Patch to GCC:
```patch
--- a/gcc/toplev.h
+++ b/gcc/toplev.h
@@ -173,6 +173,7 @@ extern int floor_log2                  (unsigned HOST_WIDE_INT);
 #  define CTZ_HWI __builtin_ctz
 # endif
 
+/*
 extern inline int
 floor_log2 (unsigned HOST_WIDE_INT x)
 {
@@ -184,6 +185,7 @@ exact_log2 (unsigned HOST_WIDE_INT x)
 {
   return x == (x & -x) && x ? (int) CTZ_HWI (x) : -1;
 }
+*/
 #endif /* GCC_VERSION >= 3004 */
```

Patch to glibc:
```patch
--- a/configure
+++ b/configure
@@ -5086,16 +5086 @@ 
-     3.79* | 3.[89]*)
+     3.79* | 4.[3]*)
```

<br />

After building for the final time:
```sh
~/x-tools/arm-unknown-linux-gnueabi/bin/arm-unknown-linux-gnueabi-gcc --version
# arm-unknown-linux-gnueabi-gcc (crosstool-NG 1.21.0) 4.3.6
```

<br />

## Building Kernel

```sh
cd path/to/linux-2.6.32.16

export ARCH=arm
export CROSS_COMPILE=/home/user/x-tools/arm-unknown-linux-gnueabi/bin/arm-unknown-linux-gnueabi-

ls arch/arm/configs | grep versatile
make versatile_defconfig

make menuconfig
```

Menuconfig changes (These will have to be made in this order):
```
File systems > Pseudo filesystems > [*] Virtual memory file system support
Device Drivers > Generic Driver Options > [*] Create a kernel maintained /dev tmpfs (EXPERIMENTAL)
Device Drivers > Generic Driver Options > [*] Automount devtmpfs at /dev  

Bus support > [*] PCI support
Device Drivers > <*> ATA/ATAPI/MFM/RLL support
Device Drivers > SCSI device support > -*- SCSI device support
Device Drivers > SCSI device support > <*> SCSI disk support
Device Drivers > SCSI device support > <*> SCSI generic support
Device Drivers > <*> Serial ATA (prod) and Parallel ATA (experimental drivers) > <*> AHCI SATA support

Kernel Features > [*] Use the ARM EABI to compile the kernel
```


Patch the dumb perl script:
```patch
--- a/kernel/timeconst.pl
+++ b/kernel/timeconst.pl
@@ -370,7 +370,7 @@ if ($hz eq '--can') {
 	}
 
 	@val = @{$canned_values{$hz}};
-	if (!defined(@val)) {
+	if (!@val) {
 		@val = compute_values($hz);
```

Build:

```sh
make
```

```sh
file vmlinux
# vmlinux: ELF 32-bit LSB executable, ARM, EABI4 version 1 (SYSV) ...

ls -la ./arch/arm/boot/zImage
```

<br />

## Build Rootfs with Buildroot

https://buildroot.org/downloads/

I downloaded `buildroot-2022.11.tar.gz`  

```sh
tar -xzvf buildroot-2022.11.tar.gz
cd buildroot-2022.11

ls configs | grep arm
make qemu_arm_versatile_defconfig

make menuconfig
```

Menuconfig changes:
```
Toolchain > Toolchain type (External toolchain)
Toolchain > Toolchain (Custom toolchain)
Set Toolchain path to /home/user/x-tools/arm-unknown-linux-gnueabi/
Toolchain > (arm-unknown-linux-gnueabi) Toolchain prefix
Toolchain > External toolchain gcc version (4.3.x)
Toolchain > External toolchain C library (glibc)
Toolchain > [] Toolchain has SSP support (**unselect it...)
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
qemu-system-arm \
	-M versatilepb -nographic \
	-kernel ./zImage \
	-append "root=/dev/sda rw console=ttyAMA0" \
	-device ahci,id=ahci \
	-device ide-hd,drive=mydrive,bus=ahci.0 \
	-drive file=rootfs.ext2,if=none,id=mydrive,format=raw
```

