# Building and Running Kernel Modules

## Setup EWDK

1. Download the EWDK ISO from Microsoft and Mount it  
2. In a non-admin terminal, change directory to where it is mounted ( ie: `cd D:\` ) 
3. run `.\LaunchBuildEnv.cmd`  
4. To change directory back to your C drive, once you are in the build environment, you need to run `c:`  

<br />

## Module Building

The following script will build modules from a directory with a `.vcxproj` file:   
```bat
:: Build all modules in the current directory hierarchy 
@echo off
for /r %%i in (*.vcxproj) do (
        echo "MODULE_BUILD: %%~dpi"
        pushd "%%~ni"
        msbuild /p:configuration=debug /p:platform=x64 /p:targetversion=windows10
        popd
)
```

See [basic_driver_setup](basic_driver_setup/Driver.cpp) for a sample basic driver to see what a .vcxproj file should look like  

For that example, in the EWDK build environment, from the basic_driver_setup directory, `..\..\module_build.cmd` would compile `Driver.cpp`  

This would generate a number of files in an `x64\debug` directory, including a `BuildDriver.sys` file, which is the actual kernel module to transfer to the target VM  

<br />

## Module Running

There is a `DriverRun.exe` executable, use it  

Examples:  
- `DriverRun.exe DriverName.sys` # load module 'DriverName'  
- `DriverRun.exe -u DriverName.sys` # unload module 'DriverName'  


