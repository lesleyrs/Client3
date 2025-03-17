#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <strings.h>
#endif
#include <stdarg.h>
#include <string.h>

#include "platform.h"

#if SDL == 3
#include "SDL3/SDL.h"
#endif
#if SDL == 2
#include "SDL.h"
#endif

void rs2_log(const char *format, ...) {
    va_list args;
    va_start(args, format);

#if SDL > 1
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, format,
                    args);
#else
    vprintf(format, args);
#endif

    va_end(args);
}

void rs2_error(const char *format, ...) {
    va_list args;
    va_start(args, format);

#if SDL > 1
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR,
                    format, args);
#else
    vfprintf(stderr, format, args);
#endif

    va_end(args);
}

// Java Math.random, rand requires + 1 to never reach 1 else it'll overflow on update_flame_buffer
// TODO: maybe float version for better perf?
double jrand(void) {
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

// Java indexOf
int indexof(const char *str, const char *str2) {
    const char *pos = strstr(str, str2);

    if (pos) {
        return (int)(pos - str);
    }

    return -1;
}

// Java substring
// NOTE: should this return a ptr instead of copy?
char *substring(const char *src, size_t start, size_t length) {
    size_t len = length - start;
    char *sub = malloc(len + 1);

    strncpy(sub, src + start, len);
    sub[len] = '\0';
    return sub;
}

// Java valueOf
char *valueof(int value) {
    char *str = malloc(12 * sizeof(char));
    sprintf(str, "%d", value);
    return str;
}

// Java startsWith
bool strstartswith(const char *str, const char *prefix) {
    size_t len = strlen(prefix);
    if (len > strlen(str)) {
        return false;
    }

    return strncmp(str, prefix, len) == 0;
}

// Java endsWith
bool strendswith(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) {
        return false;
    }

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

// Java equalsIgnoreCase
int platform_strcasecmp(const char *str1, const char *str2) {
// NOTE make non platform specific one? we undef _WIN32 in makefile if xbox
#if defined(_WIN32) || defined(NXDK)
    return _stricmp(str1, str2);
#else
    return strcasecmp(str1, str2);
#endif
}

// Java toLowerCase
void strtolower(char *s) {
    while (*s) {
        *s = tolower((unsigned char)*s);
        s++;
    }
}

// Java toUpperCase
void strtoupper(char *s) {
    while (*s) {
        *s = toupper((unsigned char)*s);
        s++;
    }
}

// Java trim, but doesn't return a string
void strtrim(char *s) {
    unsigned char *p = (unsigned char *)s;
    int l = (int)strlen(s);

    while (isspace(p[l - 1])) {
        p[--l] = 0;
    }

    while (*p && isspace(*p)) {
        ++p;
        --l;
    }

    memmove(s, p, l + 1);
}

/* int platform_asprintf(char **str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(NULL, 0, fmt, args);
    *str = malloc(len + 1);
    va_end(args);
    va_start(args, fmt);
    vsnprintf(*str, len + 1, fmt, args);
    va_end(args);
    return len;
} */

char *platform_strdup(const char *s) {
    // #if defined(MODERN_POSIX) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
    //     /* strdup is defined in POSIX rather than ISO C */
    //     return strdup(s);
    // #else
    size_t len = strlen(s);
    char *n = malloc(len + 1);
    if (n) {
        memcpy(n, s, len);
        n[len] = '\0';
    }
    return n;
    // #endif
}

char *platform_strndup(const char *s, size_t len) {
    // #if defined(MODERN_POSIX) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
    //     /* strdup is defined in POSIX rather than ISO C */
    //     return strndup(s, len);
    // #else
    char *n = malloc(len + 1);
    if (n) {
        memcpy(n, s, len);
        n[len] = '\0';
    }
    return n;
    // #endif
}
