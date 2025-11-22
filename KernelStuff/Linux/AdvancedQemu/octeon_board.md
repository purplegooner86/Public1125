# WIP

## 64-bit MIPS Virtual - Physical Mapping

To understand this, its very important to understand that multiple Virtual Addresses can map to a single Physical Address

There are multiple segments of the kernel's VA space which map to physical memory

| Segment | Virtual Addr | Physical Addr | Description |
| ------- | -------------------- | ------------ | --------- | 
| `KSEG0` | `0xffffffff8xxxxxxx` | `0x0xxxxxxx` | Cached    |
| `KSEG1` | `0xffffffffaxxxxxxx` | `0x0xxxxxxx` | Un-Cached |

There are other segments as well which map to physical memory but they are more complicated, so I will omit them here  

I think this is easiest to wrap your head around with an example:  

If we want to find the virtual address that maps to the physical address `0x1be00000` we have two options  

```
KSEG0: 0x1be00000 + 0xffffffff80000000 = 0xffffffff9be00000
KSEG1: 0x1be00000 + 0xffffffffa0000000 = 0xffffffffbbe00000
```

So, the Virtual Addresses `0xffffffff9be00000` and `0xffffffffbbe00000` both map to same physical memory location  

This is interesting to verify with Qemu memory access trace and GDB:  

```sh
(gdb) x /1bx 0xffffffffbbe00000
# Generated Trace Output:
memory_region_ops_read ... addr 0x1be00000 ...

(gdb) x /1bx 0xffffffff9be00000
# Generated Trace Output:
memory_region_ops_read ... addr 0x1be00000 ...
```

**Note**: `memory_region_ops_read` traces are only generated for i/o regions, not ram regions. So the above experiment will only work if the physical address being read from is in an i/o region, not a ram region (check with `info mtree` in Qemu monitor)  

**Converting Virtual to Physical**:  

| Virtual Address Range | Segment | Convert to Phys Formula |
| -- | -- | -- |
| `0xffffffff80000000` - `0xffffffff9fffffff` | `KSEG0` | `-0xffffffff80000000`|
| `0xffffffffa0000000` - `0xffffffffbfffffff` | `KSEG1` | `-0xffffffffa0000000` |

If a Virtual address falls outside of those two ranges, it may be in one of the other segments which I have ommited here for the sake of simplicity (`XKUSEG`, `XKSSEG`, `XKPHYS`, `XKSEG`)  
