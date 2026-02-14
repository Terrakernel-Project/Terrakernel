#ifndef TC_STRING_H
#define TC_STRING_H 1
#define STRING_H 1
#include <mem/mem.hpp>
#include <stddef.h>

static inline size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

static inline char *strcpy(char *__restrict dst, const char *__restrict src) {
    char *ret = dst;
    while ((*dst++ = *src++));
    return ret;
}

static inline char *strncpy(char *__restrict dst, const char *__restrict src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

static inline int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(unsigned char *)a - *(unsigned char *)b;
}

static inline int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == '\0') return (unsigned char)a[i] - (unsigned char)b[i];
    }
    return 0;
}

static inline int strcasecmp(const char* a, const char* b) {
    while (*a && *b) {
        char c1 = *a;
        char c2 = *b;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return (unsigned char)c1 - (unsigned char)c2;
        a++;
        b++;
    }
    char c1 = *a;
    char c2 = *b;
    if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
    if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
    return (unsigned char)c1 - (unsigned char)c2;
}

static inline int strncasecmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c1 = a[i];
        char c2 = b[i];
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2 || c1 == '\0') return (unsigned char)c1 - (unsigned char)c2;
    }
    return 0;
}

static inline char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : NULL;
}

static inline char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    return (c == '\0') ? (char *)s : (char *)last;
}

static inline char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && (*h == *n)) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

static inline size_t strnlen(const char *s, size_t max) {
    size_t len = 0;
    while (len < max && s[len]) len++;
    return len;
}

static inline char* strdup(char* src) {
    char* new_string = (char*)mem::heap::malloc(strlen(src) + 1);
    strcpy(new_string, src);
    return new_string;
}

static inline char *strcat(char *dst, const char *src) {
    char *ret = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++));
    return ret;
}

static inline char* strtok(char* str, const char* delim) {
    static char* next_token = nullptr;
    
    if (str) {
        next_token = str;
    } else {
        str = next_token;
    }
    
    if (!str || !*str) {
        return nullptr;
    }
    
    while (*str) {
        bool is_delim = false;
        for (const char* d = delim; *d; d++) {
            if (*str == *d) {
                is_delim = true;
                break;
            }
        }
        if (!is_delim) break;
        str++;
    }
    
    if (!*str) {
        next_token = nullptr;
        return nullptr;
    }
    
    char* token_start = str;
    
    while (*str) {
        bool is_delim = false;
        for (const char* d = delim; *d; d++) {
            if (*str == *d) {
                is_delim = true;
                break;
            }
        }
        if (is_delim) {
            *str = '\0';
            next_token = str + 1;
            return token_start;
        }
        str++;
    }
    
    next_token = nullptr;
    return token_start;
}

#endif
