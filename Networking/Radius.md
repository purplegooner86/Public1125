## Installing Radius in Ubuntu 22.04

https://docs.beamnetworks.dev/en/linux/networking/freeradius-install

```sh
sudo apt-get update
sudo apt install freeradius mariadb-server freeradius-mysql freeradius-utils -y
sudo systemctl enable freeradius
sudo mysql_secure_installation
# no root password, [unix_socket auth: n] n y y y y

sudo mysql -u root -p
MariaDB [(none)]> CREATE DATABASE radius;
MariaDB [(none)]> CREATE USER 'radius'@'localhost' IDENTIFIED by 'PASSWORD';
MariaDB [(none)]> GRANT ALL PRIVILEGES ON radius.* TO 'radius'@'localhost';
MariaDB [(none)]> FLUSH PRIVILEGES;
MariaDB [(none)]> quit;

sudo su -
root> mysql -u root -p radius < /etc/freeradius/3.0/mods-config/sql/main/mysql/schema.sql
root> exit

sudo ln -s /etc/freeradius/3.0/mods-available/sql /etc/freeradius/3.0/mods-enabled/

sudo vim /etc/freeradius/3.0/mods-enabled/sql
Changes: 
    driver="rlm_sql_${dialect}"
    dialect="mysql"
    uncomment read_clients=yes
    uncomment client_table="nas"
    comment out the entire tls section in the mysql section
    uncomment the Connection info: section (server, port, login, password)
        password should be "PASSWORD" - radius@localhost password from above
    change the line after # Read database-specific queries
    to:
    $INCLUDE ${modconfdir}/sql/main/mysql/queries.conf

sudo chgrp -h freerad /etc/freeradius/3.0/mods-available/sql
sudo chown -R freerad:freerad /etc/freeradius/3.0/mods-enabled/sql
sudo systemctl restart freeradius
```

Add a user and test:  
```sh
sudo mysql -u root -p
MariaDB [(none)]> use radius;
MariaDB [radius]> INSERT INTO radcheck (id, username, attribute, op, value) VALUES (1, 'gooner', 'Cleartext-Password',':=', 'abcd1234');

# This should output Access-Accept
radtest gooner abcd1234 127.0.0.1 100 testing123
```

In RADIUS Access-Request packet, the NAS-IP-Address attribute (RADIUS attribute 4) provides the identifying IP Address of the requesting Network Access Server (NAS)  


<br />

## Setup MikroTik to use the RADIUS Server

Setup RADIUS:
```
sudo vim /etc/freeradius/3.0/clients.conf
# Add a new client entry:
client mikrotik-router {
        ipaddr = 10.10.10.1
        secret = abcd1234
        nas_type = other
}

MariaDB [radius]> INSERT INTO radreply (username, attribute, op, value) values("gooner", "MikroTik-Group", ":=", "full");

MariaDB [radius]> INSERT INTO nas (nasname, shortname, type, ports, secret, server, community, description) values('10.10.10.1', 'mikrotik-client', 'other', NULL, 'abcd1234', NULL, NULL, 'MikroTik Client Router');
```

Setup MikroTik:  
- Go to Winbox
- Radius > +
  - check login (only)
  - Put Radius IP in Address
  - Put 'abcd1234' in secret
  - All other defaults are fine. Apply + OK

Use Radius to auth to Winbox:  
- Go to Winbox
- System > Users > AAA 
  - Check Use RADIUS

Use Radius to auth OpenVPN:
- Go to Winbox
- Radius > +
  - Check ppp (only)
  - Put Radius IP in Address
  - Put 'abcd1234' in secret
  - All other defaults are fine. Apply + OK
- PPP > PPP Authentication&Accounting
  - Check Use RADIUS
