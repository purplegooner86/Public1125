# Bridge Filter + Raw Socket Experiments

I was trying to figure out if a raw `PACKET_SOCKET` on a slave interface would see frames before its bridge's bridgefilter hooks dropped them  

I was doing this testing with Linux 2.6.32 The answer is *sometimes*  

The hook on `NF_BR_PRE_ROUTING` caused the bridgefilter to drop the frame *before* the raw socket ever saw it. The rest of the hooks, `NF_BR_FORWARD` , `NF_BR_LOCAL_IN` , `NF_BR_LOCAL_OUT` , and `NF_BR_POST_ROUTING` took effect after the raw socket saw the frame  

I was using a basic ESP SPI snooping utility to test this  

Test Sequence:
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

Without `NF_BR_PRE_ROUTING`, the spi snooper sees the frames before they are dropped  
With `NF_BR_PRE_ROUTING`, the spi snooper does not see the frames
