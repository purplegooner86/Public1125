KBRANCH ?= "master"

require recipes-kernel/linux/linux-yocto.inc

SRCREV_machine ?= "MACROMACROMACRO_MACHINE_HASH"
SRCREV_meta ?= "MACROMACROMACRO_META_HASH"

SRC_URI = "\
git:///workspace/linux-yocto-4.4.MACROMACROMACRO_MINOR_VERSION;name=machine;protocol=file;branch=${KBRANCH} \
git:///workspace/yocto-kernel-cache-4.4;name=meta;type=kmeta;destsuffix=${KMETA};protocol=file"

LINUX_VERSION ?= "4.4.MACROMACROMACRO_MINOR_VERSION"

PV = "${LINUX_VERSION}+git${SRCPV}"

KMETA = "kernel-meta"
KCONF_BSP_AUDIT_LEVEL = "2"

COMPATIBLE_MACHINE = "qemuarm|qemuarm64|qemux86|qemuppc|qemumips|qemumips64|qemux86-64"

# Functionality flags
KERNEL_EXTRA_FEATURES ?= "features/netfilter/netfilter.scc"
KERNEL_FEATURES_append = " ${KERNEL_EXTRA_FEATURES}"
KERNEL_FEATURES_append_qemuall=" cfg/virtio.scc"
KERNEL_FEATURES_append_qemux86=" cfg/sound.scc cfg/paravirt_kvm.scc"
KERNEL_FEATURES_append_qemux86-64=" cfg/sound.scc cfg/paravirt_kvm.scc"
KERNEL_FEATURES_append = " ${@bb.utils.contains("TUNE_FEATURES", "mx32", " cfg/x32.scc", "" ,d)}"
