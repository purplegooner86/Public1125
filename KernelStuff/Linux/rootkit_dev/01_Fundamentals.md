# Linux Rootkit Fundamentals

## Writing to read-only memory

There are multiple ways to do this  

1. Double Map Memory using PTEs
2. Setting the writable bit in the PTE for a page
3. Clearing the CR0.WP bit

### **(1) Double Mapping Memory**

Permissions are enforced on virtual pages through page table entries (PTEs)  
- The same set of physical pages can be mapped into multiple VAs using PTEs  

Given an array of `struct page` pointers, `vmap()` maps those pages to a new VA range  
- For writable mapping use `PAGE_KERNEL`
- `vmap()` always maps memory as non-executable  

Double mapping pages to make them writable is done in this lab: [map_writable](../LKXR_labs/map_writable/map_writable.c)

### **(2) Setting the writable bit in the PTE**

For writable page, set `_PAGE_RW` in `pte_t.pte`  
For executable page, clearn `_PAGE_NX` in `pte_t.pte`  

PTEs are not tied to any particular CPU (or core) so changing PTE contents changes page permission across all CPUs  

This lab shows how to add execute permissions to a page to execute some shellcode: [tamper_pte](../LKXR_labs/tamper_pte/tamper_pte.c)  

### **(3) Clearing the CR0.WP bit** 

The CR0.WP bit controls the enforcement of read-only protection in kernel mode
- Kernel modules can write to the CR0 register  

The functions `native_write_cr0()` etc. have checks to determine if the caller is attempting to tamper with sensitive bits such as CR0.WP and block it. Therefore, raw/inline assembly must be used to write to CR0.  

Thread migration to another CPU can be prevented  

There are several ways of compiling with the custom assemly needed:
- One is to compile GNU assembly (GAS) and link to it. Shown here: [gas_method](../LKXR_labs/bypass_readonly/gas_method/module.c)
- Another way is to inline the assembly into the C code. Shown here: [inline_asm_method](../LKXR_labs/bypass_readonly/inline_asm_method/bypass_readonly.c)  

<br />

## Patching Kernel Functions

Kernel functions reside in `.text` section of ELF files  
`.text` sections are executable, but read-only. This is enforced by page table permissions on the VA  
This means writes will require a read-only bypass  

Patching function prolog can skip function execution and return attacker controlled value to the caller  

Patching the `is_module_sig_enforced()` function is shown here:  [patch_kernel](../LKXR_labs/patch_kernel/module.c)  











