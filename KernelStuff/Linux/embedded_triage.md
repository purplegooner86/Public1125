# Embedded Device Triage

## Where is rootfs Mounted From?

```sh
cat /proc/cmdline
```

```sh
cat /proc/mounts
```

### /dev/root

Embedded systems will often mount the root filesystem using an internal alias `/dev/root` like so:
```sh
# (From /proc/mounts)
/dev/root / ext3 rw,relatime 0 0
```

```sh
dmesg | grep -i root
# VFS: Mounted root (ext3 filesystem) readonly on device 179:0
ls -l /dev | grep 179
# brw------- 1 root root 179, 0 Nov 16 13:55 mmcblk0  
```

<br />

## MTD vs MMC

**MTD - "raw flash"**
- No controller
- CPU accesses flash memory directly via memory-mapped interface
- Must handle bad blocks, wear levelin , and ECC in software (kernel/Mtd subsystem)
- Flash must be erased before writing
- Contains erase blocks

**MMC - "managed flash"**
- SD cards are the most common example
- Hardware contains:
    - A flash translation layer (FTL)
    - Wear leveling
    - Bad block mapping
    - Garbage collection
    - A controller that presents the flash as a block device

Notice:
```sh
ls -la /dev/mtd0
# crw-------

ls -la /dev/mmcblk0
# brw-------
```

### What is `mtdblock0`?  
Its a kernel-created "block device wrapper" around the raw character MTD device `/dev/mtd0`

```sh
ls -la /dev/mtdblock0
# brw-------

ls /sys/class/mtd/mtd0/ | grep mtd
# mtd0
# mtdblock0

dd if=/dev/mtd0 of=dump_mtd0.img bs=128K count=1
dd if=/dev/mtdblock0 of=dump_mtdblock0.img bs=128K count=1

md5sum dump_mtdblock0.img
# 8fb4c148760ce7a9611ef676ad9066aa
md5sum dump_mtd0.img
# 8fb4c148760ce7a9611ef676ad9066aa
```

<br />

## Extraction

If you are going to write to tmpfs, always check the size of tmpfs first!
```sh
tmpfs /tmp tmpfs rw,relatime 0 0

df -h /tmp
# Filesystem                Size      Used Available Use% Mounted on
# tmpfs                    56.7M     28.0K     56.7M   0% /tmp
```

**Note**: Most of the time the max size of tmpfs is set to be 50% of the size of RAM. Good to verify though:
```sh
cat /proc/meminfo
# MemAvailable:     101536 kB

# MemFree does not matter! It will usually be smaller than the actually amount of free memory because of the way Linux does disk caching
```


If its a block device (for example, MMC):
```sh
cat /sys/block/mmcblk0/size
# 131072
# ^ That is num blocks. So multiply by 512
# 131072 * 512 = 67108864
dd if=/dev/mmcblk0 of=/tmp/dump.img count=131072
```

**Note**: Random weird thing to be aware of:
```sh
cat /sys/block/mtdblock0/size
# 262144
cat /sys/block/mtdblock0/device/size
# 134217728

# 262144 * 512 = 134217728
```

If its raw flash(for example mtd0):
```sh
cat /proc/mtd
# mtd0: 08000000 00040000 "40000000.flash"

# 0x8000000 = 134217728
# (That is num bytes, not num blocks)
```
