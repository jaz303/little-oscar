#include "pattern.h"

#include <string.h>

static int do_pattern_match(const char *pattern, const char *input);

/* Public Interface */

enum {
    S_START         = 0,
    S_OUT           = 1
};

int osc_pattern_verify(const char *pattern) {
    if (!pattern || *pattern != '/') return OSC_PATTERN_INVALID;
    
    int state       = S_START;
    int has_meta    = 0;
    
    const char  *p      = pattern + 1,
                *slash  = pattern;
    
    while (*p) {
        if (*p == '/') {
            if (p - slash == 1) return OSC_PATTERN_INVALID;
            slash = p;
        } else if (*p == '[') {
            has_meta = 1;
            return OSC_PATTERN_INVALID; /* unsupported */
        } else if (*p == '{') {
            has_meta = 1;
            p++;
            int seg_len = 0;
            while (*p) {
                if (*p == '/' || *p == '#' || *p == ']' || *p == ' ' || *p == '*' || *p == '?') {
                    return OSC_PATTERN_INVALID;
                } else if (*p == ',') {
                    if (seg_len == 0) return OSC_PATTERN_INVALID;
                    seg_len = 0;
                    p++;
                } else if (*p == '}') {
                    if (seg_len == 0) return OSC_PATTERN_INVALID;
                    break;
                } else {
                    seg_len++;
                    p++;
                }
            }
            if (!*p) return OSC_PATTERN_INVALID;
        } else if (*p == '*' || *p == '?') {
            has_meta = 1;
        } else if (*p == '#' || *p == ']' || *p == '}' || *p == ',' || *p == ' ') {
            return OSC_PATTERN_INVALID;
        }
        p++;
    }
    
    return (*(p-1) == '/')
            ? OSC_PATTERN_INVALID
            : (has_meta ? OSC_PATTERN_DYNAMIC : OSC_PATTERN_STATIC);
}

int osc_pattern_compile(osc_pattern_t *pattern_out, const char *pattern_in) {
    int result = osc_pattern_verify(pattern_in);
    if (!result) {
        return 0;
    } else {
        pattern_out->is_static = (result == OSC_PATTERN_STATIC);
        pattern_out->pattern = pattern_in;
        return 1;
    }
}

int osc_pattern_match(osc_pattern_t *pattern, const char *input) {
    if (pattern->is_static) {
        return strcmp(pattern->pattern, input) == 0;
    } else {
        return do_pattern_match(pattern->pattern, input);
    }
}

/* ... */

static int do_pattern_match(const char *pattern, const char *input) {
    if (*input != '/') return 0;
    
    const char  *p      = pattern + 1,
                *i      = input + 1,
                *slash  = input;
                
    while (*p && *i) {
        
        if (*i == '/') {
            if (i - slash == 1) return 0;
            slash = i;
            if (*p == '*') p++;
            if (*p != '/') return 0;
            p++; i++;
            continue;
        }
        
        switch (*p) {
            case '?':
                if (*i == '/') {
                    return 0;
                } else {
                    p++; i++;
                    continue;
                }
            case '*':
                while (*i && *i != '/') i++;
                p++;
                continue;
            case '[':
                return 0;
            case '{':
            {
                const char *mark = i;
                
                p++;
                while (1) {
                    if (*p == *i) {
                        p++; i++;
                        if (*p == ',') {
                            p++;
                            while ((*p) != '}') p++;
                            p++;
                            break;
                        } else if (*p == '}') {
                            p++;
                            break;
                        }
                    } else {
                        i = mark;
                        while (*p != ',' && *p != '}') p++;
                        if (*p == ',') {
                            p++;
                        } else {
                            return 0;
                        }
                    }
                }
                continue;
            }
            default:
                if (*p != *i) {
                    return 0;
                } else {
                    p++; i++;
                    continue;
                }
        }
    }
    
    if (*p == '*') p++;
    
    return !(*p) && !(*i);
}
