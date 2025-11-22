# Active Directory SysAdmin Stuff

## Joining a Windows 11 machine to the domain:
```Powershell
# Powershell (from workstation):
Add-Computer -DomainName ScottDomain.local
# You will be prompted for creds, enter domain admin creds
```
After you reboot, you should be able to login with Scottdomain\Administrator

## Disabling Windows Defender

1. Go into Security Settings > Virus & threat protection settings > Switch Tamper Protection to off
2. Run local Group Policy Editor (gpedit in search box)
3. Administrative Templates > Windows Components > Microsoft Defender Antivirus > Real-time protection. Enable `turn off real time protection`
4. Run regedit as Administrator, go to `HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Microsoft\Windows Defender`
5. Create a new DWORD called `DisableAntiSpyware` and set it to 1
6. Right click start button, click Run, type `services.msc`
7. Find Windows Update service, double-click on it to open the properties window
8. Under the general tab, choose disabled from the startup type section
9. Reboot
10. `(Get-Service windefend).Status` should show "Stopped"

## Installing Active Directory Cmdlets

```Powershell
Get-WindowsCapability -Name RSAT.ActiveDirectory* -Online | Add-WindowsCapability -Online
```
 

