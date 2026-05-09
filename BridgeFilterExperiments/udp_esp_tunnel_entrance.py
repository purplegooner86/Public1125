#!/usr/bin/env python
#
# udp_esp_tunnel_entrance.py
#
# Listen on UDP, encapsulate each datagram as:
#
#   outer IPv4 proto ESP
#     ESP SPI 0x12345678
#     ESP SEQ increasing
#     XOR(0xaa) of inner IPv4/UDP packet
#
# Usage:
#   sudo python udp_esp_tunnel_entrance.py \
#       <listen_ip> <listen_port> \
#       <outer_dst_ip> \
#       <inner_src_ip> <inner_dst_ip> <inner_dst_port>
#
# Example:
#   sudo python udp_esp_tunnel_entrance.py \
#       0.0.0.0 5555 \
#       192.168.1.10 \
#       10.9.0.1 10.9.0.2 9999
#
# Then send into it:
#   echo hello | nc -u 127.0.0.1 5555

import socket
import struct
import sys

SPI = 0x12345678
XOR_CONST = 0xaa
OUTER_IP_PROTO_ESP = 50

DEFAULT_INNER_SRC_PORT = 44444


def checksum(data):
    if len(data) & 1:
        data += b"\x00"

    s = 0
    for i in range(0, len(data), 2):
        s += (data[i] << 8) + data[i + 1]

    while s >> 16:
        s = (s & 0xffff) + (s >> 16)

    return (~s) & 0xffff


def udp_checksum(src_ip_packed, dst_ip_packed, udp_hdr, payload):
    pseudo = (
        src_ip_packed +
        dst_ip_packed +
        struct.pack("!BBH", 0, socket.IPPROTO_UDP, len(udp_hdr) + len(payload))
    )

    csum = checksum(pseudo + udp_hdr + payload)

    # In IPv4 UDP, checksum 0 means disabled. If computed checksum is 0,
    # transmit it as 0xffff.
    if csum == 0:
        csum = 0xffff

    return csum


def build_inner_udp_packet(src_ip, dst_ip, src_port, dst_port, payload):
    udp_len = 8 + len(payload)

    src = socket.inet_aton(src_ip)
    dst = socket.inet_aton(dst_ip)

    udp_hdr_zero = struct.pack(
        "!HHHH",
        src_port,
        dst_port,
        udp_len,
        0
    )

    udp_sum = udp_checksum(src, dst, udp_hdr_zero, payload)

    udp_hdr = struct.pack(
        "!HHHH",
        src_port,
        dst_port,
        udp_len,
        udp_sum
    )

    ver_ihl = 0x45
    tos = 0
    total_len = 20 + udp_len
    ident = 0x4242
    flags_frag = 0
    ttl = 64
    proto = socket.IPPROTO_UDP
    ip_sum = 0

    ip_hdr_zero = struct.pack(
        "!BBHHHBBH4s4s",
        ver_ihl,
        tos,
        total_len,
        ident,
        flags_frag,
        ttl,
        proto,
        ip_sum,
        src,
        dst
    )

    ip_sum = checksum(ip_hdr_zero)

    ip_hdr = struct.pack(
        "!BBHHHBBH4s4s",
        ver_ihl,
        tos,
        total_len,
        ident,
        flags_frag,
        ttl,
        proto,
        ip_sum,
        src,
        dst
    )

    return ip_hdr + udp_hdr + payload


def build_outer_esp_packet(outer_src_ip, outer_dst_ip, seq, encrypted_payload):
    esp_hdr = struct.pack("!II", SPI, seq)
    esp_packet = esp_hdr + encrypted_payload

    ver_ihl = 0x45
    tos = 0
    total_len = 20 + len(esp_packet)
    ident = seq & 0xffff
    flags_frag = 0
    ttl = 64
    proto = OUTER_IP_PROTO_ESP
    ip_sum = 0

    src = socket.inet_aton(outer_src_ip)
    dst = socket.inet_aton(outer_dst_ip)

    ip_hdr_zero = struct.pack(
        "!BBHHHBBH4s4s",
        ver_ihl,
        tos,
        total_len,
        ident,
        flags_frag,
        ttl,
        proto,
        ip_sum,
        src,
        dst
    )

    ip_sum = checksum(ip_hdr_zero)

    ip_hdr = struct.pack(
        "!BBHHHBBH4s4s",
        ver_ihl,
        tos,
        total_len,
        ident,
        flags_frag,
        ttl,
        proto,
        ip_sum,
        src,
        dst
    )

    return ip_hdr + esp_packet


def pick_outer_source_ip(outer_dst_ip):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((outer_dst_ip, 9))
        return s.getsockname()[0]
    finally:
        s.close()


def main():
    if len(sys.argv) != 7:
        print(
            "Usage: %s <listen_ip> <listen_port> "
            "<outer_dst_ip> "
            "<inner_src_ip> <inner_dst_ip> <inner_dst_port>" %
            sys.argv[0]
        )
        sys.exit(1)

    listen_ip = sys.argv[1]
    listen_port = int(sys.argv[2])
    outer_dst_ip = sys.argv[3]
    inner_src_ip = sys.argv[4]
    inner_dst_ip = sys.argv[5]
    inner_dst_port = int(sys.argv[6])

    outer_src_ip = pick_outer_source_ip(outer_dst_ip)

    in_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    in_sock.bind((listen_ip, listen_port))

    out_sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_RAW)
    out_sock.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, 1)

    print("Listening UDP on %s:%d" % (listen_ip, listen_port))
    print("Outer ESP: %s -> %s SPI=0x%08x XOR=0x%02x" %
          (outer_src_ip, outer_dst_ip, SPI, XOR_CONST))
    print("Inner UDP destination: %s:%d" %
          (inner_dst_ip, inner_dst_port))

    seq = 1

    while True:
        payload, peer = in_sock.recvfrom(65507)

        inner_src_port = peer[1]
        if inner_src_port <= 0:
            inner_src_port = DEFAULT_INNER_SRC_PORT

        inner_plain = build_inner_udp_packet(
            inner_src_ip,
            inner_dst_ip,
            inner_src_port,
            inner_dst_port,
            payload
        )

        encrypted = bytes([b ^ XOR_CONST for b in inner_plain])

        packet = build_outer_esp_packet(
            outer_src_ip,
            outer_dst_ip,
            seq,
            encrypted
        )

        out_sock.sendto(packet, (outer_dst_ip, 0))

        print("seq=%u from %s:%u, encapsulated %u byte UDP payload, sent %u byte ESP packet" %
              (seq, peer[0], peer[1], len(payload), len(packet)))

        seq = (seq + 1) & 0xffffffff
        if seq == 0:
            seq = 1


if __name__ == "__main__":
    main()