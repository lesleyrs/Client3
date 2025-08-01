#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(SDL) && SDL > 0
typedef struct SDL_Surface Surface;
#else
typedef struct Surface {
    int *pixels;
} Surface;
#endif

#if defined(_WIN32) || defined(ANDROID)            // add other systems that use sdl_main? SDL3 replaced sdl_main with a header
#if defined(SDL) && SDL < 3 && !defined(__TINYC__) // tcc can't use .lib files but doesn't require SDL include to use subsystem:windows
#include "SDL.h"
#endif
#endif

typedef struct Client Client;
typedef struct GameShell GameShell;
typedef struct PixMap PixMap;

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

// TODO rm temp func
#if defined(__WII__) || defined(__PSP__)
int get_free_mem(void);
#endif

Surface *platform_create_surface(int *pixels, int width, int height, int alpha);
void platform_free_surface(Surface *surface);
void rs2_log(const char *format, ...);
void rs2_error(const char *format, ...);
char *platform_strndup(const char *s, size_t len);
char *platform_strdup(const char *s);
// int platform_asprintf(char **str, const char *fmt, ...);
int platform_strcasecmp(const char *_l, const char *_r);
void strtrim(char *s);
void strtolower(char *s);
void strtoupper(char *s);
bool strendswith(const char *str, const char *suffix);
bool strstartswith(const char *str, const char *prefix);
char *valueof(int value);
int indexof(const char *str, const char *str2);
char *substring(const char *src, size_t start, size_t length);
double jrand(void);

bool platform_init(void);
void platform_new(GameShell *shell);
void platform_free(void);
void platform_set_wave_volume(int wavevol);
void platform_play_wave(int8_t *src, int length);
void platform_set_midi_volume(float midivol);
void platform_set_jingle(int8_t *src, int len);
void platform_set_midi(const char *name, int crc, int len);
void platform_stop_midi(void);
void set_pixels(PixMap *pixmap, int x, int y);
void platform_poll_events(Client *c);
#if (defined(__EMSCRIPTEN__) && (!defined(SDL) || SDL == 0)) || defined(__wasm) && !defined(__EMSCRIPTEN__)
void platform_draw_string(const char *str, int x, int y, int color, bool bold, int size);
#endif
void platform_blit_surface(int x, int y, int w, int h, Surface *surface);
void platform_update_surface(void);
void platform_draw_rect(int x, int y, int w, int h, int color);
void platform_fill_rect(int x, int y, int w, int h, int color);
uint64_t rs2_now(void);
void rs2_sleep(int ms);
