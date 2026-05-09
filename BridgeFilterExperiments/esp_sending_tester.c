/*
 * esp_sending_tester.c
 *
 * Runs on HOST
 * 
 * Build:
 *   gcc -Wall -O2 -o esp_sending_tester esp_sending_tester.c
 *
 * Run as root:
 *   ./esp_sending_tester <src-ip> <dst-ip> <spi>
 *
 * Example:
 *   ./esp_sending_tester 192.168.1.10 192.168.1.20 0x12345678
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#ifndef IPPROTO_ESP
#define IPPROTO_ESP 50
#endif

static unsigned short ip_checksum(void *vdata, size_t length)
{
    unsigned char *data = (unsigned char *)vdata;
    unsigned long acc = 0xffff;

    while (length > 1) {
        unsigned short word;
        memcpy(&word, data, 2);
        acc += ntohs(word);
        if (acc > 0xffff)
            acc -= 0xffff;
        data += 2;
        length -= 2;
    }

    if (length > 0) {
        unsigned short word = 0;
        memcpy(&word, data, 1);
        acc += ntohs(word);
        if (acc > 0xffff)
            acc -= 0xffff;
    }

    return htons(~acc & 0xffff);
}

static unsigned long parse_u32(const char *s)
{
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 0);

    if (!s[0] || *end != '\0' || v > 0xffffffffUL) {
        fprintf(stderr, "Invalid 32-bit value: %s\n", s);
        exit(1);
    }

    return v;
}

int main(int argc, char **argv)
{
    int fd;
    int one = 1;
    unsigned char packet[1500];
    struct iphdr *ip;
    unsigned char *esp;
    struct sockaddr_in dst;
    unsigned long spi;
    unsigned long seq = 1;
    size_t ip_len;
    size_t esp_len;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <src-ip> <dst-ip> <spi>\n", argv[0]);
        fprintf(stderr, "Example: %s 192.168.1.10 192.168.1.20 0x12345678\n", argv[0]);
        return 1;
    }

    spi = parse_u32(argv[3]);

    memset(packet, 0, sizeof(packet));

    ip = (struct iphdr *)packet;
    esp = packet + sizeof(struct iphdr);

    /*
     * Minimal fake ESP:
     *
     *   0..3  SPI
     *   4..7  Sequence Number
     *   8..   dummy encrypted/authenticated-looking bytes
     *
     * This is not valid decryptable ESP. It is only enough for the
     * AF_PACKET sniffer to match IPv4 protocol 50 + SPI.
     */
    esp[0] = (spi >> 24) & 0xff;
    esp[1] = (spi >> 16) & 0xff;
    esp[2] = (spi >> 8)  & 0xff;
    esp[3] = spi & 0xff;

    esp[4] = (seq >> 24) & 0xff;
    esp[5] = (seq >> 16) & 0xff;
    esp[6] = (seq >> 8)  & 0xff;
    esp[7] = seq & 0xff;

    memcpy(esp + 8, "TESTESP", 7);
    esp_len = 8 + 7;

    ip_len = sizeof(struct iphdr) + esp_len;

    ip->version = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->tot_len = htons(ip_len);
    ip->id = htons(0x4242);
    ip->frag_off = htons(0);
    ip->ttl = 64;
    ip->protocol = IPPROTO_ESP;
    ip->check = 0;

    if (inet_aton(argv[1], (struct in_addr *)&ip->saddr) == 0) {
        fprintf(stderr, "Invalid source IP: %s\n", argv[1]);
        return 1;
    }

    if (inet_aton(argv[2], (struct in_addr *)&ip->daddr) == 0) {
        fprintf(stderr, "Invalid destination IP: %s\n", argv[2]);
        return 1;
    }

    ip->check = ip_checksum(ip, sizeof(struct iphdr));

    fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt(IP_HDRINCL)");
        close(fd);
        return 1;
    }

    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = ip->daddr;

    if (sendto(fd, packet, ip_len, 0,
               (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        perror("sendto");
        close(fd);
        return 1;
    }

    printf("Sent fake ESP packet: %s -> %s, SPI 0x%08lx\n",
           argv[1], argv[2], spi);

    close(fd);
    return 0;
}