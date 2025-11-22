# Arm 32 Vexpress Flash Examples

The starting point for all of these is the toolchain/kernel/rootfs build I did in [arm32le_vexpress_linux6.16_gcc15.2.0.md](../CrossToolExamples/arm32le_vexpress_linux6.16_gcc15.2.0.md)  
Follow that first

<br />

## Example 1: RootFS from MTD

This doesn't require any changes to the toolchain or kernel builds  

We need a build root to make us a flash-compatible file system though. I chose JFFS2 for this example  

```sh
make menuconfig
```
```
Filesystem images > [*] jffs2 root filesystem
Filesystem images > Flash Type (Parallel flash with 64 kB erase size)  
```

```sh
make
cp ./output/images/rootfs.jffs2 .
truncate -s 64M rootfs.jffs2
```

Qemu run command:
```sh
qemu-system-arm \
	-M vexpress-a9 -nographic \
	-dtb ./vexpress-v2p-ca9.dtb \
	-kernel ./zImage \
	-append "root=/dev/mtdblock0 rootfstype=jffs2 console=ttyAMA0" \
	-drive if=pflash,format=raw,file=rootfs.jffs2 \
	-serial mon:stdio
```

<br />

## Example 2: Boot Kernel from Flash with u-boot

I think its easiest to just have buildroot build u-boot for us  

To get a list of the board defconfigs, look at the u-boot source and:  
```sh
ls -a ./configs/ | grep -i vexpress
# ...
# vexpress_ca9x4_defconfig
# ...
```

```
make menuconfig
```

```
Bootloaders > [*] U-Boot
Bootloaders > U-Boot > U-Boot configuration (Using an in-tree board defconfig file)
Bootloaders > U-Boot > (vexpress_ca9x4) Board defconfig
Bootloaders > U-Boot > **Everything else is not selected**
```

```sh
make

file output/build/uboot-2025.07/u-boot
# ELF 32-bit LSB executable, ARM...
```

Here is a script to combine u-boot, the kernel, and the dtb onto a single 64MB flash image:
```sh
#!/bin/bash
set -e

# ---------- CONFIGURATION ----------

FLASH_SIZE_MB=64          # Total flash size
FLASH_IMG="flash.img"     # Output flash image

UBOOT_BIN="u-boot.bin"
KERNEL_IMG="zImage"
DTB_FILE="vexpress-v2p-ca9.dtb"

# ---------- OFFSETS (in bytes) ----------
OFFSET_UBOOT=$((0x00000000))
OFFSET_KERNEL=$((0x00040000))
OFFSET_BOOT_SCRIPT=$((0x00f40000))
OFFSET_DTB=$((0x00f50000)) 


echo "Creating blank flash image (${FLASH_SIZE_MB} MB)..."
dd if=/dev/zero of=$FLASH_IMG bs=1M count=$FLASH_SIZE_MB

echo "Writing U-Boot..."
dd if=$UBOOT_BIN of=$FLASH_IMG conv=notrunc bs=1 seek=$OFFSET_UBOOT

echo "Writing kernel..."
dd if=$KERNEL_IMG of=$FLASH_IMG conv=notrunc bs=1 seek=$OFFSET_KERNEL

echo "Writing DTB..."
dd if=$DTB_FILE of=$FLASH_IMG conv=notrunc bs=1 seek=$OFFSET_DTB

echo "Flash image '$FLASH_IMG' created successfully!"
```

I only had success loading u-boot as an ELF with `-kernel` as opposed to booting to the .bin. So, we are essentially faking the fact that u-boot was actually read from flash when really Qemu loaded the ELF into memory. We will actually read the kernel from flash though  

I examined the load address the vexpress board will put a zImage at with `-S -s` and gdb. It was `0x60010000`  

The only gotcha there was making sure you are actually looking at the instructions at the start of the zImage, not whatever pseudo-bootloader the vexpress Qemu board is executing before it jumps to the first instruction at the start of the zImage  

Qemu run command:
```sh
qemu-system-arm \
	-M vexpress-a9 -nographic \
	-kernel u-boot \
	-drive if=pflash,format=raw,file=flash.img \
	-sd ./rootfs.ext2 \
	-serial mon:stdio
```

Then, interrupt the U-Boot autoboot by pressing any key

U-Boot commands to do boot:
```sh
# zImage is 0x5d81a0 bytes (ls -la)
# vexpress-v2p-ca9.dtb is 0x37f9 bytes (ls -la)

setenv bootargs "console=ttyAMA0 root=/dev/mmcblk0"

cp.b 0x00040000 0x60010000 0x5d81a0
cp.b 0x00f50000 0x64000000 0x37f9
bootz 0x60010000 - 0x64000000
```

Second parameter to `bootz` is initrd addr which we are not using. Hence `-`  






