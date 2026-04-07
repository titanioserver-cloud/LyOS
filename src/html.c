#include "../include/html.h"

static int isspace(char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static char tolower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = ptr;
    while (num--) *p++ = (unsigned char)value;
    return ptr;
}

static char* strdup(const char* src) {
    if (!src) return 0;
    int len = str_len(src);
    char* dest = (char*)malloc(len + 1);
    if (dest) strcpy(dest, src);
    return dest;
}

static void* realloc_ptr(void* ptr, size_t old_size, size_t new_size) {
    if (new_size == 0) { free(ptr); return 0; }
    if (!ptr) return malloc(new_size);
    void* new_ptr = malloc(new_size);
    if (!new_ptr) return 0;
    size_t copy_sz = old_size < new_size ? old_size : new_size;
    char* d = (char*)new_ptr;
    char* s = (char*)ptr;
    for (size_t i = 0; i < copy_sz; i++) d[i] = s[i];
    free(ptr);
    return new_ptr;
}

static int is_all_whitespace(const char* s) {
    if (!s) return 1;
    while (*s) {
        if (!isspace(*s)) return 0;
        s++;
    }
    return 1;
}

DOMNode* parse_html(const char* html, int len) {
    DOMNode* root = (DOMNode*)malloc(sizeof(DOMNode));
    memset(root, 0, sizeof(DOMNode));
    root->tag = strdup("html");
    
    int pos = 0;
    DOMNode* current = root;
    
    while (pos < len) {
        while (pos < len && isspace(html[pos])) pos++;
        if (pos >= len) break;
        
        if (html[pos] == '<') {
            pos++;
            
            if (html[pos] == '/') {
                pos++;
                while (pos < len && html[pos] != '>') pos++;
                if (current->parent) current = current->parent;
                if (pos < len) pos++;
                continue;
            }
            
            DOMNode* node = (DOMNode*)malloc(sizeof(DOMNode));
            memset(node, 0, sizeof(DOMNode));
            
            int i = 0;
            char tag[64];
            while (pos < len && !isspace(html[pos]) && html[pos] != '>' && html[pos] != '/') {
                if (i < 63) tag[i++] = tolower(html[pos]);
                pos++;
            }
            tag[i] = 0;
            node->tag = strdup(tag);
            
            node->attrs = (char**)malloc(sizeof(char*) * 16);
            node->values = (char**)malloc(sizeof(char*) * 16);
            node->attr_count = 0;
            
            while (pos < len && html[pos] != '>' && html[pos] != '/') {
                while (pos < len && isspace(html[pos])) pos++;
                if (html[pos] == '>' || html[pos] == '/') break;
                
                char attr[64];
                i = 0;
                while (pos < len && !isspace(html[pos]) && html[pos] != '=' && html[pos] != '>' && html[pos] != '/') {
                    if (i < 63) attr[i++] = tolower(html[pos]);
                    pos++;
                }
                attr[i] = 0;
                
                char value[512];
                value[0] = 0;
                
                if (html[pos] == '=') {
                    pos++;
                    char quote = html[pos];
                    if (quote == '"' || quote == '\'') {
                        pos++;
                        i = 0;
                        while (pos < len && html[pos] != quote) {
                            if (i < 511) value[i++] = html[pos];
                            pos++;
                        }
                        if (pos < len) pos++;
                    } else {
                        i = 0;
                        while (pos < len && !isspace(html[pos]) && html[pos] != '>' && html[pos] != '/') {
                            if (i < 511) value[i++] = html[pos];
                            pos++;
                        }
                    }
                    value[i] = 0;
                }
                
                if (node->attr_count < 16 && attr[0] != 0) {
                    node->attrs[node->attr_count] = strdup(attr);
                    node->values[node->attr_count] = strdup(value);
                    
                    if (strcmp(attr, "id") == 0) {
                        node->id = strdup(value);
                    } else if (strcmp(attr, "class") == 0) {
                        int v_start = 0, v_curr = 0;
                        while (value[v_curr]) {
                            while (value[v_curr] && isspace(value[v_curr])) v_curr++;
                            if (!value[v_curr]) break;
                            v_start = v_curr;
                            while (value[v_curr] && !isspace(value[v_curr])) v_curr++;
                            
                            char old = value[v_curr];
                            value[v_curr] = 0;
                            
                            size_t old_sz = sizeof(char*) * node->class_count;
                            node->class_count++;
                            size_t new_sz = sizeof(char*) * node->class_count;
                            
                            node->classes = (char**)realloc_ptr(node->classes, old_sz, new_sz);
                            node->classes[node->class_count - 1] = strdup(&value[v_start]);
                            
                            value[v_curr] = old;
                        }
                    } else if (strcmp(attr, "href") == 0) {
                        node->href = strdup(value);
                    } else if (strcmp(attr, "src") == 0) {
                        node->src = strdup(value);
                    }
                    node->attr_count++;
                }
            }
            
            int is_self_closing = 0;
            if (pos < len && html[pos] == '/') {
                is_self_closing = 1;
                pos++;
            }
            if (pos < len && html[pos] == '>') pos++;
            
            if (strcmp(tag, "img") == 0 || strcmp(tag, "br") == 0 || strcmp(tag, "hr") == 0 || strcmp(tag, "input") == 0) {
                is_self_closing = 1;
            }
            
            node->parent = current;
            size_t old_c_sz = sizeof(DOMNode*) * current->child_count;
            current->child_count++;
            size_t new_c_sz = sizeof(DOMNode*) * current->child_count;
            current->children = (DOMNode**)realloc_ptr(current->children, old_c_sz, new_c_sz);
            current->children[current->child_count - 1] = node;
            
            if (!is_self_closing) {
                current = node;
            }
            
        } else {
            char text[4096];
            int i = 0;
            while (pos < len && html[pos] != '<') {
                if (i < 4095) text[i++] = html[pos];
                pos++;
            }
            text[i] = 0;
            
            if (i > 0 && !is_all_whitespace(text)) {
                DOMNode* text_node = (DOMNode*)malloc(sizeof(DOMNode));
                memset(text_node, 0, sizeof(DOMNode));
                text_node->tag = strdup("#text");
                text_node->text = strdup(text);
                text_node->parent = current;
                
                size_t old_c_sz = sizeof(DOMNode*) * current->child_count;
                current->child_count++;
                size_t new_c_sz = sizeof(DOMNode*) * current->child_count;
                current->children = (DOMNode**)realloc_ptr(current->children, old_c_sz, new_c_sz);
                current->children[current->child_count - 1] = text_node;
            }
        }
    }
    
    return root;
}

void free_dom(DOMNode* node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) {
        free_dom(node->children[i]);
    }
    if (node->children) free(node->children);
    if (node->tag) free(node->tag);
    if (node->id) free(node->id);
    if (node->text) free(node->text);
    if (node->href) free(node->href);
    if (node->src) free(node->src);
    for (int i = 0; i < node->class_count; i++) free(node->classes[i]);
    if (node->classes) free(node->classes);
    for (int i = 0; i < node->attr_count; i++) {
        free(node->attrs[i]);
        free(node->values[i]);
    }
    if (node->attrs) free(node->attrs);
    if (node->values) free(node->values);
    if (node->styles) {
        for(int i=0; i < node->style_count; i++) {
            free(node->styles[i].property);
            free(node->styles[i].value);
        }
        free(node->styles);
    }
    free(node);
}