# Helpful Windows Debugger Commands + Cmd Line Commands


Use the C++ evaluater in WinDBG to evaluate expressions with `??`:  
- WinDBG> `?? sizeof(nt!_UNICODE_STRING)`  

`find | grep` Windows equivalent:
- `dir filename.txt /s/b`  

`grep` = `findstr`
- example: `dumpbin --exports C:\windows\system32\ntoskrnl.exe | findstr NtBuildNumber`  

Disassemble the NtCreateFile() function:
- WinDBG> `uf ntdll!NtCreateFile`  

`dt` is a very useful command:  





