# Setting Up Netmap

This is using setup described [here](./lab_setup.md)

## Netmap Building

```sh
git clone https://github.com/luigirizzo/netmap.git
cd netmap

source /opt/poky/2.2.4/environment-setup-core2-64-poky-linux
export LDFLAGS=""

./configure --kernel-dir=/opt/poky/2.2.4/sysroots/core2-64-poky-linux/usr/src/kernel

SDKTARGETSYSROOT=/opt/poky/2.2.4/sysroots/core2-64-poky-linux/

make -C libnetmap SHARED=1 -j 4 CFLAGS="--sysroot=${SDKTARGETSYSROOT} -fPIC -I/home/user/Documents/netmap/sys -I /home/user/Documents/netmap/libnetmap -I/home/user/Documents/netmap/apps/include ${CFLAGS}" LDFLAGS="--sysroot=${SDKTARGETSYSROOT} ${LDFLAGS}"

cd libnetmap
echo $CC
# Take the output followed by -shared -o libnetmap.so *.o
# So in this case:
x86_64-poky-linux-gcc  -m64 -march=core2 -mtune=core2 -msse3 -mfpmath=sse --sysroot=/opt/poky/2.2.4/sysroots/core2-64-poky-linux -shared -o libnetmap.so *.o

sudo cp ./libnetmap.so /opt/poky/2.2.4/sysroots/core2-64-poky-linux/usr/lib/
# ^ Also put it in the rootfs image in the same location

cd ../
make clean
cd netmap/build-apps/pkt-gen
make WITH_PCAP=1 -j 4 CFLAGS="--sysroot=${SDKTARGETSYSROOT} -I/home/user/Documents/netmap/sys -I /home/user/Documents/netmap/libnetmap -I/home/user/Documents/netmap/apps/include ${CFLAGS}" LDFLAGS="--sysroot=${SDKTARGETSYSROOT} ${LDFLAGS}"

cp pkt-gen-b ~/Documents
cd ../../
make clean
make -j 4 
# Warnings about drivers for other nics are fine, but there should be no warnings about virtio_net driver

ls -la ./netmap.ko
ls -la ./virtio_net.c/virtio_net.ko
```

<br />

## Replace Virtio Module

```sh
sudo modprobe nbd

sudo qemu-nbd -c /dev/nbd0 ./rootfs.qcow2
sudo mount /dev/nbd0 /mnt/qcow_mounted

sudo cp ./virtio_net.c/virtio_net.ko /mnt/qcow_mounted/lib/modules/4.4.180-yocto-standard/kernel/drivers/net/virtio_net.ko

sudo cp ./netmap.ko /mnt/qcow_mounted/lib/modules/4.4.180-yocto-standard/kernel/drivers/net/

sudo umount /mnt/qcow_mounted
sudo qemu-nbd -d /dev/nbd0
```

<br />

## Test Virtio Module

Boot the emulation  
In emulation run:
```sh
depmod -a
modprobe netmap
modprobe virtio_net
```

Bring up eth0, it should still function as normal:
```sh
ip link set eth0 up
ip addr add 192.168.7.2/24 dev eth0
```

Add a second interface:
- Copy `qemu-ifup` to `qemu-ifup-eth1` and change ip and dev to `tap1`
- Add following two lines to qemu run command:

```sh
-device virtio-net-pci,netdev=net1,mac=52:54:00:12:34:03 \
-netdev tap,id=net1,ifname=tap1,script=/home/user/Documents/qemu-ifup-eth1,downscript=/home/user/Documents/qemu-ifdown \
```

eth1 should also function as normal after:
```sh
ip link set eth1 up
ip addr add 192.168.8.2/24 dev eth1
```

<br />

## Test Netmap Mode Operation

Reboot the emulation, bring eth0 back up, leave eth1

Grab the `pkt-gen-b` tool we compiled earlierand put it on the target  
Verify:
```sh
ldd pkt-gen-b 
	linux-vdso.so.1 (0x00007ffd119bc000)
	libpthread.so.0 => /lib/libpthread.so.0 (0x0000003ecaa00000)
	libm.so.6 => /lib/libm.so.6 (0x0000003ecb600000)
	libnetmap.so => /usr/lib/libnetmap.so (0x00007f2fcfb4e000)
	librt.so.1 => /lib/librt.so.1 (0x0000003ecbe00000)
	libpcap.so.1 => /usr/lib/libpcap.so.1 (0x00007f2fcf90b000)
	libc.so.6 => /lib/libc.so.6 (0x0000003eca600000)
	/lib/ld-linux-x86-64.so.2 (0x0000003eca200000)
```

Test:
```sh
# Bring up the link:
ip link set eth1 up
ip addr add 192.168.8.2/24 dev eth1
# Wait at least 5 seconds
# Run receiver on emulatee:
./pkt-gen-b -i eth1 -f rx
# Ping from host, should see 1.00 reflected:
ping 192.168.8.2
```

