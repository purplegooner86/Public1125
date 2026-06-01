# ARM BE32 Gcc 3.2.3 Glibc 2.2.5 Crosstool Build

The goal is to build a cross compilation toolchain for Arm32 BE32 with crosstool. This is just instructions for the toolchain, not a kernel  

<br />

## Containerized Environment

I did this in an Ubuntu 12.04 container  

The command I am using to run the container:
```sh
docker run -it --rm -v "$PWD":/workspace ubuntu:12.04 /bin/bash
```

In 2025, here is an `/etc/apt/sources.list` which works for 1204:  
```sh
deb http://old-releases.ubuntu.com/ubuntu precise main restricted universe multiverse
deb http://old-releases.ubuntu.com/ubuntu precise-updates main restricted universe multiverse
deb http://old-releases.ubuntu.com/ubuntu precise-security main restricted universe multiverse
```

Prepare the container:
```sh
apt-get update

apt-get install -y autoconf automake bison bzip2 cmake curl flex gawk gperf help2man libncurses5-dev libtool make patch pkg-config rsync texinfo unzip wget xz-utils gcc g++ git
```

<br />

## Toolchain Build

All of the build instructions here done in the container  

Support for this old a version of glibc and gcc predates `crosstool-ng`  
Instead, we have to go back to the ancient vanilla `crosstool`  

https://kegel.com/crosstool/  
There is a link for `crosstool-0.43.tar.gz`  

That is not a gzip, just a tarball, the filename is misleading  

First, modify `demo-armeb.sh`:  
```sh
# Change to:
TARBALLS_DIR=/workspace/downloads

# Change the eval line to:
eval `cat armeb.dat gcc-3.2.3-glibc-2.2.5.dat` sh all.sh --notest
```

Modify `all.sh` to let you run as root:
```patch
- case x$USER in
- xroot) abort "Don't run all.sh or crosstool.sh as root, it's dangerous" ;;
-  *) ;;
- esac
```

Run `./demo-arm.sh`  
You will need to help it with the downloads  
For any downloads that it fails, get the file yourself and put it in `/workspace/downloads`  
I think almost everything can be found on the berkely gnu mirror  

After everything has downloaded, extracted, and been patched once, the build will still fail because there is more patching we have to do  

But, at this point, add `--nounpack` to the eval line in `demo-arm.sh`  
This prevents crosstool from re-extracting the sources every time which would prevent you from being able to make patches  

**Patches to `gcc-3.2.3`**: 
```patch
--- a/include/obstack.h
+++ b/include/obstack.h
@@ 352
- #if defined __GNUC__ && defined __STDC__ && __STDC__
+ #if 0 && defined __GNUC__ && defined __STDC__ && __STDC__
```

```patch
--- a/gcc/collect2.c
+++ b/gcc/collect2.c
@@ 1590
- redir_handle = open (redir, O_WRONLY | O_TRUNC | O_CREAT);
+ redir_handle = open (redir, O_WRONLY | O_TRUNC | O_CREAT, 0600);
```

```patch
--- a/gcc/config/arm/linux-elf.h
+++ b/gcc/config/arm/linux-elf.h
@@ 37 
- #define SUBTARGET_EXTRA_LINK_SPEC " -m armelf_linux -p"
+ #define SUBTARGET_EXTRA_LINK_SPEC " -m armelfb_linux -p"

```

There were a brutal number of lvalue issues in `gcc/cp/decl.c`. Most of them had to do with the `current_binding_level` macro being set equal to something. You can basically search this file for `current_binding_level = ` and replace all of those like so (example):
```patch
--- a/gcc/cp/decl.c
+++ b/gcc/cp/decl.c
@@ 4295
- current_binding_level = b;
+ scope_chain->bin = b;
```

**crosstool.sh patch for glibc issue**:

```patch
--- a/crosstool.sh
+++ b/crosstool.sh
@@ 541
AR=${TARGET}-ar RANLIB=${TARGET}-ranlib \
+ libc_cv_gcc_unwind_find_fde=yes \
    ${GLIBC_DIR}/configure --prefix=/usr \
```

**Transplant Binutils**:  

The `ar` `ranlib` and `addr2line` that crosstool builds will segfault when they are run  
To fix this, I "transplanted" working versions of those programs from the generic Ubuntu arm CC toolchain.

Install it:
```sh
apt-get install -y binutils-arm-linux-gnueabi
```

Then, add the following to `crosstool.sh`:  
```sh
# Line 393 (right after Build binutils section):
rm -f /opt/crosstool/gcc-3.2.3-glibc-2.2.5/armeb-unknown-linux-gnu/bin/armeb-unknown-linux-gnu-ar
rm -f /opt/crosstool/gcc-3.2.3-glibc-2.2.5/armeb-unknown-linux-gnu/bin/armeb-unknown-linux-gnu-ranlib
rm -f /opt/crosstool/gcc-3.2.3-glibc-2.2.5/armeb-unknown-linux-gnu/bin/armeb-unknown-linux-gnu-addr2line

ln -s /usr/bin/arm-linux-gnueabi-ranlib /opt/crosstool/gcc-3.2.3-glibc-2.2.5/armeb-unknown-linux-gnu/bin/armeb-unknown-linux-gnu-ranlib
ln -s /usr/bin/arm-linux-gnueabi-ar /opt/crosstool/gcc-3.2.3-glibc-2.2.5/armeb-unknown-linux-gnu/bin/armeb-unknown-linux-gnu-ar
ln -s /usr/bin/arm-linux-gnueabi-addr2line /opt/crosstool/gcc-3.2.3-glibc-2.2.5/armeb-unknown-linux-gnu/bin/armeb-unknown-linux-gnu-addr2line
```
