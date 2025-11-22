## Setting up OVPN Server

1. Create CA certificate
```RouterOS
/certificate add name=ca-cert common-name=ca-cert days-valid=365 key-size=2048 key-usage=crl-sign,key-cert-sign
```
2. Create Server certificate
```RouterOS
/certificate add name=server-cert common-name=server-cert days-valid=365 key-size=2048 key-usage=digital-signature,key-encipherment,tls-server
```
3. Create System certificate
```RouterOS
/certificate add name=client-cert common-name=client-cert days-valid=365 key-size=2048 key-usage=tls-client
```
4. Sign the CA certificate
```RouterOS
/certificate sign ca-cert name=ca-cert ca-crl-host=""
```
5. Sign the Client and Server certificates
```RouterOS
/certificate sign server-cert name=server-cert ca=ca-cert
/certificate sign client-cert name=client-cert ca=ca-cert
```
6. Trust the Server certificate
```RouterOS
/certificate set server-cert trusted=yes
```
7. Export the certs
```RouterOS
/certificate export-certificate ca-cert export-passphrase=""
/certificate export-certificate client-cert export-passphrase="abcd1234"
```
8. Download the Files
   - In Winbox you should now have 3 cert files in the Files tab. Download them
9. Create a PPP profile
```RouterOS
# "local" is the name of the bridge interface for the LAN
# "dhcp_pool0" is the name of the IP pool for the LAN
/ppp profile add name="VPN-Profile" use-encryption=yes local-address=10.10.10.1 dns-server=10.10.10.1 remote-address="dhcp_pool0" bridge="local"
```
10.  Create a VPN User
```RouterOS
/ppp secret add name=username password="abcd1234" service=ovpn profile="VPN-Profile"
```
11.  Set up the OpenVPN Server Interface
```RouterOS
/interface ovpn-server server set default-profile=VPN-Profile certificate=server-cert require-client-certificate=yes auth=sha1 cipher=aes128,aes192,aes256 enabled=yes port=1194 mode=ip
```
12.  Add Firewall Rules
```RouterOS
/ip firewall filter add action=accept chain=input dst-port=1194 in-interface=ether1 protocol=udp
/ip firewall filter add action=accept chain=input dst-port=1194 in-interface=ether1 protocol=tcp
```
13.  Enable Proxy ARP
```RouterOS
# local is the name of the LAN bridge interface
/interface bridge set local arp=proxy-arp
```
14. forward firewall rules:
```RouterOS
# This only applies if you have a firewall rule set to drop everything on the forward chain by default
/ip firewall filter add action=accept chain=forward in-interface=<ovpn-username> out-interface=local_RouterNet place-before=0
/ip firewall filter add action=accept chain=forward in-interface=local_RouterNet out-interface=<ovpn-username> place-before=0
```

<br />

## Setting up OpenVPN Clients

First, Decrypt the client key:  
```sh
openssl rsa -in client.key -out key_decrypt.key
```
Now create a `.ovpn` file like the following:  

```ovpn
client
dev tun
proto tcp
remote 192.168.86.68 1194
resolv-retry infinite
nobind

persist-key
persist-tun
<ca>
-----BEGIN CERTIFICATE-----
(Copy paste ca.crt here)
-----END CERTIFICATE-----
</ca>

<cert>
-----BEGIN CERTIFICATE-----
(Copy paste client.crt here)
-----END CERTIFICATE-----
</cert>

<key>
-----BEGIN RSA PRIVATE KEY-----
(copy paste decrypted private key here)
-----END RSA PRIVATE KEY-----

remote-cert-tls server
cipher AES-256-CBC

auth-user-pass

# Set log file verbosity.
verb 3
```

Connect to the VPN:
- `sudo openvpn --config ovpn_config.ovpn`
- username is "username"