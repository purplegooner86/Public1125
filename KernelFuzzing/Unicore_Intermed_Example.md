# Fuzzing the Kernel With Unicore (Intermediate)

This assumes you understand everything in [Unicore_Basic_Example.md](./Unicore_Basic_Example.md)  

The kernel+rootfs setup I am using is what is described here: [arm32le_versatile_linux2.6_gcc4.6.md](../CrossToolExamples/arm32le_versatile_linux2.6_gcc4.6.md)  

I made some modifications to the kernel and to buildroot to get iptables:  
Kernel menuconfig:
```
Networking support > Networking options 
    > [*] Network packet filtering framework (Netfilter)
        > Core Netfilter Configuration
            <*> Netfilter connection tracking support
            -*- Netfilter Xtables support
        > IP: Netfilter Configuration
              <*> IPv4 connection tracking support
              <*> IP tables support
              <*> Packet filtering
```
Buildroot menuconfig:
```
Target packages > Networking applications > [*] iptables
```

With that setup, the kernel function `ipt_do_table` should be called for every packet received by the target. You can verify this with a kernel debugger  

I modified the `ipt_do_table` function to add a unique code path that will cause a null pointer dereference under certain conditions. Specifically, at the bottom of the function, I added:
```C
int *bad_bad = NULL;

if (ip->saddr == 0) {
    // Just doing some arbitrary stuff
    printk("In this code path\n");
    if (ip->daddr != 1) {
        if (hotdrop) {
            return NF_DROP;
        }
        return verdict;
    }
    printk("Intentional bad dereference: %d\n", *bad_bad);
    return NF_DROP;
}
```

I didn't feel like counting, so I also added informational prints to tell me the size of the `iphdr` structure and the offset to the `network_header` pointer in an `sk_buff`  

**Note**: In many kernels, `network_header` is an int which describes the offset into the `head*` member where the network header lives. But in this kernel, its just an `unsigned char *`  

```C
int offset_to_skb_network_header = (uint32_t)(&skb->network_header) - (uint32_t)skb;

printk("Offset to skb->network_header: 0x%x\n", offset_to_skb_network_header);
printk("Size of iphdr: %lx\n", sizeof(struct iphdr));
```

The offset to network_header was `0x8c`  
And an iphdr is `0x14` bytes  

The entry to `ipt_do_table` was `0xc024fa2c` and the `add sp,sp` instruction for the one return was at `0xc024fea4`  

`sk_buff *skb` is the first parameter (`r0`) to `ipt_do_table`  

I want AFL to fuzz the entire 0x14 byte ip header  

Here is my config.py:
```python
import os
import struct

from unicorn import Uc
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1
from unicorefuzz.unicorefuzz import Unicorefuzz

# A place to put scratch memory to. Non-kernelspace address should be fine.
SCRATCH_ADDR = 0x80000
# How much scratch to add. We don't ask for much. Default should be fine.
SCRATCH_SIZE = 0x1000

# The page size used by the emulator. Optional.
# PAGE_SIZE = 0x1000

ARCH = "arm"
GDB_HOST = "localhost"
GDB_PORT = 1234

GDB_PATH = "gdb-multiarch"

BREAK_ADDR = 0xc024fa2c

EXITS = [0xc024fea4]

# The location used to store data and logs
WORKDIR = "/home/purple/Documents/fuzzing_research/unicore_working_dirs"

# Where AFL input should be read from
AFL_INPUTS = WORKDIR + "/afl_inputs"
# Where AFL output should be placed at
AFL_OUTPUTS =  WORKDIR + "/afl_outputs"

# Optional AFL dictionary
AFL_DICT = None

def init_func(uc, rip):
    pass

def place_input(ucf: Unicorefuzz, uc: Uc, input: bytes) -> None:

    if len(input) > 0x14:
        os._exit(0)  # too big!

    # Read pointer to sk_buff
    r0 = uc.reg_read(UC_ARM_REG_R0)

    ucf.map_page(uc, r0)  # ensure sk_buff is mapped

    # network_header pointer is at offset 0x8c
    ip_hdr_ptr = struct.unpack("<I", uc.mem_read(r0 + 0x8c, 4))[0]

    ucf.map_page(uc, ip_hdr_ptr)  # ensure network_header is mapped
    uc.mem_write(ip_hdr_ptr, input)  # insert afl input
```

Here is a screenshot of the AFL output:  
![AFL_ipt_do_table.png](../../../Images/AFL_ipt_do_table.png)

<br />

## Analyzing AFL Paths

```sh
ls -l afl_outputs/master/queue/
# id:000000,time:0,orig:something
# id:000001,src:000000,time:717,op:havoc,rep:64,+cov
```

Each of these files represents an execution path and contains the first input that led to the execution of that path  

The `src` part of the filename shows the parent input that this path was mutated from. So, in this case, path 1 mutated from path 0, which was the seed input `something`  



