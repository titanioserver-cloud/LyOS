#include "../include/render.h"

static void fill_rect_safe(int x, int y, int w, int h, uint32_t color, int clip_x, int clip_y, int clip_w, int clip_h) {
    if (x + w <= clip_x || x >= clip_x + clip_w || y + h <= clip_y || y >= clip_y + clip_h) return;
    
    int draw_x = x < clip_x ? clip_x : x;
    int draw_y = y < clip_y ? clip_y : y;
    int draw_w = (x + w > clip_x + clip_w) ? (clip_x + clip_w - draw_x) : (x + w - draw_x);
    int draw_h = (y + h > clip_y + clip_h) ? (clip_y + clip_h - draw_y) : (y + h - draw_y);
    
    if (draw_w > 0 && draw_h > 0) {
        draw_rect_alpha(draw_x, draw_y, draw_w, draw_h, color, 255);
    }
}

static void draw_text_safe(int x, int y, const char* text, uint32_t color, int clip_x, int clip_y, int clip_w, int clip_h) {
    if (!text) return;
    if (y + 16 <= clip_y || y >= clip_y + clip_h) return; 
    if (x >= clip_x + clip_w) return;
    
    draw_string(x, y, text, color);
}

void render_dom(DOMNode* node, int clip_x, int clip_y, int clip_w, int clip_h) {
    if (!node || node->display == 0) return;
    
    if (node->y + node->computed_height < clip_y || node->y > clip_y + clip_h) {
        return; 
    }
    
    if (strcmp(node->tag, "#text") != 0) {
        if (node->bg_color != 0xFFFFFFFF) {
            fill_rect_safe(node->x, node->y, node->computed_width, node->computed_height, 
                          node->bg_color, clip_x, clip_y, clip_w, clip_h);
        }
        
        if (node->border[0] > 0) {
            int b = node->border[0];
            fill_rect_safe(node->x, node->y, node->computed_width, b, 0x000000, clip_x, clip_y, clip_w, clip_h);
            fill_rect_safe(node->x, node->y + node->computed_height - b, node->computed_width, b, 0x000000, clip_x, clip_y, clip_w, clip_h);
            fill_rect_safe(node->x, node->y, b, node->computed_height, 0x000000, clip_x, clip_y, clip_w, clip_h);
            fill_rect_safe(node->x + node->computed_width - b, node->y, b, node->computed_height, 0x000000, clip_x, clip_y, clip_w, clip_h);
        }
        
    } else if (node->text) {
        draw_text_safe(node->x, node->y, node->text, node->text_color, clip_x, clip_y, clip_w, clip_h);
    }
    
    for (int i = 0; i < node->child_count; i++) {
        render_dom(node->children[i], clip_x, clip_y, clip_w, clip_h);
    }
}