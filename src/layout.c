#include "../include/layout.h"

static int hex_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static uint32_t parse_color(const char* str) {
    if (!str) return 0xFFFFFFFF;
    if (str[0] == '#') {
        int len = str_len(str);
        if (len == 7) {
            uint32_t r = (hex_to_val(str[1]) << 4) | hex_to_val(str[2]);
            uint32_t g = (hex_to_val(str[3]) << 4) | hex_to_val(str[4]);
            uint32_t b = (hex_to_val(str[5]) << 4) | hex_to_val(str[6]);
            return (r << 16) | (g << 8) | b;
        } else if (len == 4) {
            uint32_t r = hex_to_val(str[1]); r = (r << 4) | r;
            uint32_t g = hex_to_val(str[2]); g = (g << 4) | g;
            uint32_t b = hex_to_val(str[3]); b = (b << 4) | b;
            return (r << 16) | (g << 8) | b;
        }
    }
    if (strcmp(str, "red") == 0) return Color.Red;
    if (strcmp(str, "green") == 0) return Color.Green;
    if (strcmp(str, "blue") == 0) return Color.Blue;
    if (strcmp(str, "yellow") == 0) return Color.Yellow;
    if (strcmp(str, "black") == 0) return 0x000000;
    if (strcmp(str, "white") == 0) return 0xFFFFFF;
    if (strcmp(str, "transparent") == 0) return 0xFFFFFFFF;
    
    return 0xFFFFFFFF;
}

static int parse_px(const char* str) {
    if (!str) return 0;
    int val = 0;
    int sign = 1;
    if (*str == '-') { sign = -1; str++; }
    while (*str >= '0' && *str <= '9') {
        val = val * 10 + (*str - '0');
        str++;
    }
    return val * sign;
}

static void apply_styles(DOMNode* node) {
    if (!node || strcmp(node->tag, "#text") == 0) return;

    node->display = 1; 
    if (strcmp(node->tag, "span") == 0 || strcmp(node->tag, "a") == 0 || 
        strcmp(node->tag, "strong") == 0 || strcmp(node->tag, "em") == 0 || 
        strcmp(node->tag, "img") == 0) {
        node->display = 2; 
    }
    if (strcmp(node->tag, "script") == 0 || strcmp(node->tag, "style") == 0 || 
        strcmp(node->tag, "head") == 0) {
        node->display = 0; 
    }

    node->bg_color = 0xFFFFFFFF; 
    node->text_color = Color.Text;
    node->font_size = 16;
    for(int i=0; i<4; i++) { node->margin[i] = 0; node->padding[i] = 0; node->border[i] = 0; }
    
    if (strcmp(node->tag, "h1") == 0) { node->font_size = 32; node->margin[0] = 10; node->margin[2] = 10; }
    else if (strcmp(node->tag, "h2") == 0) { node->font_size = 24; node->margin[0] = 8; node->margin[2] = 8; }
    else if (strcmp(node->tag, "p") == 0) { node->margin[0] = 8; node->margin[2] = 8; }

    int fixed_width = -1;
    int fixed_height = -1;

    for (int i = 0; i < node->style_count; i++) {
        const char* prop = node->styles[i].property;
        const char* val = node->styles[i].value;
        
        if (strcmp(prop, "display") == 0) {
            if (strcmp(val, "none") == 0) node->display = 0;
            else if (strcmp(val, "block") == 0) node->display = 1;
            else if (strcmp(val, "inline") == 0) node->display = 2;
            else if (strcmp(val, "inline-block") == 0) node->display = 3;
        }
        else if (strcmp(prop, "background-color") == 0 || strcmp(prop, "background") == 0) {
            node->bg_color = parse_color(val);
        }
        else if (strcmp(prop, "color") == 0) {
            node->text_color = parse_color(val);
        }
        else if (strcmp(prop, "margin") == 0) {
            int m = parse_px(val);
            for(int j=0; j<4; j++) node->margin[j] = m;
        }
        else if (strcmp(prop, "padding") == 0) {
            int p = parse_px(val);
            for(int j=0; j<4; j++) node->padding[j] = p;
        }
        else if (strcmp(prop, "border") == 0) {
            node->border[0] = parse_px(val);
            node->border[1] = node->border[0];
            node->border[2] = node->border[0];
            node->border[3] = node->border[0];
        }
        else if (strcmp(prop, "width") == 0) {
            fixed_width = parse_px(val);
        }
        else if (strcmp(prop, "height") == 0) {
            fixed_height = parse_px(val);
        }
    }
    
    if (fixed_width != -1) node->width = fixed_width; else node->width = 0;
    if (fixed_height != -1) node->height = fixed_height; else node->height = 0;
}

void calculate_layout(DOMNode* node, int x, int y, int available_width) {
    if (!node) return;
    
    if (strcmp(node->tag, "#text") == 0) {
        node->display = 2;
        node->x = x;
        node->y = y;
        
        int text_len = node->text ? str_len(node->text) : 0;
        int parent_font = node->parent ? node->parent->font_size : 16;
        int char_width = (parent_font / 2);
        
        node->computed_width = text_len * char_width;
        node->computed_height = parent_font + 4;
        
        if (node->computed_width > available_width) {
            node->computed_width = available_width;
            int lines = (text_len * char_width) / available_width + 1;
            node->computed_height = lines * (parent_font + 4);
        }
        node->needs_redraw = 1;
        return;
    }

    apply_styles(node);

    if (node->display == 0) {
        node->computed_width = 0;
        node->computed_height = 0;
        return;
    }

    node->x = x + node->margin[3];
    node->y = y + node->margin[0];
    
    int content_width = available_width - node->margin[1] - node->margin[3];
    if (node->width > 0 && node->width < content_width) {
        content_width = node->width;
    }
    
    node->computed_width = content_width;
    
    int inner_width = content_width - node->padding[1] - node->padding[3] - node->border[1] - node->border[3];
    int current_y = node->padding[0] + node->border[0];
    
    int current_inline_x = node->padding[3] + node->border[3];
    int max_inline_h = 0;
    int in_inline_context = 0;

    for (int i = 0; i < node->child_count; i++) {
        DOMNode* child = node->children[i];
        
        if (child->display == 0) continue;
        
        int is_child_inline = (strcmp(child->tag, "#text") == 0) || (child->display == 2) || (child->display == 3);
        
        if (is_child_inline) {
            in_inline_context = 1;
            calculate_layout(child, node->x + current_inline_x, node->y + current_y, inner_width - current_inline_x);
            
            if (current_inline_x + child->computed_width > inner_width && current_inline_x > node->padding[3] + node->border[3]) {
                current_y += max_inline_h;
                current_inline_x = node->padding[3] + node->border[3];
                max_inline_h = 0;
                calculate_layout(child, node->x + current_inline_x, node->y + current_y, inner_width);
            }
            
            current_inline_x += child->computed_width + child->margin[1] + child->margin[3];
            int ch_total_h = child->computed_height + child->margin[0] + child->margin[2];
            if (ch_total_h > max_inline_h) max_inline_h = ch_total_h;
            
        } else {
            if (in_inline_context) {
                current_y += max_inline_h;
                current_inline_x = node->padding[3] + node->border[3];
                max_inline_h = 0;
                in_inline_context = 0;
            }
            
            calculate_layout(child, node->x + node->padding[3] + node->border[3], node->y + current_y, inner_width);
            current_y += child->computed_height + child->margin[0] + child->margin[2];
        }
    }
    
    if (in_inline_context) {
        current_y += max_inline_h;
    }
    
    current_y += node->padding[2] + node->border[2];
    
    if (node->height > 0) {
        node->computed_height = node->height;
    } else {
        node->computed_height = current_y;
    }
    
    node->needs_redraw = 1;
}