# Malta Board Modification Experiments

I am starting with the emulation setup described [here](../CrossToolExamples/mips32be_malta_linux2.6_gcc4.6.3.md)  

First, this will go through how modifications can be made to the Linux kernel and to the Qemu source so that we can create a modified Linux kernel that will **only** run on our version of Qemu with a modified Malta board  

Then, this will go through how you would debug your broken Qemu board if you only had the modified Kernel version and not the modified version of Qemu  

<br />

## Malta Board Diagram

I think this is just helpful to have as a reference...  


<br />

## Qemu and Kernel Mods

The Malta FPGA, the I2C SMBus, and all the EEPROMs attached to the SMBus are not actually necessary to emulate a Malta kernel with the Qemu Malta board. For simplicity, I removed all of those components from Qemu's malta board source. This is probably not a necessary step, but I am mentioning it for the sake of completeness  

The *breaking* change is going to be to move the address that Qemu maps the GT64120 registers to from `0x1be00000` to `0x1de00000`.  

If the corresponding change is not made to the Linux kernel code which defines where the base for the GT64120 registers is, the kernel will not boot  

**Qemu Patch**:
```patch
--- a/hw/mips/malta.c
+++ b/hw/mips/malta.c
@@ -130,31 +130,33 @@ static void bl_setup_gt64120_jump_kernel(void **p, uint64_t run_addr,
 
     /* setup MEM-to-PCI0 mapping as done by YAMON */
 
+    #define MODIFIED_GT64120_REG_BASE 0x1de00000
+
     /* move GT64120 registers from 0x14000000 to 0x1be00000 */
     bl_gen_write_u32(p, /* GT_ISD */
                      cpu_mips_phys_to_kseg1(NULL, 0x14000000 + 0x68),
-                     cpu_to_gt32(0x1be00000 << 3));
+                     cpu_to_gt32(MODIFIED_GT64120_REG_BASE << 3));
 
     /* setup PCI0 io window to 0x18000000-0x181fffff */
     bl_gen_write_u32(p, /* GT_PCI0IOLD */
-                     cpu_mips_phys_to_kseg1(NULL, 0x1be00000 + 0x48),
+                     cpu_mips_phys_to_kseg1(NULL, MODIFIED_GT64120_REG_BASE + 0x48),
                      cpu_to_gt32(0x18000000 << 3));
     bl_gen_write_u32(p, /* GT_PCI0IOHD */
-                     cpu_mips_phys_to_kseg1(NULL, 0x1be00000 + 0x50),
+                     cpu_mips_phys_to_kseg1(NULL, MODIFIED_GT64120_REG_BASE + 0x50),
                      cpu_to_gt32(0x08000000 << 3));
 
     /* setup PCI0 mem windows */
     bl_gen_write_u32(p, /* GT_PCI0M0LD */
-                     cpu_mips_phys_to_kseg1(NULL, 0x1be00000 + 0x58),
+                     cpu_mips_phys_to_kseg1(NULL, MODIFIED_GT64120_REG_BASE + 0x58),
                      cpu_to_gt32(0x10000000 << 3));
     bl_gen_write_u32(p, /* GT_PCI0M0HD */
-                     cpu_mips_phys_to_kseg1(NULL, 0x1be00000 + 0x60),
+                     cpu_mips_phys_to_kseg1(NULL, MODIFIED_GT64120_REG_BASE + 0x60),
                      cpu_to_gt32(0x07e00000 << 3));
     bl_gen_write_u32(p, /* GT_PCI0M1LD */
-                     cpu_mips_phys_to_kseg1(NULL, 0x1be00000 + 0x80),
+                     cpu_mips_phys_to_kseg1(NULL, MODIFIED_GT64120_REG_BASE + 0x80),
                      cpu_to_gt32(0x18200000 << 3));
     bl_gen_write_u32(p, /* GT_PCI0M1HD */
-                     cpu_mips_phys_to_kseg1(NULL, 0x1be00000 + 0x88),
+                     cpu_mips_phys_to_kseg1(NULL, MODIFIED_GT64120_REG_BASE + 0x88),
                      cpu_to_gt32(0x0bc00000 << 3));
 
 #undef cpu_to_gt32
@@ -165,12 +167,12 @@ static void bl_setup_gt64120_jump_kernel(void **p, uint64_t run_addr,
      * write routing configuration to the config data register.
      */
     bl_gen_write_u32(p, /* GT_PCI0_CFGADDR */
-                     cpu_mips_phys_to_kseg1(NULL, 0x1be00000 + 0xcf8),
+                     cpu_mips_phys_to_kseg1(NULL, MODIFIED_GT64120_REG_BASE + 0xcf8),
                      tswap32((1 << 31) /* ConfigEn */
                              | PCI_BUILD_BDF(0, PIIX4_PCI_DEVFN) << 8
                              | PIIX_PIRQCA));
     bl_gen_write_u32(p, /* GT_PCI0_CFGDATA */
-                     cpu_mips_phys_to_kseg1(NULL, 0x1be00000 + 0xcfc),
+                     cpu_mips_phys_to_kseg1(NULL, MODIFIED_GT64120_REG_BASE + 0xcfc),
                      tswap32(ldl_be_p(pci_pins_cfg)));
 
     bl_gen_jump_kernel(p,
```

**Kernel Patch**:  
```patch
--- a/arch/mips/include/asm/mach-malta/mach-gt64120.h
+++ b/arch/mips/include/asm/mach-malta/mach-gt64120.h
 #ifndef _ASM_MACH_MIPS_MACH_GT64120_DEP_H
 #define _ASM_MACH_MIPS_MACH_GT64120_DEP_H
 
-#define MIPS_GT_BASE	0x1be00000
+#define MIPS_GT_BASE	0x1de00000
```

<br />

## Debugging Techniques

### **Qemu Monitor**:  
Add this to the end of qemu run command:
```sh
-monitor unix:/tmp/qemu-monitor,server,nowait
```
Then, attach to the monitor with socat:
```sh
socat -,echo=0,icanon=0 unix-connect:/tmp/qemu-monitor
```

You can look at where Qemu has mapped things in memory with `info mtree`  
In this case, this is the relevant entry:  
```
000000001be00000-000000001be00fff (prio 0, i/o): gt64120-isd
```

<br />

### **Qemu Trace**:

Show available trace types related to memory:  
```sh
qemu-system-mips -trace help | grep memory
```

Add this to the end of qemu run command:
```sh
-trace memory_region_ops_read \
-D qemu_trace.log
```

This is what is in the end of the log:  
```
memory_region_ops_write cpu 0 mr 0x61d6b6204a50 addr 0x1de00c00 value 0x0 size 4 name 'empty-slot'
memory_region_ops_read cpu 0 mr 0x61d6b6204a50 addr 0x1de00048 value 0x0 size 4 name 'empty-slot'
memory_region_ops_read cpu 0 mr 0x61d6b6204a50 addr 0x1de000f0 value 0x0 size 4 name 'empty-slot'
memory_region_ops_read cpu 0 mr 0x61d6b6204a50 addr 0x1de00048 value 0x0 size 4 name 'empty-slot'
```

'empty-slot' in this case is an indication that something is reading from a region that should be mapped to a device but is not  

<br />

### **GDB**:

Add the following to qemu run command to have it stop execution early and wait for GDB to be attached:
```sh
-S -s
```

Attach gdb:
```sh
gdb-multiarch
(gdb) file ~/Documents/linux-2.6.32.16/vmlinux
(gdb) target remote :1234
(gdb) c
# Press ctrl+c after it has hung...
(gdb) bt
#0  0x8010f7d0 in prom_putchar ()
#1  0x804c8960 in early_console_write ()
#2  0x801399c8 in __call_console_drivers ()
```

In this case, the back trace is not immediately as useful  

Taking the 'empty-slot' accesses from the trace output above though and combining that with GDB is very useful  

First it is necessary to understand how virtual memory addresses are being mapped to physical memory addresses. In this case (from kernel source):  
```C
// in arch/mips/include/asm/addrspace.h
#define KSEG1ADDR(a) (CPHYSADDR(a) | KSEG1)
#define KSEG1 0xa0000000
#define CPHYSADDR(a) ((_ACAST32_(a)) & 0x1fffffff)
```

So, in summary:  
```
physical_address = (virtual_address & 0x1fffffff)
virtual_address = physical_address | 0xa0000000
```

So, in this case:
```
physical_address | 0xa0000000 = virtual_address
0x1de00048 | 0xa0000000 = 0xbde00048
```

**Note**: You can verify your math by accessing nearby addresses in gdb. The qemu trace log should be populated with those accesses. For example:  
```sh
# GDB:
(gdb) x /1bx 0xbde06969

# Corresponding generated trace output:
memory_region_ops_read cpu -1 mr 0x5e8e91165000 addr 0x1de06969 value 0x0 size 1 name 'empty-slot'
```

So, `0xbde00048` is the virtual address that will be accessed which will correspond to the read from `0x1de00048` that we see in the trace output  

Put a watcher on the virtual address to see when it is accessed by the kernel:
```sh
(gdb) rwatch *(volatile unsigned char*)0xbde00048
(gdb) c
(gdb) bt
#0  0x804c52d4 in prom_init ()
#1  0x804c6c40 in setup_arch ()
#2  0x804c3938 in start_kernel ()
```

Ghidra decompilation of prom_init around `0x804c52d4`:
```C
else {
    mips_revision_sconid = 0xfffffffe;
    _DAT_bde00c00 = 0;
    uVar8 = (((_DAT_bde00048 >> 0x10) << 0x18 | (_DAT_bde00048 >> 0x18) << 0x10) >> 0x10) +
            ((_DAT_bde00048 & 0xff) << 8 | _DAT_bde00048 >> 8 & 0xff) * 0x10000;
    uVar5 = (((_DAT_bde000f0 >> 0x10) << 0x18 | (_DAT_bde000f0 >> 0x18) << 0x10) >> 0x10) +
            ((_DAT_bde000f0 & 0xff) << 8 | _DAT_bde000f0 >> 8 & 0xff) * 0x10000;
    _pcictrl_gt64120 = 0xbde00000;
    if ((uVar5 & uVar8) != 0) {
    uVar5 = ~uVar8 & uVar5;
    _DAT_bde000f0 =
            (((uVar5 >> 0x10) << 0x18 | (uVar5 >> 0x18) << 0x10) >> 0x10) +
            ((uVar5 & 0xff) << 8 | uVar5 >> 8 & 0xff) * 0x10000;
    }
```

This would give you the tip that `_pcictrl_gt64120` is being set to `0xbde00000` which is physical address `0x1de00000`  
