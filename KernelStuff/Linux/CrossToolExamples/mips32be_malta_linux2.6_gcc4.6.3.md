# Mips 32 BE Malta Build (ct-ng and Buildroot)

The goal is to build a cross-compilation toolchain for MIPS32 with crosstool-ng. Then, to build a Linux 2.6 kernel for a a MIPS32 Malta board with that toolchain. Finally, to build a simple rootfs for that kernel with buildroot and emulate it with qemu  

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
mkdir mips_malta_ctng_build
cd mips_malta_ctng_build

export PATH=$HOME/x-tools/bin:$PATH

ls -la ~/x-tools/lib/ct-ng.1.21.0/samples/ | grep malta
ct-ng mips-malta-linux-gnu
ct-ng menuconfig
```

Menuconfig changes:
```
Paths and misc options > [*] Use a mirror
Paths and misc options > [*] Only use a mirror
(https://mirrors.ocf.berkeley.edu/gnu) Base URL
Operating System > Linux kernel version (custom tarball or directory)
Operating System > Path to custom source... **Put path to kernel src
Binary utilities > binutils version (2.24)
C-library > C library (glibc)
C-library > glibc version (2.10.1)
C compiler > gcc version (4.3.6)
Debug facilities > [] gdb (** unselect it...)
```

If you don't use a mirror it will attempt to download everything from ftp.gnu.org which is prohibitively slow  

Unfortunately, using a mirror means we will be required to do a bit of extra work getting the tarballs to download correctly...

Build:  
```sh
ct-ng build
```

After downloading everything, it will fail during extraction of `gcc-4.3.6` because it downloaded it as an html file instead of the actual tarball. So just download it yourself from  
https://mirrors.ocf.berkeley.edu/gnu/gcc/gcc-4.3.6/  

Then:
```sh
cd .build/tarballs
rm gcc-4.3.6
mv ~/Downloads/gcc-4.3.6.tar.bz2 .
cd ../src
rm -rf gcc-4.3.6
rm .gcc-4.3.6.extracting
```

Then run the build command again

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
~/x-tools/mips-malta-linux-gnu/bin/mips-malta-linux-gnu-gcc --version
# mips-malta-linux-gnu-gcc (crosstool-NG 1.21.0) 4.3.6
```

<br />

## Building Kernel

```sh
cd path/to/linux-2.6.32.16

export ARCH=mips
export CROSS_COMPILE=/home/user/x-tools/mips-malta-linux-gnu/bin/mips-malta-linux-gnu-

ls arch/mips/configs/ | grep malta
make malta_defconfig

make menuconfig
```

Menuconfig changes:
```
Endianess selection (Big endian)
Device Drivers > Generic Driver Options > [*] Create a kernel maintained /dev tmpfs (EXPERIMENTAL)
Device Drivers > Generic Driver Options > [*] Automount devtmpfs at /dev  
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
# vmlinux: ELF 32-bit MSB executable, MIPS, MIPS32 rel2...
```

<br />

## Build Rootfs with Buildroot

https://buildroot.org/downloads/

I downloaded `buildroot-2022.11.tar.gz`  

```sh
tar -xzvf buildroot-2022.11.tar.gz
cd buildroot-2022.11

ls configs/ | grep malta
make qemu_mips32r2_malta_defconfig

make menuconfig
```

Menuconfig changes:
```
Toolchain > Toolchain type (External toolchain)
Toolchain > Toolchain (Custom toolchain)
Set Toolchain path to /home/user/x-tools/mips-malta-linux-gnu/
Toolchain > (mips-malta-linux-gnu) Toolchain prefix
Toolchain > External toolchain gcc version (4.3.x)
Toolchain > External toolchain C library (glibc)
Toolchain > [] Toolchain has SSP support (**unselect it...)

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
./qemu-system-mips \
    -M malta -nographic \
    -kernel /home/user/Documents/linux-2.6.32.16/vmlinux \
    -drive file=/home/user/Documents/buildroot-2022.11/output/images/rootfs.ext2,format=raw \
    -append "root=/dev/hda console=ttyS0" \
    -net nic,model=pcnet
```