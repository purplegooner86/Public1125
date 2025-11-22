# Driver Development Basics

## Driver Entry Points

DriverEntry()
- Only entry point known to Windows kernel mode loader
- Mandatory function required for all kernel mode drivers  
- Registers driver unload and other routines  
- Allocates global (driver-wide) resources  
- Driver is unloaded if DriverEntry() returns anything other than `STATUS_SUCCESS`  

DriverUnload()  
- Registered in `DRIVER_OBJECT.DriverUnload`  
- Last function called before driver image is unmapped
- Frees resources allocated by DriverEntry()  

<br />

## Kernel Pools

Pools are like the heap but for the kernel  
There are non-paged pools, non-paged non-executable pools, paged, and session pools  

Memory is typically allocated from the Paged pool (`POOL_FLAG_PAGED`) if that memory is only accessed at IRQL < DISPATCH_LEVEL  

Memory is typically allocated from Non-paged pool (`POOL_FLAG_NON_PAGED`) for non-executable memory that can be accessed at any IRQL  

Pool memory is allocated and freed with:  
- `ExAllocatePool2()`
- `ExFreePool`  

Pool allocations are tagged with a 4 character tag (pool tag)  
- If you want the pool to have the tag `'Scot'`, use `'tocS'` as the pool tag parameter  

<br />

## Unicode Strings

Unicode strings are the default string format for all kernel mode functions that take strings as parameters  

Consists of a `UNICODE_STRING` header structure that points to a `WCHAR` buffer  
- Try (WinDBG): `dt nt!_UNICODE_STRING`

Do not need to be `NULL` terminated 

The `Length` field is the length (in bytes) of the string in the Buffer  
The `MaximumLength` field is the size of the entire buffer in bytes  

Wide char strings are created by appending the "L" character in front of a constant ASCII string  
Creating a Unicode string example:  
```C++
UNICODE_STRING MyString = RTL_CONSTANT_STRING(L"Hello World");  
```

See [AppendString](../WKID_labs/AppendString/AppendString.cpp) for an example of working with Unicode strings in Kernel Mode  

<br />

## Dynamic Linking

Drivers can link to NTOSKRNL exports statically or dynamically  

Dynamic Linking is neccessary when an NTOSKRNL entity is exported, but is not included in `NTOSKRNL.lib`  

Dynamic linking means using `MmGetSystemRoutineAddress()` to get a pointer to the global at runtime  

See an example of this: [LinkExport](../WKID_labs/LinkExport/LinkExport.cpp)  

Check if a symbol ("NtBuildNumber" in this case) is exported:  
`dumpbin --exports C:\windows\system32\ntoskrnl.exe | findstr NtBuildNumber`  





