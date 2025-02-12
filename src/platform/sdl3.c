#if SDL == 3
#include "SDL3/SDL.h"
#include <stdio.h>
#include <string.h>

#include "../client.h"
#include "../defines.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"

extern InputTracking _InputTracking;

void platform_new(GameShell *shell, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        rs2_error("SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    shell->window = SDL_CreateWindow("Jagex", width, height, 0);
    if (!shell->window) {
        rs2_error("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }

    shell->surface = SDL_GetWindowSurface(shell->window);
    if (!shell->surface) {
        rs2_error("Window surface creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(shell->window);
        SDL_Quit();
        return;
    }

    // SDL_Surface renderer = SDL_CreateSoftwareRenderer()
    // SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    // if (!renderer) {
    //     rs2_error("Renderer creation failed: %s\n", SDL_GetError());
    //     SDL_DestroyWindow(window);
    //     SDL_Quit();
    //     return;
    // }
}

void platform_free(GameShell *shell) {
    // SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(shell->window);
    SDL_Quit();
}

void platform_play_wave(int8_t *src, int length) {
}

void platform_set_wave_volume(int wavevol) {
}

void platform_set_midi_volume(float midivol) {
}

void platform_set_jingle(int8_t *src, int len) {
}

void platform_set_midi(const char *name, int crc, int len) {
}

void platform_stop_midi(void) {
}

Surface *platform_create_surface(int *pixels, int width, int height, int alpha) {
    return SDL_CreateSurfaceFrom(width, height, SDL_GetPixelFormatForMasks(32, 0xff0000, 0x00ff00, 0x0000ff, alpha), pixels, width * sizeof(int));
}

void platform_free_surface(Surface *surface) {
    SDL_DestroySurface(surface);
}

int *get_pixels(Surface *surface) {
    return surface->pixels;
}

void set_pixels(PixMap *pixmap, int x, int y) {
    SDL_Rect dest = {x, y, pixmap->width, pixmap->height};
    SDL_BlitSurfaceScaled(pixmap->image, NULL, pixmap->shell->surface, &dest, SDL_SCALEMODE_NEAREST);
    // SDL_BlitSurfaceScaled(pixmap->image, NULL, pixmap->shell->surface, NULL, SDL_SCALEMODE_NEAREST);
    // SDL_BlitSurface(pixmap->image, NULL, pixmap->shell->surface, NULL);
    SDL_UpdateWindowSurface(pixmap->shell->window);
}

void platform_blit_surface(GameShell *shell, int x, int y, int w, int h, Surface *surface) {
    SDL_Rect rect = {x, y, w, h};
    SDL_BlitSurfaceScaled(surface, NULL, shell->surface, &rect, SDL_SCALEMODE_NEAREST);
    // SDL_BlitSurface(surface, NULL, shell->surface, &rect);
}

void platform_update_surface(GameShell *shell) {
    SDL_UpdateWindowSurface(shell->window);
}

void platform_fill_rect(GameShell *shell, int x, int y, int w, int h, int color) {
    if (color != BLACK) { // TODO other grayscale?
        color = SDL_MapRGB(SDL_GetPixelFormatDetails(shell->surface->format), SDL_GetSurfacePalette(shell->surface), color >> 16 & 0xff, color >> 8 & 0xff, color & 0xff);
    }

    SDL_Rect rect = {x, y, w, h};
    SDL_FillSurfaceRect(shell->surface, &rect, color);
}

void platform_poll_events(Client *c) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
            gameshell_destroy(c);
            break;
            break;
        case SDL_EVENT_KEY_DOWN: {
            // char ch;
            // int code;
            // platform_get_keycodes(&e.key.keysym, &code, &ch);
            // key_pressed(c->shell, code, ch);
            break;
        }
        case SDL_EVENT_KEY_UP: {
            // char ch;
            // int code;
            // platform_get_keycodes(&e.key.keysym, &code, &ch);
            // key_released(c->shell, code, ch);
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            int x = e.motion.x;
            int y = e.motion.y;

            c->shell->idle_cycles = 0;
            c->shell->mouse_x = x;
            c->shell->mouse_y = y;

            if (_InputTracking.enabled) {
                inputtracking_mouse_moved(&_InputTracking, x, y);
            }
        } break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            int x = e.button.x;
            int y = e.button.y;

            c->shell->idle_cycles = 0;
            c->shell->mouse_click_x = x;
            c->shell->mouse_click_y = y;

            if (e.button.button == SDL_BUTTON_RIGHT) {
                c->shell->mouse_click_button = e.button.button;

                c->shell->mouse_click_button = 2;
                c->shell->mouse_button = 2;
            } else {
                c->shell->mouse_click_button = 1;
                c->shell->mouse_button = 1;
            }

            if (_InputTracking.enabled) {
                inputtracking_mouse_pressed(&_InputTracking, x, y, e.button.button == SDL_BUTTON_RIGHT ? 1 : 0);
            }
        } break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            c->shell->idle_cycles = 0;
            c->shell->mouse_button = 0;

            if (_InputTracking.enabled) {
                inputtracking_mouse_released(&_InputTracking, (e.button.button & SDL_BUTTON_RMASK) != 0 ? 1 : 0);
            }
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            c->shell->surface = SDL_GetWindowSurface(c->shell->window);
            if (!c->shell->surface) {
                rs2_error("Window surface creation failed: %s\n", SDL_GetError());
                SDL_DestroyWindow(c->shell->window);
                SDL_Quit();
                return;
            }
            c->redraw_background = true;
            break;
        case SDL_EVENT_WINDOW_MOUSE_ENTER:
            if (_InputTracking.enabled) {
                inputtracking_mouse_entered(&_InputTracking);
            }
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            if (_InputTracking.enabled) {
                inputtracking_mouse_exited(&_InputTracking);
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            c->shell->has_focus = true; // mapview applet
            c->shell->refresh = true;
#ifdef client
            c->redraw_background = true;
#endif
#ifdef mapview
// TODO add mapview refresh
#endif

            if (_InputTracking.enabled) {
                inputtracking_focus_gained(&_InputTracking);
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            c->shell->has_focus = false; // mapview applet
            if (_InputTracking.enabled) {
                inputtracking_focus_lost(&_InputTracking);
            }
            break;
        }
    }
}

int64_t get_ticks(void) {
    return SDL_GetTicks();
}

void delay_ticks(int ticks) {
    SDL_Delay(ticks);
}

void rs2_log(const char *format, ...) {
    va_list args;
    va_start(args, format);

#if !defined(_3DS) && !defined(WII) && !defined(SDL12)
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

#if !defined(_3DS) && !defined(WII) && !defined(SDL12)
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR,
                    format, args);
#else
    vfprintf(stderr, format, args);
#endif

    va_end(args);
}
#endif
