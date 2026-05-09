# Bridge Filter + Raw Socket Experiments

I was trying to figure out if a raw `PACKET_SOCKET` on a slave interface would see frames before its bridge's bridgefilter hooks dropped them  

I was doing this testing with Linux 2.6.32. The answer is yes.  
I got very confused, because I didn't notice that the ARP was being dropped, so my host never actually sent the packet

**Test Sequence:**

On Host:
```sh
# ARP just gets dropped, so need this
sudo ip neigh add 10.86.1.10 lladdr 52:54:00:12:34:56 dev tap0 nud permanent
```

Then, on guest:

```sh
brctl addbr br0
brctl addif br0 eth0
ifconfig br0 up
ip addr add 10.86.1.10/24 dev br0

# Wait for forwarding state
# Confirm you can ping from host

insmod bridgefilter_drop.ko

# Confirm you can no longer ping from host
# ie: br_drop_hook: dropping packet on hook 1

./raw_sock_spi_snoop eth0 0x12345678
```

The packet socket beats the bridge filter
