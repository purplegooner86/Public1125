# Processes and Threads

## Processes

All processes in Windows run in user mode except for System, system idle, and minimal processes  

System Process (PID = 4)  
- Container for kernel mode threads
    - Threads created by drivers and by `NTOSKRNL`  
- `nt!PsInitialSystemProcess` (exported variable) points to the process structure (`EPROCESS`) of the system process  
- Handle table is called the system handle table  

System Idle Process (PID = 0)  
- Container for all idle threads (one per logical CPU)  
- `nt!PsIdleProcess` points to the `EPROCESS` of the idle process  

Minimal Processes (Registry and MemCompression)  
- User mode process with user mode virtual address space
- All code execution happend through kernel mode threads running in the context of the minimal process  

<br />

## Kernel Threads

Created using `PsCreateSystemThread`  
- `ProcessHandle = NULL` creates thread in system process  

Can attach to the address space of any process  

<br />

## Labs

There is a lot more here  
In terms of being practical, here are the related labs:  
[EnumProcess](../WKID_labs/EnumProcess/EnumProcess.cpp)
- Learn how to enumerate processes in the system and query the full path to the process's executable image  

[QueryProcess](../WKID_labs/QueryProcess/QueryProcess.cpp)  
- Learn how to query the command line of any process in the system  

[KillProcess](../WKID_labs/KillProcess/KillProcess.cpp)
- Learn how to perform operations on any process in the system  



