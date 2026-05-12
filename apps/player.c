#include "../lib/protos.h"
#include "../include/icons.h"

#define WIN_W 400
#define WIN_H 250

int main() {
    uint32_t* surface = (uint32_t*)create_shared_buffer(3, WIN_W * WIN_H * 4);
    if (!surface) return -1;
    
    int last_btn = 0;
    int playing = 0;
    int state_changed = 1;
    char status[64] = "Parado";
    
    while (1) {
        yield();
        int mx = get_mouse_x(), my = get_mouse_y(), btn = get_mouse_btn();
        char key = get_key();
        int m_type, m_d1, m_d2; while (recv_msg(&m_type, &m_d1, &m_d2) > 0) {}
        
        if (btn != last_btn || key != 0) state_changed = 1;
        
        if (key) {
            if (key == 'p' || key == 'P') {
                playing = !playing;
                if (playing) {
                    strcpy(status, "A reproduzir Audio WAV");
                    sys_play_wav("musica.wav");
                } else {
                    strcpy(status, "Parado");
                    sys_stop_wav();
                }
            }
            state_changed = 1;
        }
        
        if (state_changed) {
            for (int i = 0; i < WIN_W * WIN_H; i++) surface[i] = Color.Surface;
            
            draw_rect_to_buffer(surface, WIN_W, 10, 10, WIN_W - 20, 40, Color.Dark);
            draw_string_to_buffer(surface, WIN_W, 20, 22, "LyOS MP3/WAV Player", Color.Blue);
            
            draw_rect_to_buffer(surface, WIN_W, 10, 60, WIN_W - 20, 120, Color.Overlay);
            draw_string_to_buffer(surface, WIN_W, 100, 100, status, Color.Text);
            
            draw_rect_to_buffer(surface, WIN_W, 10, 190, WIN_W - 20, 50, Color.Dark);
            draw_string_to_buffer(surface, WIN_W, 120, 205, "[P] Play / Pause", Color.Green);
            
            state_changed = 0;
        }
        last_btn = btn;
    }
    return 0;
}