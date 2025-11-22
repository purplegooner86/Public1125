# Windows Networking Configuration

Bunch of random stuff to help with Windows networking configuring  

Set IP address with PowerShell:
```Powershell
# Get the ifIndex for the interface you are looking at:
Get-NetAdapter

# Create a new IP address:
New-NetIPAddress -IPAddress 172.16.12.1 -DefaultGateway 172.16.12.1 -PrefixLength 24 -InterfaceIndex 6
```

Set Default DNS server with PowerShell:
```Powershell
# Get the ifIndex for the interface you are looking at:
Get-NetAdapter

# Change the DNS server address:
Set-DnsClientServerAddress -InterfaceIndex 6 -ServerAddresses 127.0.0.1

# Check your work:
Get-DnsClientServerAddress -InterfaceIndex 6
```


