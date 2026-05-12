#include "../lib/protos.h"
#include "../include/icons.h"
#include "../include/background.h"

#define MAX_HIST 100
char term_hist[MAX_HIST][128];
int hist_cnt = 0;
int term_view_idx = 0;

uint8_t last_dns_ip[4] = {0,0,0,0};
char last_dns_domain[64] = "none";

int snake_x[100];
int snake_y[100];

void add_hist(const char* s) {
    if (hist_cnt < MAX_HIST) { strcpy(term_hist[hist_cnt++], s); } 
    else {
        for(int i=1; i<MAX_HIST; i++) strcpy(term_hist[i-1], term_hist[i]);
        strcpy(term_hist[MAX_HIST-1], s);
    }
    term_view_idx = hist_cnt > 15 ? hist_cnt - 15 : 0;
}

int find_substr(const char* str, const char* sub) { 
    for(int i=0; str[i]; i++) { int j=0; while(str[i+j] && sub[j] && str[i+j]==sub[j]) j++; if (!sub[j]) return i; } 
    return -1; 
}

void draw_image(int x, int y, const uint32_t* img) {
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 32; j++) {
            uint32_t pixel = img[i * 32 + j];
            uint8_t a = (pixel >> 24) & 0xFF;
            if (a > 0) draw_rect_alpha(x + j, y + i, 1, 1, pixel & 0xFFFFFF, a);
        }
    }
}

void format_bytes(size_t bytes, char* out) {
    char temp[32];
    out[0] = '\0';
    if (bytes < 1024) {
        itoa((int)bytes, temp); strcat(out, temp); strcat(out, " B");
    } else if (bytes < 1024 * 1024) {
        itoa((int)(bytes / 1024), temp); strcat(out, temp); strcat(out, " KB");
    } else {
        itoa((int)(bytes / (1024 * 1024)), temp); strcat(out, temp); strcat(out, ".");
        int dec = (int)((bytes % (1024 * 1024)) * 100 / (1024 * 1024));
        if (dec < 10) strcat(out, "0");
        itoa(dec, temp); strcat(out, temp); strcat(out, " MB");
    }
}

int main() {
    uint32_t* bg_buffer = (uint32_t*)malloc(1280 * 720 * sizeof(uint32_t));
    uint32_t base_color = 0x0C0C14;
    for (int i = 0; i < 1280 * 720; i++) {
        bg_buffer[i] = base_color;
    }

    int bg_x = (1280 - 1024) / 2;
    int bg_y = (720 - 1024) / 2;

    for (int i = 0; i < 1024; i++) {
        int screen_y = bg_y + i;
        if (screen_y < 0 || screen_y >= 720) continue;
        for (int j = 0; j < 1024; j++) {
            int screen_x = bg_x + j;
            if (screen_x < 0 || screen_x >= 1280) continue;

            uint32_t pixel = background_data[i * 1024 + j];
            uint8_t a = (pixel >> 24) & 0xFF;
            if (a == 0) continue;

            if (a == 255) {
                bg_buffer[screen_y * 1280 + screen_x] = pixel & 0xFFFFFF;
            } else {
                uint32_t fg_rb = pixel & 0xFF00FF;
                uint32_t fg_g = pixel & 0x00FF00;
                uint32_t bg_c = bg_buffer[screen_y * 1280 + screen_x];
                uint32_t bg_rb = bg_c & 0xFF00FF;
                uint32_t bg_g = bg_c & 0x00FF00;

                uint32_t res_rb = (bg_rb + (((fg_rb - bg_rb) * a) >> 8)) & 0xFF00FF;
                uint32_t res_g  = (bg_g  + (((fg_g  - bg_g) * a) >> 8)) & 0x00FF00;
                
                bg_buffer[screen_y * 1280 + screen_x] = res_rb | res_g;
            }
        }
    }

    UIWindow* windows[10]; int num_windows = 0; int next_pid = 1;
    int calc_pid = -1; int calc_win_id = -1; int calc_display_value = 0;
    int snake_win_id = -1; int snake_pid = -1; uint32_t* snake_surface = 0;

    int explorer_pid = -1;
    int explorer_win_id = -1;
    uint32_t* explorer_surface = 0;

    int player_pid = -1;
    int player_win_id = -1;
    uint32_t* player_surface = 0;

    int focused_input = 1;

    int dragging = 0; UIWindow* drag_win = 0; int drag_offset_x = 0; int drag_offset_y = 0;
    char term_buffer[256]; int term_cursor = 0; term_buffer[0] = '\0';
    int last_btn_state = 0, last_mx = -1, last_my = -1, last_s = -1;
    int state_changed = 1;
    int show_launcher = 0;
    int sysmon_cpu_current = 2;

    add_hist("Terminal Iniciado.");
    add_hist("Escreva 'task', 'ls', 'browser', 'calc', 'snake', 'player'.");

    while (1) {
        yield(); 
        int m_type, m_d1, m_d2;
        int sender = recv_msg(&m_type, &m_d1, &m_d2);
        if(sender > 0) { if(sender == calc_pid && m_type == 2) calc_display_value = m_d1; state_changed = 1; }

        int mx = get_mouse_x(); int my = get_mouse_y();
        int btn = get_mouse_btn(); char key = get_key();
        int h, m, s; get_time(&h, &m, &s);
        
        int mouse_moved = (mx != last_mx || my != last_my);
        if (btn != last_btn_state || key != 0 || s != last_s) { state_changed = 1; }

        int cpu_target = 2;
        if (state_changed && (key != 0 || btn != last_btn_state)) { cpu_target = 25 + (s % 15); } 
        else if (mouse_moved) { cpu_target = 6 + (s % 5); } 
        else { cpu_target = 1 + (s % 3); }

        if (sysmon_cpu_current < cpu_target) sysmon_cpu_current += 3;
        else if (sysmon_cpu_current > cpu_target) sysmon_cpu_current -= 2;
        if (sysmon_cpu_current < 1) sysmon_cpu_current = 1;
        if (sysmon_cpu_current > 99) sysmon_cpu_current = 99;

        if (btn == 1 && last_btn_state == 0) {
            if (mx <= 60) {
                if (my >= 10 && my <= 54) {
                    show_launcher = !show_launcher;
                } else {
                    int cur_y = 70;
                    for(int i = 0; i < num_windows; i++) {
                        if (my >= cur_y && my <= cur_y + 40) {
                            UIWindow* target = windows[i];
                            for (int j = i; j < num_windows - 1; j++) { windows[j] = windows[j + 1]; }
                            windows[num_windows - 1] = target;
                            show_launcher = 0;
                            break;
                        }
                        cur_y += 50;
                    }
                }
            } else if (show_launcher) {
                int l_w = 400; int l_h = 700; int l_x = 70; int l_y = 10;
                
                if (mx >= l_x && mx <= l_x + l_w && my >= l_y && my <= l_y + l_h) {
                    int start_x = l_x + 30; int start_y = l_y + 100; int size = 60; int spacing = 20;
                    for(int i=0; i<7; i++) {
                        int c_col = i % 4; int c_row = i / 4;
                        int ax = start_x + (c_col * (size + spacing)); int ay = start_y + (c_row * (size + spacing + 30));
                        
                        if (mx >= ax && mx <= ax + size && my >= ay && my <= ay + size) {
                            if (i == 0) { 
                                windows[num_windows++] = Window_Create(next_pid++, 100, 50, 540, 380, "Terminal", Color.Overlay);
                                focused_input = 1; show_launcher = 0;
                            } else if (i == 2) { 
                                if (calc_pid == -1 && num_windows < 10) {
                                    int new_pid = exec("calc.elf");
                                    if(new_pid > 0) {
                                        calc_pid = new_pid; calc_win_id = next_pid++;
                                        windows[num_windows++] = Window_Create(calc_win_id, 150, 300, 290, 220, "Calculadora", Color.Surface);
                                        calc_display_value = 0; show_launcher = 0;
                                    }
                                }
                            } else if (i == 3) { 
                                if (num_windows < 10) {
                                    windows[num_windows++] = Window_Create(next_pid++, 500, 100, 340, 350, "System Monitor", Color.Overlay);
                                    show_launcher = 0;
                                }
                            } else if (i == 4) { 
                                if (snake_win_id == -1 && snake_pid == -1 && num_windows < 10) {
                                    int new_pid = exec("snake.elf");
                                    if(new_pid > 0) {
                                        snake_pid = new_pid; snake_win_id = next_pid++;
                                        windows[num_windows++] = Window_Create(snake_win_id, 250, 150, 600, 430, "Snake", Color.Overlay);
                                        snake_surface = (uint32_t*)get_shared_buffer(1);
                                        focused_input = 4; show_launcher = 0;
                                    }
                                }
                            } else if (i == 5) {
                                if (explorer_win_id == -1 && explorer_pid == -1 && num_windows < 10) {
                                    int new_pid = exec("explorer.elf");
                                    if(new_pid > 0) {
                                        explorer_pid = new_pid; explorer_win_id = next_pid++;
                                        windows[num_windows++] = Window_Create(explorer_win_id, 200, 100, 700, 520, "Explorador de Ficheiros", Color.Surface);
                                        show_launcher = 0; focused_input = 0;
                                    }
                                }
                            } else if (i == 6) {
                                if (player_win_id == -1 && player_pid == -1 && num_windows < 10) {
                                    int new_pid = exec("player.elf");
                                    if(new_pid > 0) {
                                        player_pid = new_pid; player_win_id = next_pid++;
                                        windows[num_windows++] = Window_Create(player_win_id, 300, 200, 404, 285, "Player", Color.Surface);
                                        show_launcher = 0; focused_input = 0;
                                    }
                                }
                            }
                        }
                    }
                } else { show_launcher = 0; }
            } else {
                int clicked_win_idx = -1;
                for (int i = num_windows - 1; i >= 0; i--) {
                    UIWindow* w = windows[i];
                    if (mx >= w->x && mx <= w->x + w->w && my >= w->y && my <= w->y + w->h) { clicked_win_idx = i; break; }
                }
                
                if (clicked_win_idx != -1) {
                    UIWindow* target = windows[clicked_win_idx];
                    for (int j = clicked_win_idx; j < num_windows - 1; j++) { windows[j] = windows[j + 1]; }
                    windows[num_windows - 1] = target;
                    
                    if (my <= target->y + 30) { 
                        if (target->CloseClicked(target, mx, my)) {
                            if(target->id == calc_win_id) { kill(calc_pid); calc_pid = -1; calc_win_id = -1; }
                            if(target->id == snake_win_id) { kill(snake_pid); snake_pid = -1; snake_win_id = -1; snake_surface = 0; focused_input = 1; }
                            if(target->id == explorer_win_id) { kill(explorer_pid); explorer_pid = -1; explorer_win_id = -1; explorer_surface = 0; }
                            if(target->id == player_win_id) { kill(player_pid); player_pid = -1; player_win_id = -1; player_surface = 0; }
                            free(target); num_windows--; dragging = 0; 
                        } else { dragging = 1; drag_win = target; drag_offset_x = mx - target->x; drag_offset_y = my - target->y; }
                    }
                    
                    if (!dragging && strcmp(target->title, "Terminal") == 0) { focused_input = 1; }
                    if (!dragging && target->id == snake_win_id) { focused_input = 4; }
                    if (!dragging && target->id == explorer_win_id) { focused_input = 0; }
                    if (!dragging && target->id == player_win_id) { focused_input = 0; }
                    
                    if (!dragging && strcmp(target->title, "System Monitor") == 0) {
                        int list_y = target->y + 115;
                        for(int k = 0; k < num_windows; k++) {
                            if (list_y > target->y + target->h - 30) break;
                            UIButton b_kill = Button_Create(target->x + target->w - 45, list_y + 2, 21, 21, "X", Color.Red, Color.Text);
                            if (b_kill.IsClicked(&b_kill, mx, my)) {
                                if(windows[k]->id == calc_win_id) { kill(calc_pid); calc_pid = -1; calc_win_id = -1; }
                                if(windows[k]->id == snake_win_id) { kill(snake_pid); snake_pid = -1; snake_win_id = -1; snake_surface = 0; focused_input = 1; }
                                if(windows[k]->id == explorer_win_id) { kill(explorer_pid); explorer_pid = -1; explorer_win_id = -1; explorer_surface = 0; }
                                if(windows[k]->id == player_win_id) { kill(player_pid); player_pid = -1; player_win_id = -1; player_surface = 0; }
                                free(windows[k]);
                                for (int j = k; j < num_windows - 1; j++) { windows[j] = windows[j + 1]; }
                                num_windows--; dragging = 0;
                                break;
                            }
                            list_y += 30;
                        }
                    }

                    if (!dragging && target->id == calc_win_id) {
                        focused_input = 0;
                        UIButton b1 = Button_Create(target->x + 20, target->y + 105, 55, 35, "1", Color.Blue, Color.Dark);
                        UIButton b2 = Button_Create(target->x + 85, target->y + 105, 55, 35, "2", Color.Blue, Color.Dark);
                        UIButton b3 = Button_Create(target->x + 150, target->y + 105, 55, 35, "3", Color.Blue, Color.Dark);
                        UIButton bc = Button_Create(target->x + 215, target->y + 105, 55, 35, "C", Color.Red, Color.Dark);
                        UIButton b4 = Button_Create(target->x + 20, target->y + 150, 55, 35, "4", Color.Blue, Color.Dark);
                        UIButton b5 = Button_Create(target->x + 85, target->y + 150, 55, 35, "5", Color.Blue, Color.Dark);
                        UIButton b6 = Button_Create(target->x + 150, target->y + 150, 55, 35, "6", Color.Blue, Color.Dark);

                        if (b1.IsClicked(&b1, mx, my)) send_msg(calc_pid, 1, 1, 0);
                        if (b2.IsClicked(&b2, mx, my)) send_msg(calc_pid, 1, 2, 0);
                        if (b3.IsClicked(&b3, mx, my)) send_msg(calc_pid, 1, 3, 0);
                        if (bc.IsClicked(&bc, mx, my)) send_msg(calc_pid, 3, 0, 0);
                        if (b4.IsClicked(&b4, mx, my)) send_msg(calc_pid, 1, 4, 0);
                        if (b5.IsClicked(&b5, mx, my)) send_msg(calc_pid, 1, 5, 0);
                        if (b6.IsClicked(&b6, mx, my)) send_msg(calc_pid, 1, 6, 0);
                    }
                }
            }
        } else if (btn == 1 && dragging && drag_win != 0) { 
            if (mouse_moved) {
                drag_win->x = mx - drag_offset_x; if (drag_win->x < 65) drag_win->x = 65; 
                drag_win->y = my - drag_offset_y; if (drag_win->y < 0) drag_win->y = 0;
                state_changed = 1;
            }
        } else if (btn == 0) { dragging = 0; }
        
        last_mx = mx; last_my = my; last_btn_state = btn; last_s = s;

        if (num_windows > 0) {
            UIWindow* focused_win = windows[num_windows - 1];
            if (key) {
                if (focused_input == 1 && strcmp(focused_win->title, "Terminal") == 0) {
                    if (key == '\b' && term_cursor > 0) { term_buffer[--term_cursor] = '\0'; } 
                    else if (key == '\n') {
                        char in_cmd[140]; strcpy(in_cmd, "λ "); strcat(in_cmd, term_buffer); add_hist(in_cmd);
                        if (strcmp(term_buffer, "clear") == 0) { hist_cnt = 0; term_view_idx = 0; } 
                        else if (strcmp(term_buffer, "task") == 0) {
                            if (num_windows < 10) windows[num_windows++] = Window_Create(next_pid++, 500, 100, 340, 350, "System Monitor", Color.Overlay);
                        } 
                        else if (strcmp(term_buffer, "ls") == 0) { char d_files[512]; get_disk_files(d_files); add_hist("--- Conteudo ---"); add_hist(d_files); }
                        else if (find_substr(term_buffer, "cd ") == 0) { cd(term_buffer + 3); add_hist("Diretorio alterado."); }
                        else if (strcmp(term_buffer, "cd") == 0) { cd(""); add_hist("Retornado a Raiz."); }
                        else if (strcmp(term_buffer, "calc") == 0) {
                            if (calc_pid == -1 && num_windows < 10) {
                                int new_pid = exec("calc.elf");
                                if(new_pid > 0) {
                                    calc_pid = new_pid; calc_win_id = next_pid++;
                                    windows[num_windows++] = Window_Create(calc_win_id, 150, 300, 290, 220, "Calculadora", Color.Surface); calc_display_value = 0;
                                }
                            }
                        }
                        else if (strcmp(term_buffer, "browser") == 0) {
                            add_hist("Browser removido temporariamente.");
                        }
                        else if (strcmp(term_buffer, "explorer") == 0) {
                            if (explorer_win_id == -1 && explorer_pid == -1 && num_windows < 10) {
                                int new_pid = exec("explorer.elf");
                                if(new_pid > 0) {
                                    explorer_pid = new_pid; explorer_win_id = next_pid++;
                                    windows[num_windows++] = Window_Create(explorer_win_id, 200, 100, 700, 520, "Explorador de Ficheiros", Color.Surface); focused_input = 0;
                                }
                            }
                        }
                        else if (strcmp(term_buffer, "player") == 0) {
                            if (player_win_id == -1 && player_pid == -1 && num_windows < 10) {
                                int new_pid = exec("player.elf");
                                if(new_pid > 0) {
                                    player_pid = new_pid; player_win_id = next_pid++;
                                    windows[num_windows++] = Window_Create(player_win_id, 300, 200, 404, 285, "Player", Color.Surface); focused_input = 0;
                                }
                            }
                        }
                        else if (strcmp(term_buffer, "snake") == 0) {
                            if (snake_win_id == -1 && snake_pid == -1 && num_windows < 10) {
                                int new_pid = exec("snake.elf");
                                if(new_pid > 0) {
                                    snake_pid = new_pid; snake_win_id = next_pid++;
                                    windows[num_windows++] = Window_Create(snake_win_id, 250, 150, 600, 430, "Snake", Color.Overlay);
                                    snake_surface = (uint32_t*)get_shared_buffer(1);
                                    focused_input = 4;
                                }
                            }
                        }
                        term_cursor = 0; term_buffer[0] = '\0'; term_view_idx = hist_cnt > 15 ? hist_cnt - 15 : 0;
                    } else if (key != '\b' && key != '\n' && term_cursor < 254) {
                        term_buffer[term_cursor++] = key; term_buffer[term_cursor] = '\0';
                    }
                } else if (focused_input == 4 && focused_win->id == snake_win_id) {
                    send_msg(snake_pid, 4, key, 0);
                }
            }
        }

        if (state_changed) {
            draw_buffer(0, 0, 1280, 720, bg_buffer);
            
            for (int i = 0; i < num_windows; i++) {
                UIWindow* w = windows[i]; int is_focused = (i == num_windows - 1);
                
                w->Draw(w, is_focused, dragging);
                
                if (strcmp(w->title, "Terminal") == 0) {
                    int line_height = 20; int max_lines = (w->h - 80) / line_height;
                    if (term_view_idx > hist_cnt - max_lines && hist_cnt > max_lines) term_view_idx = hist_cnt - max_lines;
                    if (term_view_idx < 0) term_view_idx = 0;

                    for(int j=0; j<max_lines && (term_view_idx + j) < hist_cnt; j++) {
                        draw_string(w->x + 15, w->y + 40 + (j*line_height), term_hist[term_view_idx + j], 0xA5ADCE);
                    }
                    
                    draw_rect_alpha(w->x + w->w - 22, w->y + 40, 12, w->h - 80, Color.Dark, 150);
                    if (hist_cnt > 0) {
                        int max_s = hist_cnt > max_lines ? hist_cnt - max_lines : 1;
                        int h_size = ((w->h - 80) * max_lines) / (hist_cnt > max_lines ? hist_cnt : max_lines);
                        if (h_size < 20) h_size = 20;
                        int h_y = w->y + 40 + (term_view_idx * (w->h - 80 - h_size)) / max_s;
                        draw_rect_alpha(w->x + w->w - 20, h_y, 8, h_size, 0x585B70, 255);
                    }

                    draw_rect_alpha(w->x, w->y + w->h - 35, w->w, 1, 0x313244, 255);
                    draw_rect_alpha(w->x, w->y + w->h - 34, w->w, 34, Color.Surface, 255);
                    draw_string(w->x + 15, w->y + w->h - 22, "λ", Color.Green);
                    draw_string(w->x + 35, w->y + w->h - 22, term_buffer, Color.Text);
                    if (is_focused && focused_input == 1) draw_rect_alpha(w->x + 35 + (term_cursor * 8), w->y + w->h - 22, 8, 16, Color.Text, 180);

                } else if (strcmp(w->title, "System Monitor") == 0) {
                    char stat_buf[64]; char num_buf[16];
                    
                    strcpy(stat_buf, "Uso de CPU: "); itoa(sysmon_cpu_current, num_buf); strcat(stat_buf, num_buf); strcat(stat_buf, "%");
                    draw_string(w->x + 20, w->y + 50, stat_buf, Color.Text);
                    
                    strcpy(stat_buf, "RAM: "); format_bytes(get_used_memory(), num_buf); strcat(stat_buf, num_buf);
                    draw_string(w->x + 180, w->y + 50, stat_buf, Color.Text);

                    draw_rect_alpha(w->x + 15, w->y + 80, w->w - 30, 1, Color.Overlay, 255);
                    
                    strcpy(stat_buf, "Processos Ativos: "); itoa(num_windows, num_buf); strcat(stat_buf, num_buf);
                    draw_string(w->x + 20, w->y + 90, stat_buf, Color.Green);

                    int list_y = w->y + 115;
                    for(int k=0; k<num_windows; k++) {
                        if (list_y > w->y + w->h - 30) break;
                        draw_rect_alpha(w->x + 20, list_y, w->w - 40, 25, Color.Dark, 255);
                        draw_string(w->x + 30, list_y + 8, windows[k]->title, Color.Text);
                        
                        UIButton b_kill = Button_Create(w->x + w->w - 45, list_y + 2, 21, 21, "X", Color.Red, Color.Text);
                        b_kill.Draw(&b_kill);
                        
                        list_y += 30;
                    }
                } else if (w->id == calc_win_id) {
                    char val_str[32]; itoa(calc_display_value, val_str);
                    draw_rect_alpha(w->x + 20, w->y + 45, w->w - 40, 45, Color.Dark, 255);
                    draw_string(w->x + w->w - 30 - (str_len(val_str)*10), w->y + 60, val_str, Color.Text);
                    
                    UIButton b1 = Button_Create(w->x + 20, w->y + 105, 55, 35, "1", Color.Blue, Color.Dark); b1.Draw(&b1);
                    UIButton b2 = Button_Create(w->x + 85, w->y + 105, 55, 35, "2", Color.Blue, Color.Dark); b2.Draw(&b2);
                    UIButton b3 = Button_Create(w->x + 150, w->y + 105, 55, 35, "3", Color.Blue, Color.Dark); b3.Draw(&b3);
                    UIButton bc = Button_Create(w->x + 215, w->y + 105, 55, 35, "C", Color.Red, Color.Dark); bc.Draw(&bc);
                    UIButton b4 = Button_Create(w->x + 20, w->y + 150, 55, 35, "4", Color.Blue, Color.Dark); b4.Draw(&b4);
                    UIButton b5 = Button_Create(w->x + 85, w->y + 150, 55, 35, "5", Color.Blue, Color.Dark); b5.Draw(&b5);
                    UIButton b6 = Button_Create(w->x + 150, w->y + 150, 55, 35, "6", Color.Blue, Color.Dark); b6.Draw(&b6);

                } else if (w->id == snake_win_id) {
                    if (snake_surface) {
                        draw_buffer(w->x, w->y + 30, w->w, w->h - 30, snake_surface);
                    }
                } else if (w->id == explorer_win_id) {
                    if (!explorer_surface) explorer_surface = (uint32_t*)get_shared_buffer(2);
                    if (explorer_surface) {
                        draw_buffer(w->x + 2, w->y + 31, 600, 400, explorer_surface);
                    }
                } else if (w->id == player_win_id) {
                    if (!player_surface) player_surface = (uint32_t*)get_shared_buffer(3);
                    if (player_surface) {
                        draw_buffer(w->x + 2, w->y + 31, 400, 250, player_surface);
                    }
                }
            }

            if(show_launcher) {
                int l_w = 400, l_h = 700; int l_x = 70; int l_y = 10;
                
                draw_rect_alpha(l_x + 8, l_y + 8, l_w, l_h, 0x000000, 100);
                draw_rect_alpha(l_x + 10, l_y, l_w - 20, l_h, Color.Overlay, 250);
                draw_rect_alpha(l_x, l_y + 10, 10, l_h - 20, Color.Overlay, 250);
                draw_rect_alpha(l_x + l_w - 10, l_y + 10, 10, l_h - 20, Color.Overlay, 250);
                draw_circle_alpha(l_x + 10, l_y + 10, 10, Color.Overlay, 250);
                draw_circle_alpha(l_x + l_w - 10, l_y + 10, 10, Color.Overlay, 250);
                draw_circle_alpha(l_x + 10, l_y + l_h - 10, 10, Color.Overlay, 250);
                draw_circle_alpha(l_x + l_w - 10, l_y + l_h - 10, 10, Color.Overlay, 250);
                
                draw_rect_alpha(l_x + 30, l_y + 30, l_w - 60, 40, Color.Dark, 255);
                draw_string(l_x + 45, l_y + 42, "Search apps...", 0x6C7086);
                draw_string(l_x + 30, l_y + 90, "Apps", Color.Text);
                
                int start_x = l_x + 30; int start_y = l_y + 130; int size = 60; int spacing = 20;
                
                uint32_t colors[] = {Color.Blue, Color.Red, Color.Green, Color.Yellow, Color.Purple, Color.Text, Color.Blue};
                const char* labels[] = {"Terminal", "Browser", "Calc", "SysMon", "Snake", "Explorer", "Player"};
                int icon_map[] = {0, 1, 2, 3, 4, 5, 1};
                
                for(int i=0; i<7; i++) {
                    int c_col = i % 4; int c_row = i / 4;
                    int ax = start_x + (c_col * (size + spacing)); int ay = start_y + (c_row * (size + spacing + 30));
                    
                    draw_rect_alpha(ax + 5, ay, size - 10, size, colors[i], 255);
                    draw_rect_alpha(ax, ay + 5, 5, size - 10, colors[i], 255);
                    draw_rect_alpha(ax + size - 5, ay + 5, 5, size - 10, colors[i], 255);
                    draw_circle_alpha(ax + 5, ay + 5, 5, colors[i], 255);
                    draw_circle_alpha(ax + size - 5, ay + 5, 5, colors[i], 255);
                    draw_circle_alpha(ax + 5, ay + size - 5, 5, colors[i], 255);
                    draw_circle_alpha(ax + size - 5, ay + size - 5, 5, colors[i], 255);
                    
                    draw_image(ax + 14, ay + 14, app_icons[icon_map[i]]);

                    int lbl_len = str_len(labels[i]); int lbl_x = ax + (size/2) - (lbl_len * 4);
                    draw_string(lbl_x, ay + size + 10, labels[i], 0xA6ADC8);
                }
            }

            draw_rect_alpha(0, 0, 60, 720, Color.Dark, 255);
            
            draw_rect_alpha(8, 10, 44, 44, Color.Overlay, 255); 
            draw_rect_alpha(8 + 12, 10 + 10, 8, 8, Color.Blue, 255);
            draw_rect_alpha(8 + 24, 10 + 10, 8, 8, Color.Blue, 255);
            draw_rect_alpha(8 + 12, 10 + 26, 8, 8, Color.Blue, 255);
            draw_rect_alpha(8 + 24, 10 + 26, 8, 8, Color.Blue, 255);

            int cur_y = 70;
            for(int i = 0; i < num_windows; i++) {
                int is_foc = (i == num_windows - 1); 
                uint32_t icon_color = Color.Text; int icon_idx = 0;
                
                if (strcmp(windows[i]->title, "Terminal") == 0) { icon_color = Color.Blue; icon_idx = 0; }
                else if (strcmp(windows[i]->title, "Calculadora") == 0) { icon_color = Color.Green; icon_idx = 2; }
                else if (strcmp(windows[i]->title, "System Monitor") == 0) { icon_color = Color.Yellow; icon_idx = 3; }
                else if (strcmp(windows[i]->title, "Snake") == 0) { icon_color = Color.Purple; icon_idx = 4; }
                else if (strcmp(windows[i]->title, "Explorador de Ficheiros") == 0) { icon_color = Color.Text; icon_idx = 5; }
                else if (strcmp(windows[i]->title, "Player") == 0) { icon_color = Color.Blue; icon_idx = 1; }
                
                draw_rect_alpha(10, cur_y, 40, 40, icon_color, 255);
                draw_image(14, cur_y + 4, app_icons[icon_idx]);
                
                if(is_foc) draw_rect_alpha(2, cur_y + 10, 4, 20, Color.Blue, 255);
                cur_y += 50;
            }

            char time_h[16]; char time_m[16]; char temp[16];
            itoa(h, temp); if(h<10){strcpy(time_h,"0"); strcat(time_h,temp);} else strcpy(time_h,temp);
            itoa(m, temp); if(m<10){strcpy(time_m,"0"); strcat(time_m,temp);} else strcpy(time_m,temp);
            draw_string(14, 720 - 45, time_h, Color.Text);
            draw_string(14, 720 - 30, time_m, Color.Text);
            
            update_screen(); 
            state_changed = 0;
        } else if (mouse_moved) {
            update_screen();
        }
    }
    return 0;
}