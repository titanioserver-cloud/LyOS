#include "../include/css.h"

static int isspace(char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static int isalpha(char c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

static int isalnum(char c) {
    return (isalpha(c) || (c >= '0' && c <= '9'));
}

static void trim(char* str) {
    if (!str || !*str) return;
    int start = 0;
    while (str[start] && isspace(str[start])) start++;
    int end = str_len(str) - 1;
    while (end > start && isspace(str[end])) end--;
    int i;
    for (i = 0; i <= end - start; i++) {
        str[i] = str[start + i];
    }
    str[i] = '\0';
}

static char* strdup_css(const char* src) {
    if (!src) return 0;
    int len = str_len(src);
    char* dest = (char*)malloc(len + 1);
    if (dest) strcpy(dest, src);
    return dest;
}

static void* realloc_css(void* ptr, size_t old_size, size_t new_size) {
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

static int calculate_specificity(const char* selector) {
    int score = 0;
    int i = 0;
    while (selector[i]) {
        if (selector[i] == '#') score += 1000;
        else if (selector[i] == '.') score += 100;
        else if (selector[i] == '[') score += 10;
        else if (isalpha(selector[i]) && (i == 0 || !isalnum(selector[i-1]))) score += 1;
        i++;
    }
    return score;
}

CSSRule* parse_css(const char* css, int* rule_count) {
    int max_rules = 128;
    CSSRule* rules = (CSSRule*)malloc(sizeof(CSSRule) * max_rules);
    *rule_count = 0;
    
    int pos = 0;
    int len = str_len(css);
    
    while (pos < len) {
        while (pos < len && isspace(css[pos])) pos++;
        if (pos >= len) break;
        
        if (pos + 1 < len && css[pos] == '/' && css[pos+1] == '*') {
            pos += 2;
            while (pos + 1 < len && !(css[pos] == '*' && css[pos+1] == '/')) pos++;
            pos += 2;
            continue;
        }
        
        char selector[256];
        int i = 0;
        while (pos < len && css[pos] != '{') {
            if (i < 255) selector[i++] = css[pos];
            pos++;
        }
        selector[i] = 0;
        trim(selector);
        
        if (pos >= len) break;
        pos++; 
        
        int max_decls = 32;
        StyleDeclaration* decls = (StyleDeclaration*)malloc(sizeof(StyleDeclaration) * max_decls);
        int decl_count = 0;
        
        while (pos < len && css[pos] != '}') {
            while (pos < len && isspace(css[pos])) pos++;
            if (css[pos] == '}') break;
            
            char prop[64];
            i = 0;
            while (pos < len && css[pos] != ':' && css[pos] != '}') {
                if (i < 63) prop[i++] = css[pos];
                pos++;
            }
            prop[i] = 0;
            trim(prop);
            
            if (css[pos] == ':') pos++;
            
            char value[256];
            i = 0;
            while (pos < len && css[pos] != ';' && css[pos] != '}') {
                if (i < 255) value[i++] = css[pos];
                pos++;
            }
            value[i] = 0;
            trim(value);
            
            if (css[pos] == ';') pos++;
            
            if (str_len(prop) > 0 && str_len(value) > 0 && decl_count < max_decls) {
                decls[decl_count].property = strdup_css(prop);
                decls[decl_count].value = strdup_css(value);
                decl_count++;
            }
        }
        
        if (pos < len && css[pos] == '}') pos++;
        
        if (decl_count > 0) {
            if (*rule_count >= max_rules) {
                int old_sz = sizeof(CSSRule) * max_rules;
                max_rules *= 2;
                rules = (CSSRule*)realloc_css(rules, old_sz, sizeof(CSSRule) * max_rules);
            }
            rules[*rule_count].selector = strdup_css(selector);
            rules[*rule_count].declarations = decls;
            rules[*rule_count].decl_count = decl_count;
            rules[*rule_count].specificity = calculate_specificity(selector);
            (*rule_count)++;
        } else {
            free(decls);
        }
    }
    
    return rules;
}

void free_css(CSSRule* rules, int rule_count) {
    if (!rules) return;
    for (int i = 0; i < rule_count; i++) {
        free(rules[i].selector);
        for (int j = 0; j < rules[i].decl_count; j++) {
            free(rules[i].declarations[j].property);
            free(rules[i].declarations[j].value);
        }
        free(rules[i].declarations);
    }
    free(rules);
}

static int matches_selector(DOMNode* node, const char* selector) {
    if (!node || !node->tag || !selector) return 0;
    
    if (selector[0] == '#') {
        if (node->id && strcmp(node->id, selector + 1) == 0) return 1;
        return 0;
    }
    
    if (selector[0] == '.') {
        for (int i = 0; i < node->class_count; i++) {
            if (strcmp(node->classes[i], selector + 1) == 0) return 1;
        }
        return 0;
    }
    
    if (strcmp(node->tag, selector) == 0) return 1;
    
    return 0;
}

static void apply_css_to_node(DOMNode* node, CSSRule* rules, int rule_count) {
    if (!node || strcmp(node->tag, "#text") == 0) return;
    
    for (int i = 0; i < rule_count; i++) {
        if (matches_selector(node, rules[i].selector)) {
            for (int j = 0; j < rules[i].decl_count; j++) {
                int exists = 0;
                for (int k = 0; k < node->style_count; k++) {
                    if (strcmp(node->styles[k].property, rules[i].declarations[j].property) == 0) {
                        free(node->styles[k].value);
                        node->styles[k].value = strdup_css(rules[i].declarations[j].value);
                        exists = 1;
                        break;
                    }
                }
                if (!exists) {
                    size_t old_sz = sizeof(StyleDeclaration) * node->style_count;
                    node->style_count++;
                    size_t new_sz = sizeof(StyleDeclaration) * node->style_count;
                    node->styles = (StyleDeclaration*)realloc_css(node->styles, old_sz, new_sz);
                    node->styles[node->style_count - 1].property = strdup_css(rules[i].declarations[j].property);
                    node->styles[node->style_count - 1].value = strdup_css(rules[i].declarations[j].value);
                }
            }
        }
    }
    
    for (int i = 0; i < node->child_count; i++) {
        apply_css_to_node(node->children[i], rules, rule_count);
    }
}

void apply_all_css(DOMNode* root, CSSRule* rules, int rule_count) {
    if (!root || !rules || rule_count == 0) return;
    apply_css_to_node(root, rules, rule_count);
}