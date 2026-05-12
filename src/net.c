#include "../include/net.h"
#include "../include/e1000.h"
#include "../include/kernel.h"
#include "../include/graphics.h"

// ---- Packet structures ----
struct eth_hdr { uint8_t dest[6]; uint8_t src[6]; uint16_t type; } __attribute__((packed));
struct arp_hdr { uint16_t hw_type; uint16_t proto_type; uint8_t hw_len; uint8_t proto_len; uint16_t opcode; uint8_t sender_mac[6]; uint8_t sender_ip[4]; uint8_t target_mac[6]; uint8_t target_ip[4]; } __attribute__((packed));
struct ipv4_hdr { uint8_t ihl_version; uint8_t tos; uint16_t total_length; uint16_t id; uint16_t frag_offset; uint8_t ttl; uint8_t protocol; uint16_t checksum; uint8_t src_ip[4]; uint8_t dest_ip[4]; } __attribute__((packed));
struct icmp_hdr { uint8_t type; uint8_t code; uint16_t checksum; uint16_t id; uint16_t seq; } __attribute__((packed));
struct udp_hdr { uint16_t src_port; uint16_t dest_port; uint16_t length; uint16_t checksum; } __attribute__((packed));
struct dns_hdr { uint16_t id; uint16_t flags; uint16_t qdcount; uint16_t ancount; uint16_t nscount; uint16_t arcount; } __attribute__((packed));

// ---- Global state ----
uint8_t my_ip[4] = {10, 0, 2, 15};
uint8_t gateway_ip[4] = {10, 0, 2, 2};
uint8_t dns_ip[4] = {10, 0, 2, 3};
uint8_t gateway_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
int gateway_mac_valid = 1; // QEMU default

arp_entry_t arp_cache[16];
int arp_cache_count = 0;

// ---- TCP state ----
#define TCP_CLOSED       0
#define TCP_SYN_SENT     1
#define TCP_ESTABLISHED  2
#define TCP_FIN_WAIT_1   3
#define TCP_FIN_WAIT_2   4
#define TCP_CLOSE_WAIT   5
#define TCP_LAST_ACK     6
#define TCP_TIME_WAIT    7

typedef struct {
    uint8_t remote_ip[4];
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t seq_num;
    uint32_t ack_num;
    int state;
    uint8_t rx_buf[8192];
    int rx_len;
    int retries;
    int timeout_ticks;
} tcp_socket_t;

tcp_socket_t tcp_sockets[16];
uint16_t ephem_port = 49152;

// ---- Helper functions ----
uint16_t htons(uint16_t v) { return (v >> 8) | (v << 8); }
uint32_t htonl(uint32_t v) { return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | ((v >> 24) & 0xFF); }

uint16_t net_checksum(void* data, int len) {
    uint16_t* ptr = (uint16_t*)data; uint32_t sum = 0;
    while(len > 1) { sum += *ptr++; len -= 2; }
    if(len > 0) sum += *((uint8_t*)ptr);
    while(sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static int ip_cmp(uint8_t* a, uint8_t* b) {
    for(int i=0; i<4; i++) if(a[i] != b[i]) return 0;
    return 1;
}

static int mac_is_zero(uint8_t* m) {
    for(int i=0; i<6; i++) if(m[i] != 0) return 0;
    return 1;
}

// ---- ARP Cache ----
void arp_cache_add(uint8_t* ip, uint8_t* mac) {
    // Check if already exists
    for(int i=0; i<arp_cache_count; i++) {
        if(ip_cmp(arp_cache[i].ip, ip)) {
            for(int j=0; j<6; j++) arp_cache[i].mac[j] = mac[j];
            arp_cache[i].valid = 1;
            return;
        }
    }
    if(arp_cache_count < 16) {
        for(int j=0; j<4; j++) arp_cache[arp_cache_count].ip[j] = ip[j];
        for(int j=0; j<6; j++) arp_cache[arp_cache_count].mac[j] = mac[j];
        arp_cache[arp_cache_count].valid = 1;
        arp_cache_count++;
    }
}

int arp_resolve(uint8_t* ip, uint8_t* out_mac) {
    // Check cache first
    for(int i=0; i<arp_cache_count; i++) {
        if(ip_cmp(arp_cache[i].ip, ip) && arp_cache[i].valid) {
            for(int j=0; j<6; j++) out_mac[j] = arp_cache[i].mac[j];
            return 1;
        }
    }
    // Check if it's gateway
    if(ip_cmp(ip, gateway_ip) && gateway_mac_valid) {
        for(int j=0; j<6; j++) out_mac[j] = gateway_mac[j];
        return 1;
    }
    return 0;
}

// ---- net_init ----
void net_init() {
    // Initialize ARP cache with gateway
    arp_cache_add(gateway_ip, gateway_mac);
}

// ---- Packet sending functions ----
void net_send_arp(uint8_t* target_ip) {
    uint8_t packet[42];
    struct eth_hdr* eth = (struct eth_hdr*)packet;
    struct arp_hdr* arp = (struct arp_hdr*)(packet + 14);
    uint8_t* my_mac = get_mac_address();
    for(int i=0; i<6; i++) { eth->dest[i] = 0xFF; eth->src[i] = my_mac[i]; arp->sender_mac[i] = my_mac[i]; arp->target_mac[i] = 0x00; }
    eth->type = htons(0x0806);
    arp->hw_type = htons(1); arp->proto_type = htons(0x0800); arp->hw_len = 6; arp->proto_len = 4; arp->opcode = htons(1);
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
    for(int i=0; i<4; i++) { ip->src_ip[i] = my_ip[i]; ip->dest_ip[i] = dest_ip[i]; }
    ip->checksum = 0; ip->checksum = net_checksum(ip, 20);
    icmp->type = 8; icmp->code = 0; icmp->id = htons(1); icmp->seq = htons(1);
    for(int i=0; i<32; i++) packet[42+i] = i;
    icmp->checksum = 0; icmp->checksum = net_checksum(icmp, 40);
    e1000_send_packet(packet, 74);
}

void dhcp_request() {
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
    uint8_t dest_mac[6];
    if(!arp_resolve(dns_ip, dest_mac)) {
        // Can't resolve DNS server MAC, use broadcast
        for(int i=0; i<6; i++) dest_mac[i] = 0xFF;
    }
    for(int i=0; i<6; i++) { eth->dest[i] = dest_mac[i]; eth->src[i] = my_mac[i]; }
    eth->type = htons(0x0800);
    int dlen = 0; while(domain[dlen]) dlen++;
    int qlen = dlen + 2;
    ip->ihl_version = 0x45; ip->tos = 0; ip->total_length = htons(28 + 16 + qlen + 4); ip->id = htons(3); ip->frag_offset = 0; ip->ttl = 64; ip->protocol = 17;
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

// ---- DNS response parser ----
// Returns 1 if resolved, fills resolved_ip
int dns_parse_response(uint8_t* pkt, int len, uint8_t* resolved_ip) {
    if (len < 42) return 0;
    int p = 42; // start of UDP payload
    
    struct dns_hdr* dns = (struct dns_hdr*)(pkt + p);
    uint16_t ancount = htons(dns->ancount);
    if (ancount == 0) return 0;
    
    p += 12; // skip DNS header
    
    // Skip the question section (we know the domain was there)
    while (p < len && pkt[p] != 0) { p += pkt[p] + 1; }
    p += 5; // skip null terminator + QTYPE + QCLASS
    
    // Parse answer section
    for (int a = 0; a < ancount && a < 1; a++) {
        // Skip name (either pointer or sequence)
        if ((pkt[p] & 0xC0) == 0xC0) {
            p += 2; // pointer
        } else {
            while (p < len && pkt[p] != 0) { p += pkt[p] + 1; }
            p++; // null terminator
        }
        
        uint16_t type = htons(*(uint16_t*)(pkt + p));
        p += 8; // skip type, class, ttl
        uint16_t rdlength = htons(*(uint16_t*)(pkt + p));
        p += 2;
        
        if (type == 1 && rdlength == 4) { // A record
            for(int i=0; i<4; i++) resolved_ip[i] = pkt[p + i];
            return 1;
        }
        p += rdlength;
    }
    return 0;
}

// ---- TCP stack ----
void net_send_tcp(uint8_t* dest_ip, uint16_t src_port, uint16_t dst_port, uint32_t seq, uint32_t ack, uint8_t flags, const char* payload, uint16_t payload_len) {
    uint8_t packet[2048];
    struct eth_hdr* eth = (struct eth_hdr*)packet;
    struct ipv4_hdr* ip = (struct ipv4_hdr*)(packet + 14);
    uint8_t* tcp = packet + 34;
    uint8_t* my_mac = get_mac_address();
    uint8_t dest_mac[6];
    
    if(!arp_resolve(dest_ip, dest_mac)) {
        // Can't resolve, try gateway if dest is not on same subnet
        // For now, use all-zeros and it will fail
        for(int i=0; i<6; i++) dest_mac[i] = 0x00;
        return; // can't send without MAC
    }
    
    for(int i=0; i<6; i++) { eth->dest[i] = dest_mac[i]; eth->src[i] = my_mac[i]; }
    eth->type = htons(0x0800);

    ip->ihl_version = 0x45; ip->tos = 0; ip->total_length = htons(40 + payload_len); ip->id = htons(5); ip->frag_offset = 0; ip->ttl = 64; ip->protocol = 6; 
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

    // TCP pseudo-header checksum
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
    for(int i=0; i<16; i++) { if(tcp_sockets[i].state == TCP_CLOSED) { s = i; break; } }
    if(s == -1) return -1;

    tcp_sockets[s].local_port = ephem_port++;
    tcp_sockets[s].remote_port = port;
    for(int i=0; i<4; i++) tcp_sockets[s].remote_ip[i] = ip[i];
    tcp_sockets[s].seq_num = 0x11223344 + (s * 0x10000);
    tcp_sockets[s].ack_num = 0;
    tcp_sockets[s].state = TCP_SYN_SENT;
    tcp_sockets[s].rx_len = 0;
    tcp_sockets[s].retries = 0;
    tcp_sockets[s].timeout_ticks = 0;

    // Send SYN
    net_send_tcp(ip, tcp_sockets[s].local_port, port, tcp_sockets[s].seq_num, 0, 0x02, 0, 0);
    return s;
}

void tcp_send(int s, const char* data, int len) {
    if(s < 0 || s >= 16 || tcp_sockets[s].state != TCP_ESTABLISHED) return;
    net_send_tcp(tcp_sockets[s].remote_ip, tcp_sockets[s].local_port, tcp_sockets[s].remote_port, 
                 tcp_sockets[s].seq_num, tcp_sockets[s].ack_num, 0x18, data, len);
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

int tcp_state(int s) {
    if(s < 0 || s >= 16) return -1;
    return tcp_sockets[s].state;
}

// ---- net_poll_single: process a single incoming packet, returns 1 if processed ----
static int net_poll_single() {
    uint8_t pkt[2048];
    int len = e1000_receive_packet(pkt);
    if(len <= 0) return 0;
    
    uint16_t eth_type = (pkt[12] << 8) | pkt[13];
    
    // === ARP packet ===
    if (eth_type == 0x0806) {
        struct arp_hdr* arp = (struct arp_hdr*)(pkt + 14);
        if (htons(arp->opcode) == 2) {
            arp_cache_add(arp->sender_ip, arp->sender_mac);
        } else if (htons(arp->opcode) == 1) {
            uint8_t* my_mac = get_mac_address();
            if (ip_cmp(arp->target_ip, my_ip)) {
                uint8_t reply[42];
                struct eth_hdr* reply_eth = (struct eth_hdr*)reply;
                struct arp_hdr* reply_arp = (struct arp_hdr*)(reply + 14);
                for(int i=0; i<6; i++) { reply_eth->dest[i] = arp->sender_mac[i]; reply_eth->src[i] = my_mac[i]; }
                reply_eth->type = htons(0x0806);
                reply_arp->hw_type = htons(1); reply_arp->proto_type = htons(0x0800);
                reply_arp->hw_len = 6; reply_arp->proto_len = 4; reply_arp->opcode = htons(2);
                for(int i=0; i<6; i++) reply_arp->sender_mac[i] = my_mac[i];
                for(int i=0; i<4; i++) reply_arp->sender_ip[i] = my_ip[i];
                for(int i=0; i<6; i++) reply_arp->target_mac[i] = arp->sender_mac[i];
                for(int i=0; i<4; i++) reply_arp->target_ip[i] = arp->sender_ip[i];
                e1000_send_packet(reply, 42);
            }
        }
        return 1;
    }
    
    // === IP packet ===
    if (eth_type != 0x0800 || len < 34) return 1;
    
    uint8_t ip_proto = pkt[23];
    uint8_t ihl = (pkt[14] & 0x0F) * 4;
    
    // === ICMP (ping reply) ===
    if (ip_proto == 1) return 1;
    
    // === UDP ===
    if (ip_proto == 17) {
        int udp_start = 14 + ihl;
        struct udp_hdr* udp = (struct udp_hdr*)(pkt + udp_start);
        
        // DHCP response (port 67 -> 68)
        if (htons(udp->dest_port) == 68 && htons(udp->src_port) == 67) {
            uint8_t* dhcp_data = pkt + udp_start + 8;
            int dhcp_len = len - udp_start - 8;
            if (dhcp_len > 240) {
                if (dhcp_data[236] == 99 && dhcp_data[237] == 130 && dhcp_data[238] == 83 && dhcp_data[239] == 99) {
                    int opt = 240;
                    while (opt < dhcp_len && dhcp_data[opt] != 255) {
                        if (dhcp_data[opt] == 53 && dhcp_data[opt+1] == 1) {
                            if (dhcp_data[opt+2] == 2 || dhcp_data[opt+2] == 5) {
                                for(int i=0; i<4; i++) my_ip[i] = dhcp_data[16 + i];
                            }
                        }
                        opt += dhcp_data[opt+1] + 2;
                    }
                }
            }
            return 1;
        }
        return 1;
    }
    
    // === TCP ===
    if (ip_proto != 6) return 1;
    
    int tcp_start = 14 + ihl;
    uint16_t src_port = (pkt[tcp_start] << 8) | pkt[tcp_start + 1];
    uint16_t dst_port = (pkt[tcp_start + 2] << 8) | pkt[tcp_start + 3];
    uint32_t seq = (pkt[tcp_start+4]<<24) | (pkt[tcp_start+5]<<16) | (pkt[tcp_start+6]<<8) | pkt[tcp_start+7];
    uint32_t ack = (pkt[tcp_start+8]<<24) | (pkt[tcp_start+9]<<16) | (pkt[tcp_start+10]<<8) | pkt[tcp_start+11];
    uint8_t hdr_len = ((pkt[tcp_start+12] >> 4) & 0x0F) * 4;
    uint8_t flags = pkt[tcp_start+13];
    
    int is_rst = (flags & 0x04);
    int is_syn = (flags & 0x02);
    int is_ack = (flags & 0x10);
    int is_fin = (flags & 0x01);
    
    int payload_len = len - tcp_start - hdr_len;
    if (payload_len < 0) payload_len = 0;
    uint8_t* payload = &pkt[tcp_start + hdr_len];
    
    for(int i=0; i<16; i++) {
        if(tcp_sockets[i].state != TCP_CLOSED && 
           tcp_sockets[i].local_port == dst_port && 
           tcp_sockets[i].remote_port == src_port) {
            
            if (is_rst) {
                tcp_sockets[i].state = TCP_CLOSED;
                break;
            }
            
            switch(tcp_sockets[i].state) {
                case TCP_SYN_SENT:
                    if (is_syn && is_ack) {
                        tcp_sockets[i].ack_num = seq + 1;
                        tcp_sockets[i].seq_num = ack;
                        tcp_sockets[i].state = TCP_ESTABLISHED;
                        net_send_tcp(tcp_sockets[i].remote_ip, dst_port, src_port, 
                                     tcp_sockets[i].seq_num, tcp_sockets[i].ack_num, 0x10, 0, 0);
                    }
                    break;
                    
                case TCP_ESTABLISHED:
                    if (payload_len > 0) {
                        for(int j=0; j<payload_len; j++) {
                            if(tcp_sockets[i].rx_len < 8192) 
                                tcp_sockets[i].rx_buf[tcp_sockets[i].rx_len++] = payload[j];
                        }
                        tcp_sockets[i].ack_num = seq + payload_len;
                        net_send_tcp(tcp_sockets[i].remote_ip, dst_port, src_port, 
                                     tcp_sockets[i].seq_num, tcp_sockets[i].ack_num, 0x10, 0, 0);
                    } else if (is_fin) {
                        tcp_sockets[i].ack_num = seq + 1;
                        net_send_tcp(tcp_sockets[i].remote_ip, dst_port, src_port, 
                                     tcp_sockets[i].seq_num, tcp_sockets[i].ack_num, 0x11, 0, 0);
                        tcp_sockets[i].state = TCP_CLOSED;
                    }
                    break;
                    
                default:
                    if (is_fin || is_rst) tcp_sockets[i].state = TCP_CLOSED;
                    break;
            }
            break;
        }
    }
    return 1;
}

// ---- net_poll: drain all pending packets ----
void net_poll() {
    // Process all pending packets
    int processed = 0;
    while(net_poll_single()) {
        processed++;
        if(processed > 50) break; // Don't loop forever
    }
    
    // Handle TCP retransmission timeouts
    // Count how many times net_poll is called as a rough time measure
    static int tick_counter = 0;
    tick_counter++;
    
    for(int i=0; i<16; i++) {
        if(tcp_sockets[i].state == TCP_SYN_SENT) {
            tcp_sockets[i].timeout_ticks++;
            // Retry every ~50 calls (roughly half a second with yield())
            if(tcp_sockets[i].timeout_ticks > 50) {
                tcp_sockets[i].timeout_ticks = 0;
                tcp_sockets[i].retries++;
                if(tcp_sockets[i].retries > 15) {
                    tcp_sockets[i].state = TCP_CLOSED;
                } else {
                    // Send SYN via gateway MAC directly (bypass ARP check)
                    uint8_t syn_pkt[54];
                    uint8_t* my_mac = get_mac_address();
                    for(int j=0; j<6; j++) { 
                        syn_pkt[j] = gateway_mac[j]; // dest MAC 
                        syn_pkt[6+j] = my_mac[j];    // src MAC
                    }
                    syn_pkt[12] = 0x08; syn_pkt[13] = 0x00; // ETH type IP
                    
                    struct ipv4_hdr* ip = (struct ipv4_hdr*)(syn_pkt + 14);
                    uint8_t* tcp = syn_pkt + 34;
                    
                    ip->ihl_version = 0x45; ip->tos = 0; 
                    ip->total_length = htons(40); 
                    ip->id = htons(5 + tcp_sockets[i].retries); 
                    ip->frag_offset = 0; ip->ttl = 64; ip->protocol = 6; 
                    for(int j=0; j<4; j++) { ip->src_ip[j] = my_ip[j]; ip->dest_ip[j] = tcp_sockets[i].remote_ip[j]; }
                    ip->checksum = 0; ip->checksum = net_checksum(ip, 20);
                    
                    uint16_t sp = tcp_sockets[i].local_port;
                    uint16_t dp = tcp_sockets[i].remote_port;
                    uint32_t seq = tcp_sockets[i].seq_num;
                    tcp[0] = sp >> 8; tcp[1] = sp & 0xFF;
                    tcp[2] = dp >> 8; tcp[3] = dp & 0xFF;
                    for(int j=0; j<4; j++) { tcp[4+j] = (seq >> (24-j*8)) & 0xFF; }
                    tcp[8] = 0; tcp[9] = 0; tcp[10] = 0; tcp[11] = 0;
                    tcp[12] = 0x50; tcp[13] = 0x02;
                    tcp[14] = 0xFA; tcp[15] = 0xF0; 
                    tcp[16] = 0; tcp[17] = 0; tcp[18] = 0; tcp[19] = 0;
                    
                    // TCP checksum with pseudo header
                    uint8_t pseudo[2048];
                    for(int j=0; j<4; j++) { pseudo[j] = my_ip[j]; pseudo[4+j] = tcp_sockets[i].remote_ip[j]; }
                    pseudo[8] = 0; pseudo[9] = 6; pseudo[10] = 0; pseudo[11] = 20;
                    for(int j=0; j<20; j++) pseudo[12+j] = tcp[j];
                    uint16_t csum = net_checksum(pseudo, 12 + 20);
                    tcp[16] = csum >> 8; tcp[17] = csum & 0xFF;
                    
                    e1000_send_packet(syn_pkt, 54);
                }
            }
        }
    }
}