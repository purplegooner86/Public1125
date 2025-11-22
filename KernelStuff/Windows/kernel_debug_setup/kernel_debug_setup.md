# Setting up Win11 Kernel Debugging with Vbox

1. Get two Windows 11 VMs up and running  
2. Install WinDBG Preview on one of them  
3. Add a second network adapter to each VM in VirtualBox, and set the type to Hosbt-Only. Ensure the host only network you use has DHCP enabled.  
4. Get the 192... ip address for the debugger VM with `ipconfig`  
5. Go to `Windows Defender Firewall` settings and disable the firewall on both VMs
6. On the debugging target, in an admin cmd prompt, run the following:  
    - `bcdedit /debug on`  
    - `bcdedit /dbgsettings NET hostip:192.168.56.11 port:50005`
    - copy the key it prints out  
    - `bcdedit /set testsigning on`
7. On the WinDBG host, go to File > Attach to Kernel, specify the port 50005 and put the key in the key field  
8. Reboot the target, and you should see output in the DBG host's WinDBG prompt

## Making PrintDbg output to WinDBG

You need to add a registry key on the target to have the output of PrintDBG show up in the attached kernel debugger WinDBG  

Add the key `Debug Print Filter` at the path (in regedit on the target VM):  
`HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager\Debug Print Filter`  

Add a DWORD called `DEFAULT` with the value `0xf`  

To do this via command line:  
- `reg add "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Debug Print Filter" /v DEFAULT /t REG_DWORD /d 0xf`

You will need to reboot the target to have the changes take effect  

