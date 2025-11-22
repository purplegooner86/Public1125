# Install Windows 11 VM in Vbox  

1. Downlaod the ISO from Microsoft  
2. Create a new VM in VirtualBox, specify normal requirements  
3. Attach the ISO in Storage > Optical drive
4. Boot
5. If you get to a weird UEFI terminal thing, just type 'exit' in the prompt  
6. When you get to the first Windows setup screen, press Shift+F10 (may need to press Fn as well) to bring up a command prompt
7. type `regedit`  
8. Go to `HKLM\SYSTEM\Setup` right click on setup, select New > Key. Name is `LabConfig`
9. Right click on LabConfig, select New > DWORD. Name it `BypassTPMCheck`, after it is create it set its value to 1
10. Do the same thing for a DWORD `BypassSecureBootCheck` 
11. Exit regedit and the command prompt
12. Go through the installer prompts, you want the Custom: Install Windows only (advanced) option  
13. The VM should reboot. If takes a very long time on a screen with VirtualBox in the middle, shut it down, remove the ISO from the optical drive, and then reboot  
14. When you get to the country or region menu in the setup Windows process, press Shift+F10 again to bring up command prompt.
16. Type OOBE\BYPASSNRO in the command prompt. This should reboot the VM.
17. Bring the command prompt back up, and type `ipconfig /release`
18. Close the command prompt. This will enable you to complete setup without internet, which will allow you to setup without using a Microsoft account.
19. Once you are done, run an admin cmd prompt and type `ipconfig /renew` to get internet back
