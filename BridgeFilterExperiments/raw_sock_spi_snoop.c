/*
 * arm-unknown-linux-gnueabi-gcc raw_sock_spi_snoop.c -o raw_sock_spi_snoop
 * esp_spi_sniff.c
 *
 * Linux 2.4-era AF_PACKET raw socket ESP/SPI matcher.
 *
 * Build:
 *   gcc -Wall -O2 -o esp_spi_sniff esp_spi_sniff.c
 *
 * Run as root:
 *   ./esp_spi_sniff eth0 0x12345678
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>

#ifndef IPPROTO_ESP
#define IPPROTO_ESP 50
#endif

#ifndef ETH_P_8021Q
#define ETH_P_8021Q 0x8100
#endif

static unsigned long parse_spi(const char *s)
{
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 0);

    if (!s[0] || *end != '\0' || v > 0xffffffffUL) {
        fprintf(stderr, "Invalid SPI: %s\n", s);
        exit(1);
    }

    return v;
}

static int get_ifindex(int fd, const char *ifname)
{
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl(SIOCGIFINDEX)");
        exit(1);
    }

    return ifr.ifr_ifindex;
}

int main(int argc, char **argv)
{
    int fd;
    int ifindex;
    unsigned long wanted_spi;
    struct sockaddr_ll bind_addr;
    unsigned char buf[65536];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <interface> <spi>\n", argv[0]);
        fprintf(stderr, "Example: %s eth0 0x12345678\n", argv[0]);
        return 1;
    }

    wanted_spi = parse_spi(argv[2]);

    /*
     * ETH_P_ALL means receive all Ethernet protocols.
     * You could use ETH_P_IP to reduce traffic, but ETH_P_ALL lets us
     * handle VLAN-tagged IPv4 frames manually.
     */
    fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0) {
        perror("socket(AF_PACKET)");
        return 1;
    }

    ifindex = get_ifindex(fd, argv[1]);

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sll_family   = AF_PACKET;
    bind_addr.sll_protocol = htons(ETH_P_ALL);
    bind_addr.sll_ifindex  = ifindex;

    if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("bind(AF_PACKET)");
        close(fd);
        return 1;
    }

    printf("Listening on %s for IPv4 ESP packets with SPI 0x%08lx\n",
           argv[1], wanted_spi);

    for (;;) {
        ssize_t n;
        struct sockaddr_ll from;
        socklen_t fromlen = sizeof(from);
        unsigned int off;
        unsigned short ethertype;
        struct iphdr *ip;
        unsigned int ip_hlen;
        unsigned char *esp;
        unsigned long spi;

        n = recvfrom(fd, buf, sizeof(buf), 0,
                     (struct sockaddr *)&from, &fromlen);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("recvfrom");
            break;
        }

        /*
         * Skip packets transmitted by this host/interface.
         * PACKET_OUTGOING existed in the old packet socket API.
         */
#ifdef PACKET_OUTGOING
        if (from.sll_pkttype == PACKET_OUTGOING)
            continue;
#endif

        if (n < ETH_HLEN)
            continue;

        off = ETH_HLEN;
        ethertype = ((unsigned short)buf[12] << 8) | buf[13];

        /*
         * Basic single VLAN tag handling:
         * Ethernet:
         *   dst[6] src[6] 0x8100 tci[2] inner_ethertype[2]
         */
        if (ethertype == ETH_P_8021Q) {
            if (n < 18)
                continue;
            ethertype = ((unsigned short)buf[16] << 8) | buf[17];
            off = 18;
        }

        if (ethertype != ETH_P_IP)
            continue;

        if (n < off + sizeof(struct iphdr))
            continue;

        ip = (struct iphdr *)(buf + off);

        if (ip->version != 4)
            continue;

        ip_hlen = ip->ihl * 4;
        if (ip_hlen < sizeof(struct iphdr))
            continue;

        if (n < off + ip_hlen + 8)
            continue;

        if (ip->protocol != IPPROTO_ESP)
            continue;

        /*
         * ESP header starts immediately after the IPv4 header:
         *
         *   0..3  SPI
         *   4..7  Sequence Number
         */
        esp = buf + off + ip_hlen;

        spi = ((unsigned long)esp[0] << 24) |
              ((unsigned long)esp[1] << 16) |
              ((unsigned long)esp[2] << 8)  |
              ((unsigned long)esp[3]);

        if (spi == wanted_spi) {
            struct in_addr src, dst;
            src.s_addr = ip->saddr;
            dst.s_addr = ip->daddr;

            printf("Matched ESP SPI 0x%08lx: %s -> ",
                   spi, inet_ntoa(src));
            printf("%s, seq=%lu, frame_len=%ld\n",
                   inet_ntoa(dst),
                   ((unsigned long)esp[4] << 24) |
                   ((unsigned long)esp[5] << 16) |
                   ((unsigned long)esp[6] << 8)  |
                   ((unsigned long)esp[7]),
                   (long)n);
        }
    }

    close(fd);
    return 0;
}