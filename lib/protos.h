#ifndef PROTOS_H
#define PROTOS_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t Red;
    uint32_t Green;
    uint32_t Blue;
    uint32_t Yellow;
    uint32_t Purple;
    uint32_t Dark;
    uint32_t Surface;
    uint32_t Overlay;
    uint32_t Text;
} ColorPalette;

extern ColorPalette Color;

typedef struct UIButton UIButton;
struct UIButton {
    int x, y, w, h;
    char label[32];
    uint32_t bg_color;
    uint32_t text_color;
    void (*Draw)(UIButton* self);
    int (*IsClicked)(UIButton* self, int mx, int my);
};

typedef struct UIWindow UIWindow;
struct UIWindow {
    int id, x, y, w, h;
    char title[64];
    uint32_t bg_color;
    uint32_t* surface;
    void (*Draw)(UIWindow* self, int is_focused, int is_dragging);
    int (*CloseClicked)(UIWindow* self, int mx, int my);
};

UIButton Button_Create(int x, int y, int w, int h, const char* label, uint32_t bg, uint32_t fg);
UIWindow* Window_Create(int id, int x, int y, int w, int h, const char* title, uint32_t bg);
int str_len(const char* s);

void draw_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void draw_rect_outline(int x, int y, int w, int h, uint32_t color, int thickness);
void draw_circle_alpha(int x, int y, int r, uint32_t color, uint8_t alpha);
void draw_string(int x, int y, const char* str, uint32_t color);
void update_screen();
void clear_backbuffer(uint32_t color);
int get_mouse_x();
int get_mouse_y();
void* malloc(size_t size);
void free(void* ptr);
int get_mouse_btn();
char get_key();
size_t get_used_memory();
int get_num_tasks();
void kill(int pid);
int send_msg(int to_pid, int type, int d1, int d2);
int recv_msg(int* type, int* d1, int* d2);
int exec(const char* name);
void get_time(int* h, int* m, int* s);
void yield();
void strcpy(char* dest, const char* src);
void strcat(char* dest, const char* src);
int strcmp(const char* s1, const char* s2);
void itoa(int num, char* str);
int atoi(const char* str);
void draw_buffer(int x, int y, int w, int h, uint32_t* buffer);
void draw_string_to_buffer(uint32_t* buf, int buf_w, int x, int y, const char* str, uint32_t color);
void draw_rect_to_buffer(uint32_t* buf, int buf_w, int x, int y, int w, int h, uint32_t color);
int pci_get_device(int index, uint16_t* vendor, uint16_t* device, uint8_t* class_c);
const char* pci_class_name(uint8_t class_code);
void itoa_hex(uint32_t num, char* str);
void get_mac(uint8_t* buf);
void send_packet(void* data, int len);
int recv_packet(void* buf);
void send_arp(uint8_t* ip);
void ping_ip(uint8_t* ip, uint8_t* mac);
void request_dhcp();
void send_dns(const char* domain);
void send_tcp(uint8_t* ip, uint16_t sport, uint16_t dport, uint32_t seq, uint32_t ack, uint8_t flags, const char* data, int dlen);
void get_disk_label(char* buf);
void get_disk_files(char* buf);
void cd(const char* path);
void* create_shared_buffer(int id, size_t size);
void* get_shared_buffer(int id);
int tcp_connect(uint8_t* ip, uint16_t port);
void tcp_send(int s, const char* data, int len);
int tcp_recv(int s, char* buf, int max_len);

#endif