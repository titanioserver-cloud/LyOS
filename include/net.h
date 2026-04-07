#ifndef NET_H
#define NET_H
#include <stdint.h>

void net_send_arp(uint8_t* target_ip);
void net_send_icmp(uint8_t* dest_ip, uint8_t* dest_mac);
void net_send_dhcp();
void net_send_dns(const char* domain);
void net_send_tcp(uint8_t* dest_ip, uint16_t src_port, uint16_t dst_port, uint32_t seq, uint32_t ack, uint8_t flags, const char* payload, uint16_t payload_len);

int tcp_connect(uint8_t* ip, uint16_t port);
void tcp_send(int s, const char* data, int len);
int tcp_recv(int s, char* buf, int max_len);
void net_poll();

#endif