#include "../include/net.h"
#include "../include/e1000.h"

struct eth_hdr { uint8_t dest[6]; uint8_t src[6]; uint16_t type; } __attribute__((packed));
struct arp_hdr { uint16_t hw_type; uint16_t proto_type; uint8_t hw_len; uint8_t proto_len; uint16_t opcode; uint8_t sender_mac[6]; uint8_t sender_ip[4]; uint8_t target_mac[6]; uint8_t target_ip[4]; } __attribute__((packed));
struct ipv4_hdr { uint8_t ihl_version; uint8_t tos; uint16_t total_length; uint16_t id; uint16_t frag_offset; uint8_t ttl; uint8_t protocol; uint16_t checksum; uint8_t src_ip[4]; uint8_t dest_ip[4]; } __attribute__((packed));
struct icmp_hdr { uint8_t type; uint8_t code; uint16_t checksum; uint16_t id; uint16_t seq; } __attribute__((packed));
struct udp_hdr { uint16_t src_port; uint16_t dest_port; uint16_t length; uint16_t checksum; } __attribute__((packed));

typedef struct {
    uint8_t remote_ip[4];
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t seq_num;
    uint32_t ack_num;
    int state;
    uint8_t rx_buf[8192];
    int rx_len;
} tcp_socket_t;

tcp_socket_t tcp_sockets[16];
uint16_t ephem_port = 49152;

uint16_t htons(uint16_t v) { return (v >> 8) | (v << 8); }
uint32_t htonl(uint32_t v) { return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | ((v >> 24) & 0xFF); }

uint16_t net_checksum(void* data, int len) {
    uint16_t* ptr = (uint16_t*)data; uint32_t sum = 0;
    while(len > 1) { sum += *ptr++; len -= 2; }
    if(len > 0) sum += *((uint8_t*)ptr);
    while(sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

void net_send_arp(uint8_t* target_ip) {
    uint8_t packet[42];
    struct eth_hdr* eth = (struct eth_hdr*)packet;
    struct arp_hdr* arp = (struct arp_hdr*)(packet + 14);
    uint8_t* my_mac = get_mac_address();
    for(int i=0; i<6; i++) { eth->dest[i] = 0xFF; eth->src[i] = my_mac[i]; arp->sender_mac[i] = my_mac[i]; arp->target_mac[i] = 0x00; }
    eth->type = htons(0x0806);
    arp->hw_type = htons(1); arp->proto_type = htons(0x0800); arp->hw_len = 6; arp->proto_len = 4; arp->opcode = htons(1);
    uint8_t my_ip[4] = {10, 0, 2, 15};
    for(int i=0; i<4; i++) { arp->sender_ip[i] = my_ip[i]; arp->target_ip[i] = target_ip[i]; }
    e1000_send_packet(packet, 42);
}

void net_send_icmp(uint8_t* dest_ip, uint8_t* dest_mac) {
    uint8_t packet[74];
    struct eth_hdr* eth = (struct eth_hdr*)packet;
    struct ipv4_hdr* ip = (struct ipv4_hdr*)(packet + 14);
    struct icmp_hdr* icmp = (struct icmp_hdr*)(packet + 34);
    uint8_t* my_mac = get_mac_address();
    for(int i=0; i<6; i++) { eth->dest[i] = dest_mac[i]; eth->src[i] = my_mac[i]; }
    eth->type = htons(0x0800);
    ip->ihl_version = 0x45; ip->tos = 0; ip->total_length = htons(60); ip->id = htons(1); ip->frag_offset = 0; ip->ttl = 64; ip->protocol = 1;
    uint8_t my_ip[4] = {10, 0, 2, 15};
    for(int i=0; i<4; i++) { ip->src_ip[i] = my_ip[i]; ip->dest_ip[i] = dest_ip[i]; }
    ip->checksum = 0; ip->checksum = net_checksum(ip, 20);
    icmp->type = 8; icmp->code = 0; icmp->id = htons(1); icmp->seq = htons(1);
    for(int i=0; i<32; i++) packet[42+i] = i;
    icmp->checksum = 0; icmp->checksum = net_checksum(icmp, 40);
    e1000_send_packet(packet, 74);
}

void net_send_dhcp() {
    uint8_t packet[300];
    for(int i=0; i<300; i++) packet[i] = 0;
    struct eth_hdr* eth = (struct eth_hdr*)packet;
    struct ipv4_hdr* ip = (struct ipv4_hdr*)(packet + 14);
    struct udp_hdr* udp = (struct udp_hdr*)(packet + 34);
    uint8_t* my_mac = get_mac_address();
    for(int i=0; i<6; i++) { eth->dest[i] = 0xFF; eth->src[i] = my_mac[i]; }
    eth->type = htons(0x0800);
    ip->ihl_version = 0x45; ip->tos = 0; ip->total_length = htons(286); ip->id = htons(2); ip->frag_offset = 0; ip->ttl = 64; ip->protocol = 17;
    for(int i=0; i<4; i++) { ip->src_ip[i] = 0; ip->dest_ip[i] = 0xFF; }
    ip->checksum = 0; ip->checksum = net_checksum(ip, 20);
    udp->src_port = htons(68); udp->dest_port = htons(67); udp->length = htons(266); udp->checksum = 0;
    uint8_t* dhcp = packet + 42;
    dhcp[0] = 1; dhcp[1] = 1; dhcp[2] = 6; dhcp[3] = 0;
    dhcp[4] = 0x39; dhcp[5] = 0x03; dhcp[6] = 0xF3; dhcp[7] = 0x26;
    for(int i=0; i<6; i++) dhcp[28+i] = my_mac[i];
    dhcp[236] = 99; dhcp[237] = 130; dhcp[238] = 83; dhcp[239] = 99;
    dhcp[240] = 53; dhcp[241] = 1; dhcp[242] = 1; dhcp[243] = 255;
    e1000_send_packet(packet, 300);
}

void net_send_dns(const char* domain) {
    uint8_t packet[200];
    for(int i=0; i<200; i++) packet[i] = 0;
    struct eth_hdr* eth = (struct eth_hdr*)packet;
    struct ipv4_hdr* ip = (struct ipv4_hdr*)(packet + 14);
    struct udp_hdr* udp = (struct udp_hdr*)(packet + 34);
    uint8_t* my_mac = get_mac_address();
    uint8_t router_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
    for(int i=0; i<6; i++) { eth->dest[i] = router_mac[i]; eth->src[i] = my_mac[i]; }
    eth->type = htons(0x0800);
    int dlen = 0; while(domain[dlen]) dlen++;
    int qlen = dlen + 2;
    ip->ihl_version = 0x45; ip->tos = 0; ip->total_length = htons(28 + 16 + qlen + 4); ip->id = htons(3); ip->frag_offset = 0; ip->ttl = 64; ip->protocol = 17;
    uint8_t my_ip[4] = {10, 0, 2, 15}; uint8_t dns_ip[4] = {10, 0, 2, 3};
    for(int i=0; i<4; i++) { ip->src_ip[i] = my_ip[i]; ip->dest_ip[i] = dns_ip[i]; }
    ip->checksum = 0; ip->checksum = net_checksum(ip, 20);
    udp->src_port = htons(50000); udp->dest_port = htons(53); udp->length = htons(8 + 16 + qlen + 4); udp->checksum = 0;
    uint8_t* dns = packet + 42;
    dns[0] = 0x12; dns[1] = 0x34; dns[2] = 0x01; dns[3] = 0x00; dns[4] = 0x00; dns[5] = 0x01;
    int p = 12; int i = 0;
    while (i < dlen) {
        int j = i; while(j < dlen && domain[j] != '.') j++;
        dns[p++] = j - i;
        while(i < j) { dns[p++] = domain[i++]; }
        if (domain[i] == '.') i++;
    }
    dns[p++] = 0; dns[p++] = 0x00; dns[p++] = 0x01; dns[p++] = 0x00; dns[p++] = 0x01;
    e1000_send_packet(packet, 42 + p);
}

void net_send_tcp(uint8_t* dest_ip, uint16_t src_port, uint16_t dst_port, uint32_t seq, uint32_t ack, uint8_t flags, const char* payload, uint16_t payload_len) {
    uint8_t packet[2048];
    struct eth_hdr* eth = (struct eth_hdr*)packet;
    struct ipv4_hdr* ip = (struct ipv4_hdr*)(packet + 14);
    uint8_t* tcp = packet + 34;
    uint8_t* my_mac = get_mac_address();
    uint8_t router_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
    
    for(int i=0; i<6; i++) { eth->dest[i] = router_mac[i]; eth->src[i] = my_mac[i]; }
    eth->type = htons(0x0800);

    ip->ihl_version = 0x45; ip->tos = 0; ip->total_length = htons(40 + payload_len); ip->id = htons(5); ip->frag_offset = 0; ip->ttl = 64; ip->protocol = 6; 
    uint8_t my_ip[4] = {10, 0, 2, 15};
    for(int i=0; i<4; i++) { ip->src_ip[i] = my_ip[i]; ip->dest_ip[i] = dest_ip[i]; }
    ip->checksum = 0; ip->checksum = net_checksum(ip, 20);

    tcp[0] = src_port >> 8; tcp[1] = src_port & 0xFF;
    tcp[2] = dst_port >> 8; tcp[3] = dst_port & 0xFF;
    tcp[4] = (seq >> 24) & 0xFF; tcp[5] = (seq >> 16) & 0xFF; tcp[6] = (seq >> 8) & 0xFF; tcp[7] = seq & 0xFF;
    tcp[8] = (ack >> 24) & 0xFF; tcp[9] = (ack >> 16) & 0xFF; tcp[10] = (ack >> 8) & 0xFF; tcp[11] = ack & 0xFF;
    tcp[12] = 0x50; tcp[13] = flags;
    tcp[14] = 0xFA; tcp[15] = 0xF0; 
    tcp[16] = 0; tcp[17] = 0; tcp[18] = 0; tcp[19] = 0;

    for(int i=0; i<payload_len; i++) tcp[20 + i] = payload[i];

    uint8_t pseudo[2048];
    for(int i=0; i<4; i++) { pseudo[i] = my_ip[i]; pseudo[4+i] = dest_ip[i]; }
    pseudo[8] = 0; pseudo[9] = 6; pseudo[10] = (20 + payload_len) >> 8; pseudo[11] = (20 + payload_len) & 0xFF;
    for(int i=0; i<20+payload_len; i++) pseudo[12+i] = tcp[i];
    
    uint16_t csum = net_checksum(pseudo, 12 + 20 + payload_len);
    tcp[16] = csum >> 8; tcp[17] = csum & 0xFF;

    e1000_send_packet(packet, 54 + payload_len);
}

int tcp_connect(uint8_t* ip, uint16_t port) {
    int s = -1;
    for(int i=0; i<16; i++) { if(tcp_sockets[i].state == 0) { s = i; break; } }
    if(s == -1) return -1;

    tcp_sockets[s].local_port = ephem_port++;
    tcp_sockets[s].remote_port = port;
    for(int i=0; i<4; i++) tcp_sockets[s].remote_ip[i] = ip[i];
    tcp_sockets[s].seq_num = 0x11223344 + s;
    tcp_sockets[s].ack_num = 0;
    tcp_sockets[s].state = 1;
    tcp_sockets[s].rx_len = 0;

    net_send_tcp(ip, tcp_sockets[s].local_port, port, tcp_sockets[s].seq_num, 0, 0x02, 0, 0);
    return s;
}

void tcp_send(int s, const char* data, int len) {
    if(s < 0 || s >= 16 || tcp_sockets[s].state != 2) return;
    net_send_tcp(tcp_sockets[s].remote_ip, tcp_sockets[s].local_port, tcp_sockets[s].remote_port, tcp_sockets[s].seq_num, tcp_sockets[s].ack_num, 0x18, data, len);
    tcp_sockets[s].seq_num += len;
}

int tcp_recv(int s, char* buf, int max_len) {
    if(s < 0 || s >= 16 || tcp_sockets[s].rx_len == 0) return 0;
    int copy_len = tcp_sockets[s].rx_len > max_len ? max_len : tcp_sockets[s].rx_len;
    for(int i=0; i<copy_len; i++) buf[i] = tcp_sockets[s].rx_buf[i];
    for(int i=0; i<tcp_sockets[s].rx_len - copy_len; i++) tcp_sockets[s].rx_buf[i] = tcp_sockets[s].rx_buf[i + copy_len];
    tcp_sockets[s].rx_len -= copy_len;
    return copy_len;
}

void net_poll() {
    uint8_t pkt[2048];
    int len = e1000_receive_packet(pkt);
    if(len <= 0) return;
    
    if(pkt[12] == 0x08 && pkt[13] == 0x00 && pkt[23] == 0x06) {
        uint8_t ihl = (pkt[14] & 0x0F) * 4;
        int tcp_start = 14 + ihl;
        uint16_t src_port = (pkt[tcp_start] << 8) | pkt[tcp_start + 1];
        uint16_t dst_port = (pkt[tcp_start + 2] << 8) | pkt[tcp_start + 3];
        uint32_t seq = (pkt[tcp_start+4]<<24) | (pkt[tcp_start+5]<<16) | (pkt[tcp_start+6]<<8) | pkt[tcp_start+7];
        uint8_t hdr_len = ((pkt[tcp_start+12] >> 4) & 0x0F) * 4;
        uint8_t flags = pkt[tcp_start+13];
        
        int payload_len = len - tcp_start - hdr_len;
        uint8_t* payload = &pkt[tcp_start + hdr_len];

        for(int i=0; i<16; i++) {
            if(tcp_sockets[i].state != 0 && tcp_sockets[i].local_port == dst_port && tcp_sockets[i].remote_port == src_port) {
                if(tcp_sockets[i].state == 1 && (flags & 0x12)) {
                    tcp_sockets[i].ack_num = seq + 1;
                    tcp_sockets[i].seq_num++;
                    tcp_sockets[i].state = 2;
                    net_send_tcp(tcp_sockets[i].remote_ip, dst_port, src_port, tcp_sockets[i].seq_num, tcp_sockets[i].ack_num, 0x10, 0, 0);
                } else if(tcp_sockets[i].state == 2 && payload_len > 0) {
                    tcp_sockets[i].ack_num = seq + payload_len;
                    for(int j=0; j<payload_len; j++) {
                        if(tcp_sockets[i].rx_len < 8192) tcp_sockets[i].rx_buf[tcp_sockets[i].rx_len++] = payload[j];
                    }
                    net_send_tcp(tcp_sockets[i].remote_ip, dst_port, src_port, tcp_sockets[i].seq_num, tcp_sockets[i].ack_num, 0x10, 0, 0);
                } else if(tcp_sockets[i].state == 2 && (flags & 0x01)) {
                    tcp_sockets[i].ack_num = seq + 1;
                    net_send_tcp(tcp_sockets[i].remote_ip, dst_port, src_port, tcp_sockets[i].seq_num, tcp_sockets[i].ack_num, 0x11, 0, 0);
                    tcp_sockets[i].state = 0;
                }
                break;
            }
        }
    }
}