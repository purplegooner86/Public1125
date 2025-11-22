# Basic Styhead Linux 6.10.14 Build

Doing this on Ubuntu 22.04

## Kernel, Rootfs, SDK Building

Build Kernel:  

```sh
git clone https://github.com/yoctoproject/poky.git
cd poky
git checkout styhead
source oe-init-build-env
```

Add to `conf/local.conf`:
```sh
EXTRA_IMAGE_FEATURES += "tools-debug tools-profile tools-sdk debug-tweaks"
IMAGE_INSTALL:append = " kernel-dev kernel-devsrc"
LLVM_TARGETS_TO_BUILD = "x86"
CORE_IMAGE_EXTRA_INSTALL += " kernel-modules"

```sh
bitbake virtual/kernel
bitbake core-image-full-cmdline
bitbake core-image-full-cmdline -c populate_sdk
```

Install the sdk:
```sh
./tmp/deploy/sdk/poky-glibc-x86_64-core-image-full-cmdline-core2-64-qemux86-64-toolchain-5.1.4.sh
```

Configure the sdk:
```sh
sudo apt install libelf-dev

sudo su -
source /opt/poky/5.1.4/environment-setup-core2-64-poky-linux
cd /opt/poky/5.1.4/sysroots/core2-64-poky-linux/lib/modules/6.10.14-yocto-standard/source

make modules_prepare
```

Grab the kernel and rootfs:
```sh
cp ./tmp/deploy/images/qemux86-64/core-image-full-cmdline-qemux86-64.rootfs-20250712101148.ext4 ~/Documents/rootfs.ext4
cp ./tmp/deploy/images/qemux86-64/bzImage--6.10.14+git0+af06ad75b8_bbe3d1be4e-r0-qemux86-64-20250711232213.bin ~/Documents/kernel_bzImage_6.10.14
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

# Might have to install a bunch of python deps with pip and add .local/bin to your path...
echo 'export PATH=/home/user/.local/bin:$PATH' >> ~/.zshrc

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
-drive file=/home/user/Documents/rootfs.ext4,if=virtio,format=raw \
-usb -device usb-tablet -usb -device usb-kbd \
-cpu IvyBridge -machine q35,i8042=off -smp 4 -m 256 \
-serial mon:vc -serial null -device virtio-vga \
-display sdl,show-cursor=on \
-kernel /home/user/Documents/kernel_bzImage_6.10.14 \
-append 'root=/dev/vda rw  ip=192.168.7.2::192.168.7.1:255.255.255.0::eth0:off:8.8.8.8 net.ifnames=0 oprofile.timer=1 tsc=reliable no_timer_check rcupdate.rcu_expedited=1 swiotlb=0'
```

