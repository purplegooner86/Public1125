# Low Level

## Control Registers 

Control registers are used to set and query a CPU's operating mode  
They contain security related configuration  
Control registers can only be read from and written to in kernel mode  
You can modify them directly (eg. `mov cr0, rax`) or use the kernel functions `read_crX()` and `write_crX()` to read and write them
- Some bits cannot be written using the kernel functions because of pinning  

| Register | Description |
| -------- | ----------- |
| CR0      | Write Protect (bit 16) – Prevents kernel mode write to read only pages |
| CR2 | Virtual address that caused the last page fault (exception 14) | 
| CR3 | Page Table Base Physical Address. Physical address of PXE/PML4 page |
| CR4 | A Lot of stuff (see below) |
| CR8 | CPU's current interrupt level (bits 0-3) |

CR4:
- User-Mode Instruction Prevention (bit 11) – Blocks SGDT, SIDT, SLDT, SMSW, and STR from being executed from user mode.
- FS/GS Base (bit 16) – Controls enabling of instructions to access `[RD|WR][FS|GS]BASE`
- SMEP (bit 20) – Controls supervisor-mode execution prevention.
- SMAP (bit 21) – Controls supervisor-mode access prevention.
- CET (bit 23) – Control-flow Enforcement Technology.  

<br />

## Model Specific Registers (MSRs)

Used by the OS to configure the CPU  
Accessible only in kernel mode by MSR address using special CPU instructions `rdmsr` and `wrmsr`  
Categorized as architectural and vendor specific MSRs  
Kernel functions `rdmsrl_safe()` and `wrmsrl_safe()` can be used to read and write MSRs  

<br />

## Thread Affinity  

Threads are allowed to run on a specific set of CPUs  

`set_cpus_allowed_ptr()` modifies this mask  

<br />

This lab shows how to affinitize a thread to a CPU and read the values of MSRs and CRs: [access_registers](../LKXR_labs/access_registers/access_registers.c)  



