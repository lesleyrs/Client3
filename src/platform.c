#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #ifndef _WIN32
// #include <strings.h>
// #endif

#include "defines.h"
#include "platform.h"

#if SDL == 3
#include "SDL3/SDL.h"
#endif
#if SDL == 2 || SDL == 1
#include "SDL.h"
#endif

void platform_set_pixels(uint32_t *restrict dst, Surface *restrict surface, int x, int y, bool argb) {
    if (!dst) return;

    for (int row = 0; row < surface->h; row++) {
        for (int col = 0; col < surface->w; col++) {
            uint32_t pixel = ((uint32_t*)surface->pixels)[row * surface->w + col];
            // TODO add screen limits?
            if (argb) {
#ifdef __wasm
                pixel = ((pixel >> 16) & 0xff) | (pixel & 0xff00) | ((pixel & 0xff) << 16) | 0xff000000;
#else
                pixel = ((pixel >> 16) & 0xff) | (pixel & 0xff00) | ((pixel & 0xff) << 16);
#endif
            }
            dst[(y + row) * SCREEN_FB_WIDTH + (x + col)] = pixel;
        }
    }
}

#ifndef __wasm
void platform_draw_rect(int x, int y, int w, int h, int color) {
    int *pixels = calloc(w * h, sizeof(int));
    Surface *rect = platform_create_surface(pixels, w, h, false);

    for (int i = 0; i < w; i++) {
        pixels[i] = color;                 // top
        pixels[((h - 1) * w) + i] = color; // bottom
    }

    for (int i = 0; i < h; i++) {
        pixels[i * w] = color;         // left
        pixels[i * w + w - 1] = color; // right
    }

    platform_blit_surface(rect, x, y);
    platform_free_surface(rect);
    free(pixels);
}

void platform_fill_rect(int x, int y, int w, int h, int color) {
    int *pixels = calloc(w * h, sizeof(int));
    Surface *rect = platform_create_surface(pixels, w, h, false);

    for (int j = 0; j < h; j++) {
        int *row = pixels + j * w;
        for (int i = 0; i < w; i++) {
            row[i] = color;
        }
    }

    platform_blit_surface(rect, x, y);
    platform_free_surface(rect);
    free(pixels);
}

#define STB_TRUETYPE_IMPLEMENTATION
#include "thirdparty/stb_truetype.h"

static int ttf_string_width(stbtt_fontinfo *font, const char *message, float scale) {
    int string_width = 0;
    while (*message) {
        int advanceWidth, leftSideBearing;
        stbtt_GetCodepointHMetrics(font, *message, &advanceWidth, &leftSideBearing);
        string_width += (int)(advanceWidth * scale);
        message++;
    }
    return string_width;
}

void platform_draw_string(const char *str, int x, int y, int color, bool bold, int size) {
    (void)bold; // TODO: add regular fonts for mapview too
#ifdef ANDROID
    SDL_RWops *file = NULL;
#else
    FILE *file = NULL;
#endif
#if defined(_WIN32)
    // c:/windows/fonts/ - arialbd
    file = fopen("c:/windows/fonts/arialbd.ttf", "rb");
#elif defined(__linux__) && !defined(ANDROID)
    // /usr/share/fonts - dejavu/liberation sans
    // FILE *file = fopen("/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf", "rb");
    file = fopen("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "rb");
#elif defined(__FreeBSD__)
    file = fopen("/usr/local/share/fonts/dejavu/DejaVuSans-Bold.ttf", "rb");
#elif defined(__APPLE__) && defined(__MACH__)
// /system/library/fonts/ - helvetica neue
#endif

    if (!file) {
#ifdef _WIN32
        // fallback for windows 2000 and windows NT4
        file = fopen("c:/winnt/fonts/arialbd.ttf", "rb");
#elif NXDK
        file = fopen("D:\\Roboto\\Roboto-Bold.ttf", "rb");
#elif ANDROID
        file = SDL_RWFromFile("Roboto/Roboto-Bold.ttf", "rb");
#else
        file = fopen("Roboto/Roboto-Bold.ttf", "rb");
#endif
        if (!file) {
#ifdef _WIN32
            file = fopen("c:/reactos/fonts/arialbd.ttf", "rb");
#endif
            if (!file) {
                // last try for desktop without system fonts found, where /rom isn't root in romfs
                file = fopen("rom/Roboto/Roboto-Bold.ttf", "rb");
                if (!file) {
                    // TODO: won't show errors on screen if no font, try use native text drawing for all platforms if no system font or Roboto
                    rs2_error("Failed to open font file\n");
                    return;
                }
            }
        }
    }

#ifdef ANDROID
    size_t sz = SDL_RWseek(file, 0, RW_SEEK_END);
    SDL_RWseek(file, 0, RW_SEEK_SET);
#else
    fseek(file, 0, SEEK_END);
    size_t sz = ftell(file);
    fseek(file, 0, SEEK_SET);
#endif

    uint8_t *buffer = malloc(sz);
#ifdef ANDROID
    if (SDL_RWread(file, buffer, 1, sz) != sz) {
#else
    if (fread(buffer, 1, sz, file) != sz) {
#endif
        rs2_error("Failed to read file\n", strerror(errno));
    }
#ifdef ANDROID
    SDL_RWclose(file);
#else
    fclose(file);
#endif

    stbtt_fontinfo font;
    stbtt_InitFont(&font, buffer, stbtt_GetFontOffsetForIndex(buffer, 0));

    float scale = stbtt_ScaleForPixelHeight(&font, (float)size);
    bool centered = x == SCREEN_WIDTH / 2;
    if (centered) {
        // TODO: is this centering correct? maybe few pixels to the left?
        x = (SCREEN_WIDTH - ttf_string_width(&font, str, scale)) / 2;
    }

    float xpos = 0;
    int ch = 0;
    // TODO: this could probably be improved a lot by drawing text in one go
    while (str[ch]) {
        int advance, lsb, x0, y0, x1, y1;
        float x_shift = xpos - floorf(xpos);

        stbtt_GetCodepointHMetrics(&font, str[ch], &advance, &lsb);
        stbtt_GetCodepointBitmapBoxSubpixel(&font, str[ch], scale, scale, x_shift, 0, &x0, &y0, &x1, &y1);

        int width = x1 - x0;
        int height = y1 - y0;
        unsigned char *bitmap = stbtt_GetCodepointBitmapSubpixel(&font, scale, scale, x_shift, 0, str[ch], &width, &height, &x0, &y0);

        int *pixels = malloc(width * height * sizeof(int));
        Surface *surface = platform_create_surface(pixels, width, height, 0xff000000);

        for (int i = 0; i < width * height; i++) {
            unsigned char value = bitmap[i];
            pixels[i] = (value << 24) | (value << 16) | (value << 8) | value;
            pixels[i] = (pixels[i] & 0xff000000) | (pixels[i] & color);
        }

        // NOTE this inaccurately draws 1 character per surface, noticeable in sdl
        platform_blit_surface(surface, x + (int)xpos + x0, y + y0);

        stbtt_FreeBitmap(bitmap, NULL);
        platform_free_surface(surface);
        free(pixels);

        xpos += advance * scale;
        if (str[ch + 1]) {
            xpos += scale * stbtt_GetCodepointKernAdvance(&font, str[ch], str[ch + 1]);
        }

        ++ch;
    }

    free(buffer);
}
#endif

Surface *platform_create_surface(int *pixels, int width, int height, int alpha) {
#if SDL == 3
    return SDL_CreateSurfaceFrom(width, height, SDL_GetPixelFormatForMasks(32, 0xff0000, 0x00ff00, 0x0000ff, alpha), pixels, width * sizeof(int));
#elif SDL == 2 || SDL == 1
    return SDL_CreateRGBSurfaceFrom(pixels, width, height, 32, width * sizeof(int), 0xff0000, 0x00ff00, 0x0000ff, alpha);
#else
    (void)alpha;
    Surface *surface = calloc(1, sizeof(Surface));
    surface->pixels = pixels;
    surface->w = width;
    surface->h = height;
    return surface;
#endif
}

void platform_free_surface(Surface *surface) {
#if SDL == 3
    SDL_DestroySurface(surface);
#elif SDL == 2 || SDL == 1
    SDL_FreeSurface(surface);
#else
    free(surface);
#endif
}

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
#ifdef USE_FLOATS
double jrand(void) {
    return (float)rand() / ((float)RAND_MAX + 1.0);
}
#else
double jrand(void) {
    return (double)rand() / ((double)RAND_MAX + 1.0);
}
#endif

// Java indexOf
int indexof(const char *str, const char *str2) {
    const char *pos = strstr(str, str2);

    if (pos) {
        return (int)(pos - str);
    }

    return -1;
}

// Java substring
// TODO: should this return a ptr instead of copy?
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
int platform_strcasecmp(const char *_l, const char *_r) {
#if defined(_WIN32) || defined(NXDK)
    return _stricmp(_l, _r);
#else
    // return strcasecmp(_l, _r);
    // from musl
    const unsigned char *l = (void *)_l, *r = (void *)_r;
    for (; *l && *r && (*l == *r || tolower(*l) == tolower(*r)); l++, r++)
        ;
    return tolower(*l) - tolower(*r);
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
