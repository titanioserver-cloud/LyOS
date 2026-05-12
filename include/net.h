#ifndef NET_H
#define NET_H
#include <stdint.h>

// IP e MAC globais (atualizados por DHCP/ARP)
extern uint8_t my_ip[4];
extern uint8_t gateway_ip[4];
extern uint8_t dns_ip[4];
extern uint8_t gateway_mac[6];
extern int gateway_mac_valid;

// ARP cache
typedef struct {
    uint8_t ip[4];
    uint8_t mac[6];
    int valid;
} arp_entry_t;
extern arp_entry_t arp_cache[16];
extern int arp_cache_count;

void net_send_arp(uint8_t* target_ip);
void net_send_icmp(uint8_t* dest_ip, uint8_t* dest_mac);
void net_send_dhcp();
void net_send_dns(const char* domain);
void net_send_tcp(uint8_t* dest_ip, uint16_t src_port, uint16_t dst_port, uint32_t seq, uint32_t ack, uint8_t flags, const char* payload, uint16_t payload_len);

int tcp_connect(uint8_t* ip, uint16_t port);
void tcp_send(int s, const char* data, int len);
int tcp_recv(int s, char* buf, int max_len);
int tcp_state(int s);
void net_poll();
void net_init();
int arp_resolve(uint8_t* ip, uint8_t* out_mac);
void dhcp_request();

#endif