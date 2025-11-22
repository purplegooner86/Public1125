#include "ntifs.h"

#define __MODULE__ "BuildDriver"

EXTERN_C DRIVER_INITIALIZE  DriverEntry;
EXTERN_C DRIVER_UNLOAD DriverUnload;

EXTERN_C NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("+++ %s.sys Loaded +++\n", __MODULE__);

    DriverObject->DriverUnload = DriverUnload;

    return STATUS_SUCCESS;
}

EXTERN_C VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    DbgPrint("--- %s.sys Unloaded ---\n", __MODULE__);
}