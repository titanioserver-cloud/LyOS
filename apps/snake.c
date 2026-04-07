#include "../lib/protos.h"

int snake_x[100];
int snake_y[100];

int main() {
    uint32_t* surface = (uint32_t*)create_shared_buffer(1, 600 * 400 * 4);
    int snake_length = 3; int snake_dir = 1;
    int food_x = 15; int food_y = 10; int snake_score = 0; int snake_game_over = 0;
    for(int k = 0; k < snake_length; k++) { snake_x[k] = 10 - k; snake_y[k] = 10; }
    
    int frame_count = 0;

    while(1) {
        yield();
        int type, d1, d2;
        int sender = recv_msg(&type, &d1, &d2);
        if (sender > 0 && type == 4) {
            char key = (char)d1;
            if (key == 'w' && snake_dir != 2) snake_dir = 0;
            else if (key == 'd' && snake_dir != 3) snake_dir = 1;
            else if (key == 's' && snake_dir != 0) snake_dir = 2;
            else if (key == 'a' && snake_dir != 1) snake_dir = 3;
            else if (key == 'r' && snake_game_over) {
                snake_length = 3; snake_dir = 1; snake_score = 0; snake_game_over = 0;
                for(int k = 0; k < snake_length; k++) { snake_x[k] = 10 - k; snake_y[k] = 10; }
            }
        }

        if (++frame_count > 20) { 
            frame_count = 0;
            if (!snake_game_over) {
                for (int i = snake_length - 1; i > 0; i--) {
                    snake_x[i] = snake_x[i-1]; snake_y[i] = snake_y[i-1];
                }
                if (snake_dir == 0) snake_y[0]--;
                else if (snake_dir == 1) snake_x[0]++;
                else if (snake_dir == 2) snake_y[0]++;
                else if (snake_dir == 3) snake_x[0]--;
                
                if (snake_x[0] < 0 || snake_x[0] >= 30 || snake_y[0] < 0 || snake_y[0] >= 20) snake_game_over = 1;
                for(int i = 1; i < snake_length; i++) { if (snake_x[0] == snake_x[i] && snake_y[0] == snake_y[i]) snake_game_over = 1; }
                
                if (snake_x[0] == food_x && snake_y[0] == food_y) {
                    snake_score += 10; snake_length++;
                    food_x = (food_x + 7) % 30; food_y = (food_y + 3) % 20;
                }
            }

            draw_rect_to_buffer(surface, 600, 0, 0, 600, 400, Color.Overlay);

            if (snake_game_over) {
                draw_string_to_buffer(surface, 600, 300 - 40, 200 - 10, "GAME OVER", Color.Red);
                draw_string_to_buffer(surface, 600, 300 - 70, 200 + 10, "Pressione R para recomecar", Color.Text);
            } else {
                draw_rect_to_buffer(surface, 600, food_x * 20, food_y * 20, 20, 20, Color.Red);
                for (int k = 0; k < snake_length; k++) {
                    draw_rect_to_buffer(surface, 600, snake_x[k] * 20, snake_y[k] * 20, 20, 20, k == 0 ? Color.Blue : Color.Green);
                }
            }
            char score_str[32]; strcpy(score_str, "Score: "); 
            char pts[10]; itoa(snake_score, pts); strcat(score_str, pts);
            draw_string_to_buffer(surface, 600, 500, 8, score_str, Color.Yellow);
        }
    }
    return 0;
}