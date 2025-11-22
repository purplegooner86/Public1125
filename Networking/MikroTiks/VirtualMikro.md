# Setting up a VirtualBox MikroTik

## Installing winbox on Linux

- `sudo apt install snapd`
- `sudo snap install winbox`
- `winbox`
- Or maybe `/snap/bin/winbox`

<br />

## Installing RouterOS in VirtualBox

https://www.youtube.com/watch?v=oHXkaHkSVVo
- When you are at the import vdi part, move chr-ver.vdi into the folder VBox created 


<br />

## Configuring the Router for a VirtualBox Internal network

- `/interface ethernet print`
- `/ip address print`
  - The non-bridged network interface is the one for the internal network
  - In this case `ether2`
- Setup the Router's intnet interface:
  - `/interface bridge add name=local`
  - `/interface bridge port add interface=ether2 bridge=local`
  - `/ip address add address=10.10.10.1/24 interface=local`
- Setup a DHCP server:
  - `/ip dhcp-server setup`
    - Follow the prompts...
    - put `local` as the dhcp server interface:
    - and everything else should be good as the defaults


<br />


## Assorted RouterOS Commands

- `/system reset-configuration`
- `system shutdown`
- `/ip address print`
- `/ip firewall filter print`
- `/ip firewall filter disable 4`
- Typing `?` with anything typed into the console will print the options
- You can scroll up/down with shift + Page up / down
- Mikrotik was not asking for a DHCP IP address on one of its interfaces. Tell it to do so with:  
  - `/ip dhcp-client add interface=ether2 disabled=no`
- Default rule to drop all forwards:  
  - `/ip firewall filter add action=drop chain=forward`

<br />

## Understanding Firewall Rules and Routes

Here is the scenario:  
- We have a Mikrotik with two interfaces, one with the address space `172.21.12.0/24` and the other with the address space `172.21.11.0/24`  
- Each interface has a host on its respective subnet. The one on the first subnet has ip address `172.21.12.254` and the one on the other subnet has ip address `172.21.11.254`

What is going to happen if you `ping 172.21.11.254`? No response
- This might lead you to believe that the Mikrotik does not forward between different subnets by default, but this is not the case
- What is happening, is neither hosts know how where to send traffic destined for the other subnet  
- You need to tell them both (both because the reply also needs to be routed) how to do this
- On host 1:
```sh
sudo ip route add 172.21.11.0/24 via 172.21.12.1 dev enp0s9
```
- On host 2:
```sh
sudo ip route add 172.21.12.0/24 via 172.21.11.1 dev enp0s9
```
- Now you should be able to get a response to your ping  

Difference between `forward` and `input` chains in firewall rules
- Setting a rule for the `forward` chain affects how the Router will act on packets it is supposed to be forwarding
- Setting a rule for the `input` chain affects how the Router will act on packets destined for itself  
- So, if you wanted to add a rule to enable winbox management of the router, you would add a rule to accept tcp/8291 on the `input` chain  
- If you want to be able to nc to host 2 from host 1 on port 8888, you would set that rule up on the `forward` chain  
- If you don't want the host on the `.12` network to be pingable (or reachable at all) from the `.11` network, setup a rule on the `forward` chain to drop everything


