#include "protos.h"

static inline uint64_t syscall(uint64_t sysno, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6) {
    uint64_t ret; register uint64_t r8 asm("r8") = p5; register uint64_t r9 asm("r9") = p6;
    __asm__ volatile ( "int $0x80" : "=a"(ret) : "a"(sysno), "D"(p1), "S"(p2), "d"(p3), "c"(p4), "r"(r8), "r"(r9) : "memory" );
    return ret;
}

void draw_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha) { syscall(1, x, y, w, h, color, alpha); }
void draw_rect_outline(int x, int y, int w, int h, uint32_t color, int thickness) { syscall(2, x, y, w, h, color, thickness); }
void draw_string(int x, int y, const char* str, uint32_t color) { syscall(3, x, y, (uint64_t)str, color, 0, 0); }
void update_screen() { syscall(4, 0, 0, 0, 0, 0, 0); }
void clear_backbuffer(uint32_t color) { syscall(5, color, 0, 0, 0, 0, 0); }
int get_mouse_x() { return (int)syscall(6, 0, 0, 0, 0, 0, 0); }
int get_mouse_y() { return (int)syscall(7, 0, 0, 0, 0, 0, 0); }
void* malloc(size_t size) { return (void*)syscall(8, size, 0, 0, 0, 0, 0); }
void free(void* ptr) { syscall(9, (uint64_t)ptr, 0, 0, 0, 0, 0); }
int get_mouse_btn() { return (int)syscall(10, 0, 0, 0, 0, 0, 0); }
char get_key() { return (char)syscall(11, 0, 0, 0, 0, 0, 0); }
size_t get_used_memory() { return (size_t)syscall(12, 0, 0, 0, 0, 0, 0); }
int get_num_tasks() { return (int)syscall(13, 0, 0, 0, 0, 0, 0); }
void kill(int pid) { syscall(14, pid, 0, 0, 0, 0, 0); }
int send_msg(int to_pid, int type, int d1, int d2) { return (int)syscall(15, to_pid, type, d1, d2, 0, 0); }
int recv_msg(int* type, int* d1, int* d2) { return (int)syscall(16, (uint64_t)type, (uint64_t)d1, (uint64_t)d2, 0, 0, 0); }
int exec(const char* name) { return (int)syscall(17, (uint64_t)name, 0, 0, 0, 0, 0); }
void get_time(int* h, int* m, int* s) { syscall(18, (uint64_t)h, (uint64_t)m, (uint64_t)s, 0, 0, 0); }
void yield() { syscall(19, 0, 0, 0, 0, 0, 0); }
void draw_circle_alpha(int x, int y, int r, uint32_t color, uint8_t alpha) { syscall(20, x, y, r, color, alpha, 0); }
void draw_buffer(int x, int y, int w, int h, uint32_t* buffer) { syscall(21, x, y, w, h, (uint64_t)buffer, 0); }
void draw_string_to_buffer(uint32_t* buf, int buf_w, int x, int y, const char* str, uint32_t color) { syscall(22, (uint64_t)buf, buf_w, x, y, (uint64_t)str, color); }
void draw_rect_to_buffer(uint32_t* buf, int buf_w, int x, int y, int w, int h, uint32_t color) {
    uint64_t wh = ((uint64_t)w << 32) | (uint32_t)h;
    syscall(23, (uint64_t)buf, buf_w, x, y, wh, color);
}
int pci_get_device(int index, uint16_t* vendor, uint16_t* device, uint8_t* class_c) { return (int)syscall(24, index, (uint64_t)vendor, (uint64_t)device, (uint64_t)class_c, 0, 0); }
void get_mac(uint8_t* buf) { syscall(25, (uint64_t)buf, 0, 0, 0, 0, 0); }
void send_packet(void* data, int len) { syscall(26, (uint64_t)data, len, 0, 0, 0, 0); }
int recv_packet(void* buf) { return (int)syscall(27, (uint64_t)buf, 0, 0, 0, 0, 0); }
void send_arp(uint8_t* ip) { syscall(28, (uint64_t)ip, 0, 0, 0, 0, 0); }
void ping_ip(uint8_t* ip, uint8_t* mac) { syscall(29, (uint64_t)ip, (uint64_t)mac, 0, 0, 0, 0); }
void request_dhcp() { syscall(30, 0, 0, 0, 0, 0, 0); }
void dhcp_request() { syscall(30, 0, 0, 0, 0, 0, 0); }
void send_dns(const char* domain) { syscall(31, (uint64_t)domain, 0, 0, 0, 0, 0); }
void send_tcp(uint8_t* ip, uint16_t sport, uint16_t dport, uint32_t seq, uint32_t ack, uint8_t flags, const char* data, int dlen) {
    uint64_t p1 = (uint64_t)ip;
    uint64_t p2 = ((uint64_t)sport << 16) | dport;
    uint64_t p3 = ((uint64_t)seq << 32) | ack;
    uint64_t p4 = ((uint64_t)flags << 32) | dlen;
    syscall(32, p1, p2, p3, p4, (uint64_t)data, 0);
}
void get_disk_label(char* buf) { syscall(33, (uint64_t)buf, 0, 0, 0, 0, 0); }
void get_disk_files(char* buf) { syscall(34, (uint64_t)buf, 0, 0, 0, 0, 0); }
void cd(const char* path) { syscall(35, (uint64_t)path, 0, 0, 0, 0, 0); }
void* create_shared_buffer(int id, size_t size) { return (void*)syscall(36, id, size, 0, 0, 0, 0); }
void* get_shared_buffer(int id) { return (void*)syscall(37, id, 0, 0, 0, 0, 0); }
int tcp_connect(uint8_t* ip, uint16_t port) { return (int)syscall(38, (uint64_t)ip, port, 0, 0, 0, 0); }
void tcp_send(int s, const char* data, int len) { syscall(39, s, (uint64_t)data, len, 0, 0, 0); }
int tcp_recv(int s, char* buf, int max_len) { return (int)syscall(40, s, (uint64_t)buf, max_len, 0, 0, 0); }
int tcp_state(int s) { return (int)syscall(41, s, 0, 0, 0, 0, 0); }

int disk_mkfile(const char* name) { return (int)syscall(42, (uint64_t)name, 0, 0, 0, 0, 0); }
int disk_mkdir(const char* name) { return (int)syscall(43, (uint64_t)name, 0, 0, 0, 0, 0); }
int disk_delete(const char* name) { return (int)syscall(44, (uint64_t)name, 0, 0, 0, 0, 0); }
int disk_writefile(const char* name, const uint8_t* data, uint32_t size) { return (int)syscall(45, (uint64_t)name, (uint64_t)data, size, 0, 0, 0); }
int disk_readfile(const char* name, uint8_t* buffer, uint32_t max_size) { return (int)syscall(46, (uint64_t)name, (uint64_t)buffer, max_size, 0, 0, 0); }
uint32_t disk_getfilesize(const char* name) { return (uint32_t)syscall(47, (uint64_t)name, 0, 0, 0, 0, 0); }
void disk_getcwd(char* out) { syscall(48, (uint64_t)out, 0, 0, 0, 0, 0); }


void sys_play_wav(const char* filename) { syscall(49, (uint64_t)filename, 0, 0, 0, 0, 0); }
void sys_stop_wav() { syscall(50, 0, 0, 0, 0, 0, 0); }

void strcpy(char* dest, const char* src) { while (*src) *dest++ = *src++; *dest = '\0'; }
void strcat(char* dest, const char* src) { while (*dest) dest++; while (*src) *dest++ = *src++; *dest = '\0'; }
int strcmp(const char* s1, const char* s2) { while (*s1 && (*s1 == *s2)) { s1++; s2++; } return *(const unsigned char*)s1 - *(const unsigned char*)s2; }
void itoa(int num, char* str) {
    int i = 0; if (num == 0) { str[i++] = '0'; str[i] = '\0'; return; }
    while (num != 0) { str[i++] = (num % 10) + '0'; num = num / 10; }
    str[i] = '\0'; int start = 0; int end = i - 1;
    while (start < end) { char temp = str[start]; str[start] = str[end]; str[end] = temp; start++; end--; }
}
int atoi(const char* str) { int res = 0; while (*str >= '0' && *str <= '9') { res = res * 10 + (*str - '0'); str++; } return res; }
int str_len(const char* s) { int l=0; while(s[l]) l++; return l; }
const char* pci_class_name(uint8_t class_code) {
    if (class_code == 0x01) return "Storage Ctrl";
    if (class_code == 0x02) return "Network Ctrl";
    if (class_code == 0x03) return "Display Ctrl";
    if (class_code == 0x04) return "Multimedia Ctrl";
    if (class_code == 0x05) return "Memory Ctrl";
    if (class_code == 0x06) return "Bridge Device";
    if (class_code == 0x0C) return "Serial Bus Ctrl";
    return "Unknown Device";
}
void itoa_hex(uint32_t num, char* str) {
    const char hex[] = "0123456789ABCDEF";
    int i = 0;
    if (num == 0) { str[i++] = '0'; str[i] = '\0'; return; }
    while (num > 0) { str[i++] = hex[num % 16]; num /= 16; }
    str[i] = '\0';
    int start = 0; int end = i - 1;
    while (start < end) { char temp = str[start]; str[start] = str[end]; str[end] = temp; start++; end--; }
}

ColorPalette Color = {
    .Red = 0xF38BA8,
    .Green = 0xA6E3A1,
    .Blue = 0x89B4FA,
    .Yellow = 0xF9E2AF,
    .Purple = 0xCBA6F7,
    .Dark = 0x11111B,
    .Surface = 0x1E1E2E,
    .Overlay = 0x181825,
    .Text = 0xCDD6F4
};

static void _Button_Draw(UIButton* self) {
    int r = self->h / 2;
    if (r > 10) r = 10;
    draw_rect_alpha(self->x + r, self->y, self->w - (r * 2), self->h, self->bg_color, 255);
    draw_rect_alpha(self->x, self->y + r, r, self->h - (r * 2), self->bg_color, 255);
    draw_rect_alpha(self->x + self->w - r, self->y + r, r, self->h - (r * 2), self->bg_color, 255);
    draw_circle_alpha(self->x + r, self->y + r, r, self->bg_color, 255); 
    draw_circle_alpha(self->x + self->w - r, self->y + r, r, self->bg_color, 255);
    draw_circle_alpha(self->x + r, self->y + self->h - r, r, self->bg_color, 255); 
    draw_circle_alpha(self->x + self->w - r, self->y + self->h - r, r, self->bg_color, 255);
    
    int t_len = str_len(self->label);
    draw_string(self->x + (self->w/2) - (t_len*4), self->y + (self->h/2) - 8, self->label, self->text_color);
}

static int _Button_IsClicked(UIButton* self, int mx, int my) {
    return (mx >= self->x && mx <= self->x + self->w && my >= self->y && my <= self->y + self->h);
}

UIButton Button_Create(int x, int y, int w, int h, const char* label, uint32_t bg, uint32_t fg) {
    UIButton btn;
    btn.x = x; btn.y = y; btn.w = w; btn.h = h;
    btn.bg_color = bg; btn.text_color = fg;
    strcpy(btn.label, label);
    btn.Draw = _Button_Draw;
    btn.IsClicked = _Button_IsClicked;
    return btn;
}

static void _Window_Draw(UIWindow* self, int is_focused, int is_dragging) {
    int x = self->x, y = self->y, width = self->w, height = self->h; 
    uint32_t bg = self->bg_color; 
    int r = 8;
    
    if (is_focused && !is_dragging) {
        draw_rect_alpha(x + 10, y + 10, width, height, 0x000000, 80);
    }
    
    uint32_t header = is_focused ? Color.Surface : Color.Dark;
    draw_rect_alpha(x + r, y, width - 2*r, 30, header, 255);
    draw_rect_alpha(x, y + r, r, 30 - r, header, 255); 
    draw_rect_alpha(x + width - r, y + r, r, 30 - r, header, 255);
    draw_circle_alpha(x + r, y + r, r, header, 255); 
    draw_circle_alpha(x + width - r, y + r, r, header, 255);
    
    draw_rect_alpha(x + r, y + 30, width - 2*r, height - 30, bg, 255);
    draw_rect_alpha(x, y + 30, r, height - 30 - r, bg, 255);
    draw_rect_alpha(x + width - r, y + 30, r, height - 30 - r, bg, 255);
    draw_circle_alpha(x + r, y + height - r, r, bg, 255); 
    draw_circle_alpha(x + width - r, y + height - r, r, bg, 255);
    
    draw_rect_alpha(x, y + 30, width, 1, is_focused ? 0x313244 : Color.Overlay, 255);
    
    draw_string(x + 10, y + 8, self->title, is_focused ? Color.Text : 0x6C7086);

    int btn_w = 30, btn_h = 20, btn_r = 4;
    int btn_x = x + width - btn_w - 5;
    int btn_y = y + 5;
    draw_rect_alpha(btn_x + btn_r, btn_y, btn_w - 2*btn_r, btn_h, Color.Red, 255);
    draw_rect_alpha(btn_x, btn_y + btn_r, btn_r, btn_h - 2*btn_r, Color.Red, 255);
    draw_rect_alpha(btn_x + btn_w - btn_r, btn_y + btn_r, btn_r, btn_h - 2*btn_r, Color.Red, 255);
    draw_circle_alpha(btn_x + btn_r, btn_y + btn_r, btn_r, Color.Red, 255);
    draw_circle_alpha(btn_x + btn_w - btn_r, btn_y + btn_r, btn_r, Color.Red, 255);
    draw_circle_alpha(btn_x + btn_r, btn_y + btn_h - btn_r, btn_r, Color.Red, 255);
    draw_circle_alpha(btn_x + btn_w - btn_r, btn_y + btn_h - btn_r, btn_r, Color.Red, 255);
    draw_string(btn_x + 11, btn_y + 3, "x", 0xFFFFFF);
}

static int _Window_CloseClicked(UIWindow* self, int mx, int my) {
    int btn_w = 30;
    int close_x = self->x + self->w - btn_w - 5;
    return (mx >= close_x && mx <= close_x + btn_w && my >= self->y + 5 && my <= self->y + 25);
}

UIWindow* Window_Create(int id, int x, int y, int w, int h, const char* title, uint32_t bg) {
    UIWindow* win = (UIWindow*)malloc(sizeof(UIWindow));
    win->id = id; win->x = x; win->y = y; win->w = w; win->h = h;
    strcpy(win->title, title); win->bg_color = bg; win->surface = 0;
    win->Draw = _Window_Draw;
    win->CloseClicked = _Window_CloseClicked;
    return win;
}