/*
 * arm-unknown-linux-gnueabi-gcc raw_sock_spi_snoop_v2.c -o raw_sock_spi_snoop_v2
 * raw_sock_spi_tunnel.c
 *
 * Linux 2.4-era AF_PACKET raw socket ESP/SPI matcher.
 * Binds to a specific interface, matches ESP SPI, XOR-decodes the ESP payload,
 * expects an encapsulated IPv4 UDP packet, and forwards it.
 *
 * Build:
 *   arm-unknown-linux-gnueabi-gcc -Wall -O2 raw_sock_spi_tunnel.c -o raw_sock_spi_tunnel
 *
 * Run as root:
 *   ./raw_sock_spi_tunnel eth0 0x12345678
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
#include <netinet/udp.h>
#include <arpa/inet.h>

#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>

#ifndef IPPROTO_ESP
#define IPPROTO_ESP 50
#endif

#ifndef ETH_P_8021Q
#define ETH_P_8021Q 0x8100
#endif

#define BUF_SIZE 65536

/*
 * Hardcoded payload XOR byte.
 * Change this to match your tunnel.
 */
#define XOR_CONST 0xaa

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

static unsigned short ip_checksum(void *data, int len)
{
    unsigned long sum = 0;
    unsigned short *p = (unsigned short *)data;

    while (len > 1) {
        sum += *p++;
        len -= 2;
    }

    if (len == 1)
        sum += *((unsigned char *)p);

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return (unsigned short)(~sum);
}

int main(int argc, char **argv)
{
    int recv_fd;
    int send_fd;
    int ifindex;
    unsigned long wanted_spi;
    struct sockaddr_ll bind_addr;
    unsigned char buf[BUF_SIZE];
    unsigned char inner[BUF_SIZE];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <interface> <spi>\n", argv[0]);
        fprintf(stderr, "Example: %s eth0 0x12345678\n", argv[0]);
        return 1;
    }

    wanted_spi = parse_spi(argv[2]);

    /*
     * Receive raw Ethernet frames on the selected interface.
     * ETH_P_ALL lets us manually handle VLAN-tagged IPv4 frames.
     */
    recv_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (recv_fd < 0) {
        perror("socket(AF_PACKET)");
        return 1;
    }

    ifindex = get_ifindex(recv_fd, argv[1]);

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sll_family   = AF_PACKET;
    bind_addr.sll_protocol = htons(ETH_P_ALL);
    bind_addr.sll_ifindex  = ifindex;

    if (bind(recv_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("bind(AF_PACKET)");
        close(recv_fd);
        return 1;
    }

    /*
     * Send decrypted inner IPv4 packets.
     */
    send_fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (send_fd < 0) {
        perror("socket(AF_INET, SOCK_RAW)");
        close(recv_fd);
        return 1;
    }

    {
        int one = 1;
        if (setsockopt(send_fd, IPPROTO_IP, IP_HDRINCL,
                       &one, sizeof(one)) < 0) {
            perror("setsockopt(IP_HDRINCL)");
            close(send_fd);
            close(recv_fd);
            return 1;
        }
    }

    printf("Listening on %s for IPv4 ESP packets with SPI 0x%08lx, XOR 0x%02x\n",
           argv[1], wanted_spi, XOR_CONST);

    for (;;) {
        ssize_t n;
        struct sockaddr_ll from;
        socklen_t fromlen = sizeof(from);

        unsigned int off;
        unsigned short ethertype;

        struct iphdr *outer_ip;
        unsigned int outer_ip_hlen;
        unsigned int outer_ip_total_len;

        unsigned char *esp;
        unsigned int esp_len;
        unsigned long spi;
        unsigned long seq;

        unsigned char *crypted;
        unsigned int crypted_len;

        struct iphdr *inner_ip;
        struct udphdr *inner_udp;
        unsigned int inner_ip_hlen;
        unsigned int inner_total_len;

        struct sockaddr_in dst;
        unsigned int i;

        n = recvfrom(recv_fd, buf, sizeof(buf), 0,
                     (struct sockaddr *)&from, &fromlen);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("recvfrom");
            break;
        }

#ifdef PACKET_OUTGOING
        /*
         * Avoid seeing packets sent by this host on the same interface.
         */
        if (from.sll_pkttype == PACKET_OUTGOING)
            continue;
#endif

        if (n < ETH_HLEN)
            continue;

        off = ETH_HLEN;
        ethertype = ((unsigned short)buf[12] << 8) | buf[13];

        /*
         * Handle a single 802.1Q VLAN tag.
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

        outer_ip = (struct iphdr *)(buf + off);

        if (outer_ip->version != 4)
            continue;

        outer_ip_hlen = outer_ip->ihl * 4;
        if (outer_ip_hlen < sizeof(struct iphdr))
            continue;

        if (n < off + outer_ip_hlen + 8)
            continue;

        if (outer_ip->protocol != IPPROTO_ESP)
            continue;

        outer_ip_total_len = ntohs(outer_ip->tot_len);
        if (outer_ip_total_len < outer_ip_hlen + 8)
            continue;

        if (outer_ip_total_len > (unsigned int)(n - off))
            continue;

        esp = buf + off + outer_ip_hlen;
        esp_len = outer_ip_total_len - outer_ip_hlen;

        if (esp_len < 8)
            continue;

        /*
         * ESP:
         *   0..3 SPI
         *   4..7 Sequence Number
         *   8..  encrypted payload
         */
        spi = ((unsigned long)esp[0] << 24) |
              ((unsigned long)esp[1] << 16) |
              ((unsigned long)esp[2] << 8)  |
              ((unsigned long)esp[3]);

        if (spi != wanted_spi)
            continue;

        seq = ((unsigned long)esp[4] << 24) |
              ((unsigned long)esp[5] << 16) |
              ((unsigned long)esp[6] << 8)  |
              ((unsigned long)esp[7]);

        crypted = esp + 8;
        crypted_len = esp_len - 8;

        if (crypted_len < sizeof(struct iphdr))
            continue;

        if (crypted_len > sizeof(inner))
            continue;

        for (i = 0; i < crypted_len; i++)
            inner[i] = crypted[i] ^ XOR_CONST;

        /*
         * The decrypted payload is expected to be a complete inner IPv4 packet.
         */
        inner_ip = (struct iphdr *)inner;

        if (inner_ip->version != 4)
            continue;

        inner_ip_hlen = inner_ip->ihl * 4;
        if (inner_ip_hlen < sizeof(struct iphdr))
            continue;

        if (crypted_len < inner_ip_hlen + sizeof(struct udphdr))
            continue;

        inner_total_len = ntohs(inner_ip->tot_len);

        if (inner_total_len < inner_ip_hlen + sizeof(struct udphdr))
            continue;

        if (inner_total_len > crypted_len)
            continue;

        if (inner_ip->protocol != IPPROTO_UDP)
            continue;

        inner_udp = (struct udphdr *)(inner + inner_ip_hlen);

        /*
         * Recompute IPv4 header checksum before forwarding.
         * UDP checksum is preserved from the encapsulated packet.
         */
        inner_ip->check = 0;
        inner_ip->check = ip_checksum(inner_ip, inner_ip_hlen);

        memset(&dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        dst.sin_addr.s_addr = inner_ip->daddr;

        if (sendto(send_fd,
                   inner,
                   inner_total_len,
                   0,
                   (struct sockaddr *)&dst,
                   sizeof(dst)) < 0) {
            perror("sendto");
            continue;
        }

        {
            struct in_addr outer_src;
            struct in_addr outer_dst;
            struct in_addr inner_src;
            struct in_addr inner_dst;
            char outer_src_s[32];
            char outer_dst_s[32];
            char inner_src_s[32];
            char inner_dst_s[32];

            outer_src.s_addr = outer_ip->saddr;
            outer_dst.s_addr = outer_ip->daddr;
            inner_src.s_addr = inner_ip->saddr;
            inner_dst.s_addr = inner_ip->daddr;

            strncpy(outer_src_s, inet_ntoa(outer_src), sizeof(outer_src_s) - 1);
            outer_src_s[sizeof(outer_src_s) - 1] = '\0';

            strncpy(outer_dst_s, inet_ntoa(outer_dst), sizeof(outer_dst_s) - 1);
            outer_dst_s[sizeof(outer_dst_s) - 1] = '\0';

            strncpy(inner_src_s, inet_ntoa(inner_src), sizeof(inner_src_s) - 1);
            inner_src_s[sizeof(inner_src_s) - 1] = '\0';

            strncpy(inner_dst_s, inet_ntoa(inner_dst), sizeof(inner_dst_s) - 1);
            inner_dst_s[sizeof(inner_dst_s) - 1] = '\0';

            printf("ESP SPI 0x%08lx seq=%lu outer %s -> %s, forwarded UDP %s:%u -> %s:%u, %u bytes\n",
                   spi,
                   seq,
                   outer_src_s,
                   outer_dst_s,
                   inner_src_s,
                   ntohs(inner_udp->source),
                   inner_dst_s,
                   ntohs(inner_udp->dest),
                   inner_total_len);
        }
    }

    close(send_fd);
    close(recv_fd);
    return 0;
}