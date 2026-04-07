#ifndef HTML_H
#define HTML_H

#include "../lib/protos.h"

typedef struct StyleDeclaration {
    char* property;
    char* value;
} StyleDeclaration;

typedef struct CSSRule {
    char* selector;
    StyleDeclaration* declarations;
    int decl_count;
    int specificity;
} CSSRule;

typedef struct DOMNode {
    char* tag;
    char* id;
    char** classes;
    int class_count;
    
    char** attrs;
    char** values;
    int attr_count;
    
    char* text;
    struct DOMNode** children;
    int child_count;
    struct DOMNode* parent;
    
    int x, y, width, height;
    int computed_width, computed_height;
    int margin[4];
    int padding[4];
    int border[4];
    
    StyleDeclaration* styles;
    int style_count;
    
    uint32_t bg_color;
    uint32_t text_color;
    int font_size;
    int display;
    int position;
    
    char* href;
    char* src;
    uint32_t* image_data;
    int img_width, img_height;
    
    int needs_redraw;
} DOMNode;

DOMNode* parse_html(const char* html, int len);
void free_dom(DOMNode* node);

#endif