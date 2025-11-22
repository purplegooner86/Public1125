# Setup For Netmap Experiments

## Kernel Building and SDK

Build Kernel:  

I built a linux 4.4.180 kernel based on the "steps" [here](../YoctoStuff/Dockerized_morty/build_commands.sh)  

Just had to make it x86-64 instead of ppc and add the following to `conf/local.conf`:
```sh
IMAGE_INSTALL_append = " kernel-dev kernel-devsrc libpcap"
LLVM_TARGETS_TO_BUILD = "x86"
CORE_IMAGE_EXTRA_INSTALL += " kernel-modules"
TOOLCHAIN_TARGET_TASK_append = " libpcap-dev"
```

We need the virtio_net driver to be built as a module, not baked-in to the kernel  

Getting morty poky (which does not have `menuconfig`) to do this for me was a massive pain  

For some reason using a `fragment.cfg` and adding it to source files was not working  

Instead I did the following:  
Built the kernel with no changes:
```sh
bitbake virtual/kernel
```
Copied off the .config:
```sh
cp ./tmp/work/qemux86_64-poky-linux/linux-yocto-4.4.180/4.4.180+gitAUTOINC+e051a28f0c_a22a2726d0-r0/linux-qemux86_64-standard-build/.config ~/Documents/dockerized_morty/my_config
```
Manually changed `CONFIG_VIRTIO_NET` to `m`  

Added the following to `linux-yocto-4.4.180.bb`:
```sh
do_configure_append() {
    cp /workspace/my_config .config
}
```

Rebuild:

```sh
bitbake virtual/kernel
bitbake core-image-full-cmdline
bitbake core-image-full-cmdline -c populate_sdk
```

Confirm virtio module change:
```sh
cat ./tmp/work/qemux86_64-poky-linux/linux-yocto-4.4.180/4.4.180+gitAUTOINC+e051a28f0c_a22a2726d0-r0/linux-qemux86_64-standard-build/.config | grep -i virtio_net
CONFIG_VIRTIO_NET=m
```

Install the sdk:
```sh
./tmp/deploy/sdk/poky-glibc-x86_64-core-image-full-cmdline-core2-64-toolchain-2.2.4.sh
```

Configure the sdk:
```sh
sudo apt install libelf-dev

sudo su -
source /opt/poky/2.2.4/environment-setup-core2-64-poky-linux
cd /opt/poky/2.2.4/sysroots/core2-64-poky-linux/usr/src/kernel

make modules_prepare
```

Grab the kernel and rootfs:
```sh
cp ./tmp/deploy/images/qemux86-64/core-image-full-cmdline-qemux86-64.ext4 ~/Documents/rootfs.ext4
cp ./tmp/deploy/images/qemux86-64/bzImage--4.4.180+git0+e051a28f0c_a22a2726d0-r0-qemux86-64-20250713150853.bin ~/Documents/kernel_4.4.180.bzImage
```

Turn the rootfs into a qcow2 image:
```sh
sudo mkdir /mnt/extmounted
sudo mkdir /mnt/qcow_mounted

sudo mount -o loop ./rootfs.ext4 /mnt/extmounted

sudo modprobe nbd

qemu-img create -f qcow2 rootfs.qcow2 5G
sudo qemu-nbd -c /dev/nbd0 ./rootfs.qcow2
sudo mkfs.ext4 /dev/nbd0
sudo mount /dev/nbd0 /mnt/qcow_mounted

sudo cp -a /mnt/extmounted/* /mnt/qcow_mounted

sudo umount /mnt/qcow_mounted
sudo umount /mnt/extmounted

sudo qemu-nbd -d /dev/nbd0
```

<br />

## Qemu Building

Apt's version of qemu is old. So build it yourself:
```sh
git clone https://github.com/qemu/qemu.git
cd qemu
mkdir build
cd build

sudo apt-get install git libglib2.0-dev libfdt-dev libpixman-1-dev zlib1g-dev ninja-build
sudo apt-get install libsdl2-dev

python3 -m pip install tomli
# Might be a few other python deps you have to add...

echo 'export PATH=/home/user/.local/bin:$PATH' >> ~/.zshrc
source ~/.zshrc

../configure --enable-sdl --target-list=x86_64-softmmu
make -j 4
```

Get qemu tap networking script:
```sh
cd ~/Documents
cp /etc/qemu-ifup .
cp /etc/qemu-ifdown .

echo 'ip addr add 192.168.7.1/24 dev tap0' >> ./qemu-ifup
```

Qemu run command:
```sh
/home/user/Documents/qemu/build/qemu-system-x86_64 \
-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:02 \
-netdev tap,id=net0,ifname=tap0,script=/home/user/Documents/qemu-ifup,downscript=/home/user/Documents/qemu-ifdown \
-object rng-random,filename=/dev/urandom,id=rng0 \
-device virtio-rng-pci,rng=rng0 \
-drive file=/home/user/Documents/rootfs.qcow2,if=virtio,format=qcow2 \
-usb -device usb-tablet -usb -device usb-kbd \
-cpu IvyBridge -machine q35,i8042=off -smp 4 -m 256 \
-serial mon:vc -serial null -device virtio-vga \
-display sdl,show-cursor=on \
-kernel /home/user/Documents/kernel_4.4.180.bzImage \
-append 'root=/dev/vda rw  ip=192.168.7.2::192.168.7.1:255.255.255.0::eth0:off:8.8.8.8 net.ifnames=0 oprofile.timer=1 tsc=reliable no_timer_check rcupdate.rcu_expedited=1 swiotlb=0'
```

Because `virtio_net.ko` is being loaded as a module, and not baked in, you need to manually start eth0 after booting:
```sh
ip link set eth0 up
ip addr add 192.168.7.2/24 dev eth0
```

Confirm qemu target is using virtio_net driver:
```sh
ethtool -i eth0
```
