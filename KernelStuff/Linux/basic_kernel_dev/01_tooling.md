# Tooling for Linux Rootkit Development

## Kernel Debug Symbols

Production Linux Kernels have their symbols stripped. This means that debug symbols are removed from the kernel image  

This is why the content of `/usr/lib/debug` is empty on a out of the box Ubuntu installation  

To get kernel debugging symbols:  

First:  
`sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys C8CAB6595FDFF622`   

Then:  
```
codename=$(lsb_release -c | awk  '{print $2}')
sudo tee /etc/apt/sources.list.d/ddebs.list << EOF  
deb http://ddebs.ubuntu.com/ ${codename}      main restricted universe multiverse
deb http://ddebs.ubuntu.com/ ${codename}-security main restricted universe multiverse
deb http://ddebs.ubuntu.com/ ${codename}-updates  main restricted universe multiverse
deb http://ddebs.ubuntu.com/ ${codename}-proposed main restricted universe multiverse
EOF
```

Then:  
`sudo apt-get update`  

Finally:  
`sudo apt-get install linux-image-$(uname -r)-dbgsym`  

`/usr/lib/debug/boot` should now have a `vmlinux-...` file  

<br />

## Setting Up VSCode Intellisense

It's a lot easier to write kernel code when VSCode can resolve the types and definitions we need  

First, find your kernel version: `uname -r`  

Next, make sure headers for that version are installed:
- `ls /usr/src/linux-headers-$(uname -r)`  
- If they are not, I think: `sudo apt-get install linux-headers-$(uname -r)` will do the trick

Next, add the following to `.vscode/c_cpp_properties.json`  
Replace `5.15.0-78-generic` with the output of `uname -r`  

```json
"configurations": [
    ...
    {
        "name": "Driver",
        "compilerPath": "/usr/bin/gcc",
        "cStandard": "c17",
        "cppStandard": "c++14",
        "intelliSenseMode": "linux-gcc-x64",
        "defines": [ "__KERNEL__", "MODULE", "KBUILD_MODNAME" ],
        "includePath": [
            "${workspaceFolder}/**",
            "/usr/src/linux-headers-5.15.0-78-generic/include/",
            "/usr/src/linux-headers-5.15.0-78-generic/arch/x86/include",
            "/usr/src/linux-headers-5.15.0-78-generic/arch/x86/include/generated",
            "/usr/src/linux-headers-5.15.0-78-generic/arch/x86/include/uapi",
            "/usr/src/linux-headers-5.15.0-78-generic/arch/x86/include/generated/uapi",
            "/usr/src/linux-headers-5.15.0-78-generic/include/uapi",
            "/usr/src/linux-headers-5.15.0-78-generic/include/generated/uapi",
            "/usr/src/linux-headers-5.15.0-78-generic/ubuntu/include"
        ]
    }
]
```

The do `Ctrl+Shift+p` and look for `C/C++: Select a Configuration...` and change to the "Driver" configuration  

Note: The `.vscode` directory gets created in the directory that vscode is open to. So, these changes will not persist for all of VSCode; only when you open VSCode at that directory  

<br />

## Crash Utility Installation

Crash is the main tool we will use to analyze the kernel  

Crash is very finnicky to setup. Installing with apt does not work for me  

I have not found a way to get crash to work on a 22.04 Ubuntu installation  

Instead, this will detail how to install it on a 20.04 Ubuntu installation  

- `sudo apt-get install -y g++ software-properties-common lsb-release build-essential libtool libncurses5-dev libz-dev bison texinfo` 
- `sudo apt-get install cmake`  
- Download crash tar from: https://github.com/crash-utility/crash/releases/tag/8.0.3 and unarchive it  
- `cd crash`  
- `make`  
- `sudo ./crash -s --hex /usr/lib/debug/boot/vmlinux-5.15.0-78-generic`  

Make a symbolic link:  
- `cd /usr/bin`  
- `sudo ln -s /home/user/crash-8.0.3/crash crash`  

<br />

## Helpful Crash Commands  

print the type of the symbol init_task: 
- crash> `whatis init_task`  

print the comm element of the symbol init_task:
- crash> `p init_task->comm`  

print the address of the comm element of the symbol init_task:
- crash> `p &(init_task->comm)`

view the typedef of the struct `mm_struct`:  
- crash> `struct mm_struct`  

view some things about the VA `0xffffffff9b21bf68` (including permissions):
- crash> `vtop 0xffffffff9b21bf68`  
    - part of output: `(PRESENT|RW|ACCESSED|DIRTY|PSE|GLOBAL|NX)`
    - the presence of `RW` means this page is writable  

print the address of the sys_call_table:
- crash> `p &sys_call_table`  

print first element of the sys_call_table:
- crash> `p sys_call_table[0]`  

print 15 as a hex unsigned long:  
- crash> `printf "0x%llx\n", (unsigned long) 15`

read 5 8-bit values from the `is_module_sig_enforced` symbol's address: (`rd` command is read memory)  
- crash> `rd -8 is_module_sig_enforced 5`  

dissassemble `is_module_sig_enforced`:  
- crash> `dis is_module_sig_enforced`  

