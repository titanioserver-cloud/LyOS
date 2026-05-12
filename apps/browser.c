#include "../lib/protos.h"
#include "../include/html.h"
#include "../include/css.h"
#include "../include/layout.h"
#include "../include/render.h"

// ---- Browser state ----
char url[64] = "google.com";
int cursor_pos = 10;

int phase = 0;   // 0=idle, 1=arp/connect, 2=tcp_handshake, 3=http_request, 4=done
int timer = 0;
int sock = -1;
int headers_passed = 0;
int match_state = 0;
int sent_req = 0;

char html_buf[65536];
int html_len = 0;

DOMNode* dom = 0;
CSSRule* css = 0;
int css_count = 0;
int scroll_y = 0;

int win_x = 50, win_y = 50, win_w = 680, win_h = 500;

// Colors
#define COLOR_BG      0x11111B
#define COLOR_DARK    0x181825
#define COLOR_SURFACE 0x1E1E2E
#define COLOR_OVERLAY 0x313244
#define COLOR_TEXT    0xCDD6F4
#define COLOR_BLUE    0x89B4FA
#define COLOR_RED     0xF38BA8
#define COLOR_GREEN   0xA6E3A1
#define COLOR_YELLOW  0xF9E2AF

// UI state
int dragging = 0;
int drag_off_x = 0, drag_off_y = 0;
int focused = 0; // 0=not focused, 1=url bar
int last_mx = -1, last_my = -1, last_btn = 0;
int state_changed = 1;

void start_request() {
    phase = 1;
    timer = 0;
    sock = -1;
    headers_passed = 0;
    match_state = 0;
    sent_req = 0;
    html_len = 0;
    state_changed = 1;
}

int main() {
    while (1) {
        yield();
        
        int mx = get_mouse_x();
        int my = get_mouse_y();
        int btn = get_mouse_btn();
        char key = get_key();
        
        int mouse_moved = (mx != last_mx || my != last_my);
        if (btn != last_btn || key != 0) state_changed = 1;
        
        // Check for messages (calc sends back results, we ignore them)
        int m_type, m_d1, m_d2;
        int sender = recv_msg(&m_type, &m_d1, &m_d2);
        (void)sender; (void)m_type; (void)m_d1; (void)m_d2;
        
        // === Mouse click ===
        if (btn == 1 && last_btn == 0) {
            // Title bar - drag
            if (mx >= win_x && mx <= win_x + win_w && my >= win_y && my <= win_y + 25) {
                // Close button (X) in top right
                if (mx >= win_x + win_w - 30 && mx <= win_x + win_w - 10 && my >= win_y + 5 && my <= win_y + 20) {
                    return 0; // Exit browser
                }
                dragging = 1;
                drag_off_x = mx - win_x;
                drag_off_y = my - win_y;
            }
            // URL bar
            else if (mx >= win_x + 60 && mx <= win_x + win_w - 30 && my >= win_y + 50 && my <= win_y + 70) {
                focused = 1;
            }
            // Go button
            else if (mx >= win_x + win_w - 35 && mx <= win_x + win_w - 5 && my >= win_y + 48 && my <= win_y + 72) {
                if (url[0] != '\0') start_request();
            }
            // Quick links
            else if (mx >= win_x + 15 && mx <= win_x + 115 && my >= win_y + 85 && my <= win_y + 105) {
                strcpy(url, "google.com"); cursor_pos = 10; start_request();
            }
            else if (mx >= win_x + 125 && mx <= win_x + 225 && my >= win_y + 85 && my <= win_y + 105) {
                strcpy(url, "httpbin.org"); cursor_pos = 11; start_request();
            }
            // Scroll bar area
            else if (mx >= win_x + win_w - 20 && mx <= win_x + win_w - 5 && my >= win_y + 115 && my <= win_y + win_h - 20) {
                if (dom) {
                    int content_h = dom->computed_height;
                    int view_h = win_h - 130;
                    if (content_h > view_h) {
                        float ratio = (float)(my - (win_y + 115)) / view_h;
                        scroll_y = (int)(ratio * (content_h - view_h));
                        if (scroll_y < 0) scroll_y = 0;
                        state_changed = 1;
                    }
                }
            }
            // Content area (for scroll wheel would go here)
            else if (mx >= win_x + 10 && mx <= win_x + win_w - 25 && my >= win_y + 115 && my <= win_y + win_h - 20) {
                focused = 0;
            }
            else {
                focused = 0;
            }
        } else if (btn == 0) {
            dragging = 0;
        }
        
        // === Drag window ===
        if (dragging && btn == 1 && mouse_moved) {
            win_x = mx - drag_off_x;
            win_y = my - drag_off_y;
            if (win_x < 0) win_x = 0;
            if (win_y < 0) win_y = 0;
            state_changed = 1;
        }
        
        // === Keyboard input ===
        if (focused == 1 && key) {
            if (key == '\b' && cursor_pos > 0) {
                url[--cursor_pos] = '\0';
                state_changed = 1;
            } else if (key == '\n') {
                if (url[0] != '\0') start_request();
            } else if (key != '\b' && key != '\n' && cursor_pos < 60) {
                url[cursor_pos++] = key;
                url[cursor_pos] = '\0';
                state_changed = 1;
            }
        }
        
        // === Browser async state machine ===
        if (phase != 0) {
            timer++;
            
            if (phase == 1) {
                // TCP connect (which sends SYN)
                uint8_t proxy_ip[4] = {10, 0, 2, 2};
                sock = tcp_connect(proxy_ip, 8080);
                
                if (sock >= 0) {
                    phase = 2;
                    timer = 0;
                } else {
                    if (timer > 100) phase = 4; // no free sockets
                }
                state_changed = 1;
                
            } else if (phase == 2) {
                // Wait for TCP handshake complete
                int st = tcp_state(sock);
                
                if (st == 2) { // ESTABLISHED
                    phase = 3;
                    timer = 0;
                } else if (st == 0) { // CLOSED (RST received)
                    phase = 4;
                } else if (timer > 400) { // timeout ~4 seconds
                    phase = 4;
                }
                state_changed = 1;
                
            } else if (phase == 3) {
                // Send HTTP request and receive response
                char http_req[512];
                strcpy(http_req, "GET /"); 
                strcat(http_req, url); 
                strcat(http_req, " HTTP/1.1\r\nHost: 10.0.2.2\r\nConnection: close\r\n\r\n");
                
                int st = tcp_state(sock);
                if (st == 0) { // CLOSED
                    phase = 4;
                }
                
                // Send request periodically
                if (!sent_req) {
                    tcp_send(sock, http_req, str_len(http_req));
                    sent_req = 1;
                } else if (timer % 30 == 0 && !headers_passed && timer < 100) {
                    tcp_send(sock, http_req, str_len(http_req));
                }
                
                char buf[2048];
                int payload_len = tcp_recv(sock, buf, 2048);
                
                if (payload_len > 0) {
                    int k = 0;
                    if (!headers_passed) {
                        for (; k < payload_len; k++) {
                            char c = buf[k];
                            if (match_state == 0 && c == '\r') match_state = 1;
                            else if (match_state == 1 && c == '\n') match_state = 2;
                            else if (match_state == 2 && c == '\r') match_state = 3;
                            else if (match_state == 3 && c == '\n') { 
                                headers_passed = 1; k++; 
                                break; 
                            }
                            else if (c == '\r') match_state = 1;
                            else if (c == '\n') match_state = 0;
                            else match_state = 0;
                        }
                    }
                    
                    if (headers_passed) {
                        for (; k < payload_len; k++) {
                            if (html_len < 65535) {
                                html_buf[html_len++] = buf[k];
                            }
                        }
                    }
                }
                
                // Finish condition
                if (headers_passed && timer > 30 && payload_len == 0) {
                    phase = 4;
                } else if (timer > 150) { // max ~1.5 seconds waiting for response
                    phase = 4;
                }
                state_changed = 1;
                
            } else if (phase == 4) {
                // Parse and render
                html_buf[html_len] = '\0';
                
                if (dom) { free_dom(dom); dom = 0; }
                if (css) { free_css(css, css_count); css = 0; css_count = 0; }
                
                if (html_len > 0) {
                    dom = parse_html(html_buf, html_len);
                    css = parse_css("body { background-color: transparent; color: #CDD6F4; padding: 10px; } h1 { color: #89B4FA; font-size: 24px; margin: 10px; } p { margin: 8px; } a { color: #A6E3A1; }", &css_count);
                    apply_all_css(dom, css, css_count);
                }
                
                phase = 0;
                state_changed = 1;
            }
            
            last_mx = mx; last_my = my; last_btn = btn;
        }
        
        // === Drawing ===
        if (!state_changed && !mouse_moved) {
            last_mx = mx; last_my = my; last_btn = btn;
            continue;
        }
        
        // === Background (just clear area) ===
        draw_rect_alpha(win_x, win_y, win_w, win_h, COLOR_BG, 255);
        draw_rect_alpha(win_x + 1, win_y + win_h, win_w + 2, 2, COLOR_DARK, 255);
        draw_rect_alpha(win_x + win_w, win_y, 2, win_h, COLOR_DARK, 255);
        draw_rect_alpha(win_x - 1, win_y, 2, win_h, COLOR_DARK, 255);
        draw_rect_alpha(win_x, win_y - 1, win_w, 2, COLOR_DARK, 255);
        
        // === Title bar ===
        draw_rect_alpha(win_x + 5, win_y + 5, win_w - 10, 20, COLOR_OVERLAY, 255);
        draw_string(win_x + 15, win_y + 8, "PROTOS Browser", COLOR_TEXT);
        // Close button
        draw_rect_alpha(win_x + win_w - 30, win_y + 5, 20, 18, COLOR_RED, 255);
        draw_string(win_x + win_w - 26, win_y + 7, "X", COLOR_TEXT);
        
        // === URL bar ===
        draw_rect_alpha(win_x + 10, win_y + 50, win_w - 50, 22, COLOR_DARK, 255);
        draw_string(win_x + 15, win_y + 53, url, COLOR_TEXT);
        if (focused == 1) {
            draw_rect_alpha(win_x + 15 + (cursor_pos * 8), win_y + 53, 8, 15, COLOR_TEXT, 180);
        }
        
        // Go button
        draw_rect_alpha(win_x + win_w - 35, win_y + 48, 30, 24, COLOR_BLUE, 255);
        draw_string(win_x + win_w - 30, win_y + 52, "Ir", 0x11111B);
        
        // === Quick links ===
        draw_rect_alpha(win_x + 10, win_y + 80, win_w - 20, 28, COLOR_SURFACE, 255);
        draw_rect_alpha(win_x + 15, win_y + 85, 100, 18, COLOR_OVERLAY, 255);
        draw_string(win_x + 20, win_y + 87, "google.com", COLOR_GREEN);
        draw_rect_alpha(win_x + 125, win_y + 85, 100, 18, COLOR_OVERLAY, 255);
        draw_string(win_x + 130, win_y + 87, "httpbin.org", COLOR_GREEN);
        
        // === Content area ===
        draw_rect_alpha(win_x + 10, win_y + 115, win_w - 20, win_h - 130, COLOR_DARK, 255);
        
        if (phase == 0) {
            if (dom) {
                int b_x = win_x + 10;
                int b_y = win_y + 115 - scroll_y;
                int b_w = win_w - 20;
                int b_h = win_h - 130;
                
                calculate_layout(dom, b_x, b_y, b_w);
                render_dom(dom, b_x, win_y + 115, b_w, b_h);
                
                int total_h = dom->computed_height;
                if (total_h > b_h) {
                    int sb_x = win_x + win_w - 15;
                    int sb_y = win_y + 115;
                    int sb_h = b_h;
                    draw_rect_alpha(sb_x, sb_y, 6, sb_h, COLOR_OVERLAY, 255);
                    int thumb_h = (sb_h * b_h) / total_h;
                    if (thumb_h < 20) thumb_h = 20;
                    int thumb_y = sb_y + (scroll_y * (sb_h - thumb_h)) / (total_h - b_h);
                    draw_rect_alpha(sb_x, thumb_y, 6, thumb_h, 0x585B70, 255);
                }
            } else {
                draw_string(win_x + (win_w/2) - 60, win_y + 200, "PROTOS BROWSER", COLOR_YELLOW);
                draw_string(win_x + (win_w/2) - 80, win_y + 230, "Escreve um URL e clica Ir", 0x6C7086);
            }
        } else {
            if (html_len > 0 && headers_passed) {
                draw_string(win_x + 25, win_y + 130, "A receber dados...", COLOR_GREEN);
            } else if (phase == 1) {
                draw_string(win_x + 25, win_y + 130, "A conectar ao proxy (10.0.2.2:8080)...", 0xA6ADC8);
            } else if (phase == 2) {
                draw_string(win_x + 25, win_y + 130, "Handshake TCP...", 0xA6ADC8);
                if (timer > 100) {
                    draw_string(win_x + 25, win_y + 150, "(proxy ligado? Verifica com python3 proxy.py)", 0x6C7086);
                }
            } else if (phase == 3) {
                draw_string(win_x + 25, win_y + 130, "A enviar HTTP GET...", 0xA6ADC8);
            }
            
            // Draw mini scroll bar for progress
            int prog = (timer * (win_w - 40)) / 300;
            if (prog > win_w - 40) prog = win_w - 40;
            draw_rect_alpha(win_x + 20, win_y + win_h - 30, prog, 6, COLOR_BLUE, 255);
        }
        
        update_screen();
        state_changed = 0;
        
        last_mx = mx; last_my = my; last_btn = btn;
    }
    return 0;
}
