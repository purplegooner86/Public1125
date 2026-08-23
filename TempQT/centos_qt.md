# CentOS 7 in Qemu

```bash
uname -a
# Linux centos7-qemu 3.10.0-327.10.1.el7.x86_64 #1 SMP Tue Feb 16 17:03:50 UTC 2016 x86_64 x86_64 x86_64 GNU/Linux

cat /etc/redhat-release
# CentOS Linux release 7.2.1511 (Core)
```

## Inputs

| Item | Path |
|---|---|
| CentOS 7 installer ISO (minimal) | `CentOS-7-x86_64-Minimal-1602-99.iso` |
| QEMU source tree (pre-configured) | `qemu-10.1.0/` |
| QEMU binary (after build) | `qemu-10.1.0/build/qemu-system-x86_64` |

All VM artifacts created by this process live under `vm/`.

## 1. Verify KVM acceleration

```bash
[ -r /dev/kvm ] && [ -w /dev/kvm ] && echo "KVM OK"
```

The current user has access via an ACL on `/dev/kvm`, so `-enable-kvm` is used for near-native install/boot speed.

## 2. Create the guest disk

```bash
qemu-10.1.0/build/qemu-img create -f qcow2 vm/centos7.qcow2 20G
```

qcow2 is thin-provisioned: the file only grows as the guest writes data.

## 3. Make the install unattended (kickstart)

The CentOS 7 installer (Anaconda) is normally interactive. To run it hands-free
and headless:

1. **Kickstart file** — `vm/ks/ks.cfg` answers every installer question:
   text-mode install from CD-ROM, auto-partitioning of the virtio disk, `@core`
   package group only, `skipx` (no X configured by the installer — the Qt/X11
   stack comes later on our own terms), SELinux permissive, firewall off, root
   password `centos`, and — critically — a GRUB kernel argument of
   `console=ttyS0,115200n8 console=tty0` so the installed system is always
   reachable over the serial console. Here is `ks.cfg`:
    ```

    # Kickstart for unattended CentOS 7 minimal install (QEMU guest)
    install
    cdrom
    text
    skipx
    lang en_US.UTF-8
    keyboard us
    timezone UTC --isUtc
    network --hostname=centos7-qemu
    rootpw --plaintext centos
    authconfig --enableshadow --passalgo=sha512
    selinux --permissive
    firewall --disabled
    firstboot --disabled
    zerombr
    clearpart --all --initlabel
    bootloader --location=mbr --append="console=ttyS0,115200n8 console=tty0"
    autopart --type=plain
    reboot

    %packages --ignoremissing
    @core
    %end
    ```

2. **Deliver the kickstart via an `OEMDRV` volume** — Anaconda automatically
   looks for a volume labeled `OEMDRV`; we also point at it explicitly with
   `inst.ks=`. Build a tiny ISO containing `ks.cfg`:

   ```bash
   mkisofs -V OEMDRV -o vm/oemdrv.iso vm/ks/
   ```

3. **Boot the installer's kernel directly** so we can pass installer arguments
   without touching the ISOLINUX menu. Extract kernel + initrd from the ISO:

   ```bash
   isoinfo -R -i CentOS-7-x86_64-Minimal-1602-99.iso -x /isolinux/vmlinuz   > vm/boot/vmlinuz
   isoinfo -R -i CentOS-7-x86_64-Minimal-1602-99.iso -x /isolinux/initrd.img > vm/boot/initrd.img
   ```

   The stage-2 image is found on the attached ISO by its volume label
   (`CentOS 7 x86_64`, spaces escaped as `\x20` on the kernel command line).


## 4. Run the installation

```bash
qemu-10.1.0/build/qemu-system-x86_64 \
  -enable-kvm -cpu host -m 2048 -smp 2 \
  -drive file=vm/centos7.qcow2,if=virtio,format=qcow2 \
  -drive file=CentOS-7-x86_64-Minimal-1602-99.iso,media=cdrom,readonly=on \
  -drive file=vm/oemdrv.iso,media=cdrom,readonly=on \
  -kernel vm/boot/vmlinuz -initrd vm/boot/initrd.img \
  -append 'inst.stage2=hd:LABEL=CentOS\x207\x20x86_64 inst.ks=hd:LABEL=OEMDRV:/ks.cfg inst.text console=ttyS0,115200n8' \
  -display none -serial file:vm/install-serial.log \
  -no-reboot
```

The install takes several minutes; progress can be watched with
`tail -f vm/install-serial.log`.

## 5. Boot the installed system

Here is a nice bash script that lets you toggle tap networking on/off:  
```sh
#!/usr/bin/env bash
# Boot the installed CentOS 7 guest.
#
# Usage:
#   ./vm/run-centos7.sh              # no networking (default)
#   ./vm/run-centos7.sh --tap        # tap networking (host tap device must exist)
#
# Serial console is on this terminal: log in as root / centos.
# Ctrl-A X exits QEMU; Ctrl-A C toggles the QEMU monitor.
#
# Tap mode expects a preconfigured host tap device (default: tap0, override
# with TAP_IFNAME). Example host-side setup (run as root):
#   ip tuntap add dev tap0 mode tap user "$USER"
#   ip link set tap0 up
#   # then either add tap0 to a bridge, or give it an IP + NAT, e.g.:
#   ip addr add 192.168.100.1/24 dev tap0
#   sysctl -w net.ipv4.ip_forward=1
#   iptables -t nat -A POSTROUTING -s 192.168.100.0/24 -j MASQUERADE
# Guest side: static IP in 192.168.100.0/24, gateway/DNS 192.168.100.1.
#
# Note: user-mode networking (-netdev user) is NOT compiled into this QEMU
# build (no libslirp), which is why the choices are tap or nothing.

set -euo pipefail
cd "$(dirname "$0")/.."

QEMU=qemu-10.1.0/build/qemu-system-x86_64
DISK=vm/centos7.qcow2
TAP_IFNAME="${TAP_IFNAME:-tap0}"
GUEST_MAC="${GUEST_MAC:-52:54:00:c7:05:01}"

NET_ARGS=()
case "${1:-}" in
  --tap)
    NET_ARGS=(
      -netdev "tap,id=n0,ifname=${TAP_IFNAME},script=no,downscript=no"
      -device "virtio-net-pci,netdev=n0,mac=${GUEST_MAC}"
    )
    shift
    ;;
  "") ;;
  *)
    echo "usage: $0 [--tap]" >&2
    exit 1
    ;;
esac

exec "$QEMU" \
  -enable-kvm -cpu host -m 2048 -smp 2 \
  -drive "file=${DISK},if=virtio,format=qcow2" \
  "${NET_ARGS[@]}" \
  -display none \
  -serial mon:stdio \
  "$@"
```

# Building Qt 5.2.1 from Source in the CentOS 7 Guest

```bash
yum -y install \
    xorg-x11-server-Xorg xorg-x11-xinit xorg-x11-xauth \
    xorg-x11-drv-qxl xorg-x11-drv-vesa xorg-x11-drv-fbdev \
    xorg-x11-drv-evdev xorg-x11-drv-keyboard xorg-x11-drv-mouse \
    xorg-x11-fonts-misc dejavu-sans-fonts xorg-x11-utils

yum -y groupinstall "Development Tools"

yum -y install \
    libX11-devel libXext-devel libXrender-devel libXi-devel \
    libXfixes-devel libXcursor-devel libXrandr-devel libXdamage-devel \
    libSM-devel libICE-devel \
    libxcb-devel xcb-util-devel xcb-util-image-devel xcb-util-keysyms-devel \
    xcb-util-renderutil-devel xcb-util-wm-devel \
    libxkbcommon-devel libxkbcommon-x11-devel \
    fontconfig-devel freetype-devel \
    mesa-libGL-devel mesa-libEGL-devel \
    dbus-devel glib2-devel zlib-devel libpng-devel libjpeg-turbo-devel
```

## 1. Getting usable source (important gotcha)

The `qt5-5.2.1.tar.gz` release tarball from the **qt/qt5 GitHub repo is not
buildable source** — it is only the "supermodule" scaffolding (configure
scripts, `.gitmodules`); the actual module code normally arrives as git
submodules. The real source lives in the official Qt archive.

We only need three modules for this project (a widgets app that renders
numbers to an X11 screen), which is much faster to build than all of Qt:

| Module | Why |
|---|---|
| `qtbase` | QtCore/QtGui/QtWidgets, qmake, the xcb platform plugin |
| `qtx11extras` | X11-specific helpers (QX11Info) |
| `qtsvg` | optional, cheap to build, useful for scalable glyphs |

*(host)* Download from the Qt archive and copy into the guest:

```bash
cd vm
for m in qtbase qtx11extras qtsvg; do
  curl -sSL -O "https://download.qt.io/new_archive/qt/5.2/5.2.1/submodules/${m}-opensource-src-5.2.1.tar.gz"
done
scp qt*-opensource-src-5.2.1.tar.gz root@192.168.100.10:/root/
```

Extract in the guest:

```bash
cd /root
for f in qtbase qtsvg qtx11extras; do tar xzf ${f}-opensource-src-5.2.1.tar.gz; done
```

## 2. Configure qtbase

```bash
cd /root/qtbase-opensource-src-5.2.1
./configure -prefix /opt/qt-5.2.1 \
  -opensource -confirm-license -release \
  -nomake examples -nomake tests \
  -qt-xcb -qt-xkbcommon
```

Flag rationale:

- `-prefix /opt/qt-5.2.1` — self-contained install, easy to wipe/rebuild.
- `-qt-xcb -qt-xkbcommon` — use Qt's **bundled** xcb/xkbcommon sources.
  This is the critical flag pair: Qt 5.2's xcb platform plugin was written
  against the pre-1.10 libxcb XKB API; CentOS 7.9 ships libxcb 1.13, whose
  xkb interface changed, so building against the system copies fails.
- `-release -nomake examples -nomake tests` — smaller/faster build.

Configure summary must show: `XCB ... yes (bundled copy)`,
`FontConfig ... yes`, `OpenGL ... desktop`, `xkbcommon ... yes (bundled copy)`.

## 3. Build and install (qtbase, then the add-on modules)

The guest has 2 vCPUs, so `-j2`:

```bash
cd /root/qtbase-opensource-src-5.2.1
make -j2
make install          # installs to /opt/qt-5.2.1

for m in qtx11extras qtsvg; do
  cd /root/${m}-opensource-src-5.2.1
  /opt/qt-5.2.1/bin/qmake   # add-on modules build against the installed qmake
  make -j2
  make install
done
```

Build logs are kept in the guest as `/root/qtbase-configure.log`,
`/root/qtbase-make.log`, `/root/qtbase-install.log`, and
`/root/<module>-build.log`.

## 4. Environment

```bash
cat > /etc/profile.d/qt521.sh << 'EOF'
export PATH=/opt/qt-5.2.1/bin:$PATH
EOF
```

`LD_LIBRARY_PATH` is not needed: Qt was built with rpath, so its binaries and
anything built with its qmake resolve `/opt/qt-5.2.1/lib` automatically.

Also give the non-fontconfig platform plugins (offscreen, linuxfb) a font
directory — Qt 5.2 expects `$prefix/lib/fonts`:

```bash
ln -sfn /usr/share/fonts /opt/qt-5.2.1/lib/fonts
```

## 5. Verify

```bash
/opt/qt-5.2.1/bin/qmake -v        # expect: Using Qt version 5.2.1 in /opt/qt-5.2.1/lib
ls /opt/qt-5.2.1/plugins/platforms # expect libqxcb.so (plus linuxfb, minimal, offscreen)
```

End-to-end smoke test performed (see `/root/qtsmoke/` in the guest): a
minimal QtWidgets program (`QApplication` + `QLabel("12345")`) was compiled
with the new qmake and executed with `-platform offscreen` — it initialized
Qt 5.2.1 and exited cleanly. Displaying on the real xcb platform is part of
the next milestone, once X is configured kiosk-style on the virtual screen.

Build timings for reference: qtbase ≈ 3 min at `-j2` (KVM on a fast host),
qtx11extras and qtsvg seconds each. Total install ≈ 60 MB in `/opt/qt-5.2.1`.


