## Installing

https://www.youtube.com/watch?v=LRx8QIzxsUQ

Virtual Box Settings:
- Type: Linux
- Version: Linux 2.6 / 3.x / 4.x / 5.x (64-bit)
- Base Memory: 512 MB
- Disk Size: 1 GB

Boot it with ISO, login as root  
Run `setup-alpine`
- keyboard layout: us
- variant: us
- Timezone: EST
- Allow root ssh login? yes
- Which disk(s) would you like to use? sda
- How would you like to use it? sys

Power it off, remove the iso, reboot  

<br />

## Random Config Stuff

```sh
apk add vim
vim /etc/apk/repositories
# Uncomment the community line
apk update

apk add tmux  
apk add zsh
apk add zsh-vcs # This is necessary
apk add git
```

Gonna have to put this in `.zshrc`:
```
ZSH_AUTOSUGGEST_HIGHLIGHT_STYLE='fg=4'
```

<br />

## Networking Stuff

### **Detecting New Network Interfaces**

`vim /etc/network/interfaces`
```sh
# If you just added a new interface that uses DHCP, add an entry like so:
auto eth1
iface eth1 inet dhcp
```
`service networking restart`  

### **Setting a static IP address**

`vim /etc/network/interfaces`
```sh
# Update your entry like so:
auto eth1
iface eth1 inet static
address 172.16.69.11/24
gateway 172.16.69.1

# gateway is optional
```

<br />

## Enable SSH Root Logon with Password

`vim /etc/ssh/sshd_config`
- change "PermitRootLogin prohibit-password" to "PermitRootLogin yes" 

`service sshd restart`  

<br />

## Turning Alpine into a Router

```sh
# Enable forwarding:
# vim /etc/sysctl.conf
# add:
net.ipv4.ip_forward = 1
net.ipv6.conf.all.forwarding = 1

sudo sysctl -p
```
