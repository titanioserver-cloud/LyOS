#include "../lib/protos.h"
#include "../include/icons.h"

#define EXFAT_MAX_NAME 256
#define EXFAT_MAX_FILES 256
#define EXFAT_MAX_PATH 512

#define WIN_W 600
#define WIN_H 400

typedef struct {
    char name[EXFAT_MAX_NAME];
    uint32_t size;
    int is_dir;
    int is_valid;
} FileEntry;

FileEntry entries[EXFAT_MAX_FILES];
int entry_count = 0;
int scroll_offset = 0;
char cwd[EXFAT_MAX_PATH] = "/";
char status_msg[256] = "Explorador de Ficheiros";
int status_timer = 0;
int selected_idx = -1;
char dialog_buf[64] = "";
int dialog_active = 0;
int dialog_mode = 0;

void format_size_str(uint32_t size, char* out) {
    char tmp[16];
    if (size < 1024) { itoa(size, tmp); strcpy(out, tmp); strcat(out, " B"); }
    else if (size < 1024*1024) { itoa(size/1024, tmp); strcpy(out, tmp); strcat(out, " KB"); }
    else { itoa(size/(1024*1024), tmp); strcpy(out, tmp); strcat(out, " MB"); }
}

void refresh_dir() {
    entry_count = 0;
    char raw_list[2048];
    get_disk_files(raw_list);
    char name_buf[EXFAT_MAX_PATH];
    int i = 0;
    while (raw_list[i] && entry_count < EXFAT_MAX_FILES) {
        if (raw_list[i] == '[') {
            i++; int j = 0;
            while (raw_list[i] && raw_list[i] != ']' && j < EXFAT_MAX_PATH - 1) name_buf[j++] = raw_list[i++];
            name_buf[j] = '\0'; if (raw_list[i] == ']') i++;
            if (j > 0) { strcpy(entries[entry_count].name, name_buf); entries[entry_count].is_dir = 1; entries[entry_count].size = 0; entries[entry_count].is_valid = 1; entry_count++; }
        } else if (raw_list[i] == '{') {
            i++; int j = 0;
            while (raw_list[i] && raw_list[i] != ':' && j < EXFAT_MAX_PATH - 1) name_buf[j++] = raw_list[i++];
            name_buf[j] = '\0'; if (raw_list[i] == ':') i++;
            uint32_t fsize = 0;
            while (raw_list[i] && raw_list[i] >= '0' && raw_list[i] <= '9') fsize = fsize * 10 + (raw_list[i++] - '0');
            if (raw_list[i] == '}') i++;
            if (j > 0) { strcpy(entries[entry_count].name, name_buf); entries[entry_count].is_dir = 0; entries[entry_count].size = fsize; entries[entry_count].is_valid = 1; entry_count++; }
        } else i++;
    }
}

void show_status(const char* msg) { strcpy(status_msg, msg); status_timer = 150; }

void create_new_file(const char* name) {
    if (disk_mkfile(name)) { show_status("Ficheiro criado: "); strcat(status_msg, name); refresh_dir(); }
    else show_status("Erro ao criar ficheiro!");
}

void create_new_dir(const char* name) {
    if (disk_mkdir(name)) { show_status("Pasta criada: "); strcat(status_msg, name); refresh_dir(); }
    else show_status("Erro ao criar pasta!");
}

void delete_selected() {
    if (selected_idx < 0 || selected_idx >= entry_count) return;
    if (disk_delete(entries[selected_idx].name)) { show_status("Eliminado: "); strcat(status_msg, entries[selected_idx].name); selected_idx = -1; refresh_dir(); }
    else show_status("Erro ao eliminar!");
}

void open_selected() {
    if (selected_idx < 0 || selected_idx >= entry_count) return;
    if (entries[selected_idx].is_dir) {
        cd(entries[selected_idx].name); disk_getcwd(cwd); scroll_offset = 0; selected_idx = -1; refresh_dir();
        show_status("Aberto: "); strcat(status_msg, entries[selected_idx].name);
    } else {
        char size_str[16]; format_size_str(entries[selected_idx].size, size_str);
        show_status("Ficheiro: "); strcat(status_msg, entries[selected_idx].name);
        strcat(status_msg, " ("); strcat(status_msg, size_str); strcat(status_msg, ")");
    }
}

void draw_to_buffer(uint32_t* buf, int buf_w, int buf_h) {
    int x = 0, y = 0, width = buf_w, height = buf_h;
    for (int i = 0; i < buf_w * buf_h; i++) buf[i] = Color.Surface;
    
    draw_rect_to_buffer(buf, buf_w, x + 8, y, width - 16, 30, Color.Dark);
    char pd[EXFAT_MAX_PATH + 10]; strcpy(pd, "Local: "); strcat(pd, cwd);
    if (str_len(pd) > 50) { draw_string_to_buffer(buf, buf_w, x + 15, y + 6, "...", Color.Green); draw_string_to_buffer(buf, buf_w, x + 35, y + 6, pd + str_len(pd) - 42, Color.Green); }
    else draw_string_to_buffer(buf, buf_w, x + 15, y + 6, pd, Color.Green);
    
    int nav_y = y + 34;
    draw_rect_to_buffer(buf, buf_w, x + 10, nav_y, width - 20, 28, Color.Overlay);
    
    int list_x = x + 14, list_y = y + 70, list_w = width - 28, list_h = height - 108;
    int item_h = 24, max_visible = list_h / item_h;
    draw_rect_to_buffer(buf, buf_w, list_x, list_y, list_w, list_h, Color.Dark);
    
    if (entry_count == 0) {
        draw_string_to_buffer(buf, buf_w, list_x + 20, list_y + 20, "(Pasta vazia)", 0x6C7086);
    } else {
        draw_rect_to_buffer(buf, buf_w, list_x, list_y, list_w, 20, Color.Overlay);
        draw_string_to_buffer(buf, buf_w, list_x + 10, list_y + 3, "Nome", Color.Text);
        draw_string_to_buffer(buf, buf_w, list_x + list_w - 80, list_y + 3, "Tamanho", Color.Text);
        draw_string_to_buffer(buf, buf_w, list_x + list_w - 30, list_y + 3, "Tipo", Color.Text);
        
        if (scroll_offset > entry_count - max_visible) scroll_offset = entry_count - max_visible;
        if (scroll_offset < 0) scroll_offset = 0;
        
        for (int i = scroll_offset; i < entry_count && i < scroll_offset + max_visible; i++) {
            int ry = list_y + 24 + (i - scroll_offset) * item_h;
            uint32_t row_c = (i == selected_idx) ? Color.Blue : Color.Dark;
            uint32_t tc = (i == selected_idx) ? 0xFFFFFF : Color.Text;
            draw_rect_to_buffer(buf, buf_w, list_x, ry, list_w, item_h - 1, row_c);
            
            char dn[EXFAT_MAX_NAME + 4]; uint32_t nc = entries[i].is_dir ? Color.Blue : tc;
            strcpy(dn, entries[i].is_dir ? "[/] " : "    ");
            strcat(dn, entries[i].name);
            if (str_len(dn) > 32) { dn[29] = '.'; dn[30] = '.'; dn[31] = '\0'; }
            draw_string_to_buffer(buf, buf_w, list_x + 10, ry + 4, dn, nc);
            
            if (!entries[i].is_dir) {
                char sz[16]; format_size_str(entries[i].size, sz);
                draw_string_to_buffer(buf, buf_w, list_x + list_w - 85, ry + 4, sz, tc);
                draw_string_to_buffer(buf, buf_w, list_x + list_w - 30, ry + 4, "F", tc);
            } else draw_string_to_buffer(buf, buf_w, list_x + list_w - 30, ry + 4, "D", Color.Blue);
        }
    }
    
    if (entry_count > max_visible) {
        int sb_x = list_x + list_w - 8, sb_h = list_h;
        int thumb_h = (sb_h * max_visible) / entry_count; if (thumb_h < 10) thumb_h = 10;
        int thumb_y = list_y + (scroll_offset * (sb_h - thumb_h)) / (entry_count - max_visible);
        draw_rect_to_buffer(buf, buf_w, sb_x, list_y, 6, sb_h, Color.Overlay);
        draw_rect_to_buffer(buf, buf_w, sb_x, thumb_y, 6, thumb_h, 0x585B70);
    }
    
    draw_rect_to_buffer(buf, buf_w, x + 10, y + height - 24, width - 20, 18, Color.Overlay);
    draw_string_to_buffer(buf, buf_w, x + 15, y + height - 22, status_msg, Color.Text);
    char cs[16]; itoa(entry_count, cs);
    draw_string_to_buffer(buf, buf_w, x + width - 80, y + height - 22, cs, 0x6C7086);
    draw_string_to_buffer(buf, buf_w, x + width - 57, y + height - 22, " itens", 0x6C7086);
}

int main() {
    int content_w = WIN_W, content_h = WIN_H;
    uint32_t* surface = (uint32_t*)create_shared_buffer(2, WIN_W * WIN_H * 4);
    if (!surface) return -1;
    
    cd(""); disk_getcwd(cwd); refresh_dir();
    show_status("Explorador de Ficheiros carregado");
    
    int last_btn = 0, state_changed = 1;
    
    while (1) {
        yield();
        int mx = get_mouse_x(), my = get_mouse_y(), btn = get_mouse_btn();
        char key = get_key();
        int m_type, m_d1, m_d2; while (recv_msg(&m_type, &m_d1, &m_d2) > 0) {}
        
        if (status_timer > 0) status_timer--;
        if (status_timer == 1) strcpy(status_msg, "Explorador de Ficheiros");
        if (btn != last_btn || key != 0) state_changed = 1;
        
        if (key) {
            if (dialog_active) {
                if (key == '\b' && str_len(dialog_buf) > 0) dialog_buf[str_len(dialog_buf) - 1] = '\0';
                else if (key == '\n') {
                    if (str_len(dialog_buf) > 0) {
                        if (dialog_mode == 1) create_new_file(dialog_buf); else create_new_dir(dialog_buf);
                    }
                    dialog_active = 0;
                } else if (key > 31 && str_len(dialog_buf) < 60) {
                    int len = str_len(dialog_buf); dialog_buf[len] = key; dialog_buf[len + 1] = '\0';
                }
            } else {
                if (key == 'r' || key == 'R') { refresh_dir(); show_status("Lista atualizada"); }
                if (key == '\b') { cd(".."); disk_getcwd(cwd); scroll_offset = 0; selected_idx = -1; refresh_dir(); show_status("Subir pasta"); }
                if (key == '\n' && selected_idx >= 0) open_selected();
                if (key == 0x48 && selected_idx > 0) { selected_idx--; if (selected_idx < scroll_offset) scroll_offset = selected_idx; }
                if (key == 0x50 && selected_idx < entry_count - 1) { selected_idx++; if (selected_idx >= scroll_offset + 15) scroll_offset = selected_idx - 14; }
                if (key == 0x7F && selected_idx >= 0) delete_selected();
                if (key == 'n' || key == 'N') { dialog_active = 1; dialog_mode = 1; strcpy(dialog_buf, ""); }
                if (key == 'm' || key == 'M') { dialog_active = 1; dialog_mode = 2; strcpy(dialog_buf, ""); }
                if (key == 'h' || key == 'H') { cd(""); disk_getcwd(cwd); scroll_offset = 0; selected_idx = -1; refresh_dir(); show_status("Raiz do disco"); }
            }
            state_changed = 1;
        }
        
        if (state_changed) {
            draw_to_buffer(surface, content_w, content_h);
            state_changed = 0;
        }
        last_btn = btn;
    }
    return 0;
}