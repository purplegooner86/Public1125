# Linux Kernel Security Mitigations

## KASLR

KASLR randomizes the base VAs of the following kernel regions:
- Direct mapped physical memory
- vmalloc range
- vmemmap range  

<br />

## Kernel Stack Randomization

All system calls are dispatched by `do_syscall_64()`. The calling thread's kernel stack is used during such calls  
`do_syscall_64()` adds a random offset to the kernel mode stack before handling system calls  

This is enabled by the boot parameter `randomize_kstack_offset=1`  
- `add_random_kstack_offset()` performs the randomization  

<br />

## SMEP

Supervisor Mode Execution Prevention (SMEP)  

Blocks execution of instructions from user mode code pages with kernel privileges  
- This prevents an attacker from putting shellcode in user space and making kernel RIP point to it after exploitation  

Is a hardware feature indicated by `X86_FEATURE_SMEP` bit in `cpuinfo_x86.x86_capability`
- Enabled by CR4 SMEP bit (20)  

<br />

## SMAP

Supervisor Mode Access Prevention (SMAP)  

Blocks access to user pages when executing in kernel mode  

However, access to user VAS is necessary during system calls  
- User mode access is allowed from the kernel if Alignment Check (AC) bit in EFLAGS is set  
- CPU instructions `STAC` and `CLAC` set and clear the AC bit  
- `copy_from_user()` and `copy_to_user()` do this internally  

<br />

## Kernel Page Table Isolation (KPTI)

KPTI works by employing two different page tables, one for user and another for kernel mode execution  

When executing user code the page table only maps kernel virtual address space partially 
- Only critical handlers are mapped - syscall, interrupt, exception, etc.  

When executing kernel code, the full kernel address space is mapped and user mode is mapped as NX  


