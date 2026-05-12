#ifndef E1000_H
#define E1000_H
#include <stdint.h>

// RX/TX buffer sizes
#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 8
#define E1000_BUF_SIZE 8192

void init_e1000();
uint8_t* get_mac_address();
void e1000_rx_init();
void e1000_tx_init();
void e1000_enable();
void e1000_send_packet(void* data, uint16_t len);
int e1000_receive_packet(void* buf);
int e1000_has_packet();
void e1000_poll();

#endif