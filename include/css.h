#ifndef CSS_H
#define CSS_H

#include "html.h"

CSSRule* parse_css(const char* css, int* rule_count);
void free_css(CSSRule* rules, int rule_count);
void apply_all_css(DOMNode* root, CSSRule* rules, int rule_count);

#endif