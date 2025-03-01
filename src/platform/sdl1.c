#if SDL == 1
#include "SDL.h"
#include <stdio.h>
#include <string.h>

#include "../client.h"
#include "../defines.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"

extern InputTracking _InputTracking;

void platform_init(void) {
}

void platform_new(GameShell *shell, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        rs2_error("SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    // NOTE: input isn't compatible with sdl2 :(
    // (void)SDL_EnableUNICODE(1);
    // SDL_EnableUNICODE(1);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
    SDL_WM_SetCaption("Jagex", NULL);
    // shell->surface = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE | SDL_RESIZABLE);
    shell->surface = SDL_SetVideoMode(width, height, 32, SDL_SWSURFACE);
}

void platform_free(GameShell *shell) {
    (void)shell;
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
    return SDL_CreateRGBSurfaceFrom(pixels, width, height, 32, width * sizeof(int), 0xff0000, 0x00ff00, 0x0000ff, alpha);
}

void platform_free_surface(Surface *surface) {
    SDL_FreeSurface(surface);
}

int *get_pixels(Surface *surface) {
    return surface->pixels;
}

void set_pixels(PixMap *pixmap, int x, int y) {
    SDL_Rect dest = {x, y, pixmap->width, pixmap->height};
    SDL_BlitSurface(pixmap->image, NULL, pixmap->shell->surface, &dest);
    // SDL_BlitSurface(pixmap->image, NULL, pixmap->shell->surface, NULL);
    SDL_Flip(pixmap->shell->surface);
}

void platform_blit_surface(GameShell *shell, int x, int y, int w, int h, Surface *surface) {
    SDL_Rect rect = {x, y, w, h};
    SDL_BlitSurface(surface, NULL, shell->surface, &rect);
}

void platform_update_surface(GameShell *shell) {
    SDL_Flip(shell->surface);
}

void platform_fill_rect(GameShell *shell, int x, int y, int w, int h, int color) {
    if (color != BLACK) { // TODO other grayscale?
        color = SDL_MapRGB(shell->surface->format, color >> 16 & 0xff, color >> 8 & 0xff, color & 0xff);
    }

    SDL_Rect rect = {x, y, w, h};
    SDL_FillRect(shell->surface, &rect, color);
}

void platform_get_keycodes(SDL_keysym *keysym, int *code, char *ch) {
    *code = -1;
    *ch = -1;

    /* note: unicode is not set for key released */
    if (keysym->unicode > 0 && keysym->unicode < 128) {
        if (isprint((unsigned char)keysym->unicode)) {
            *code = keysym->unicode;
            *ch = keysym->unicode;
            return;
        }
    }

    switch (keysym->sym) {
    case SDLK_TAB:
        *code = K_TAB;
        *ch = '\t';
        break;
    case SDLK_BACKSPACE:
        *code = K_BACKSPACE;
        *ch = '\b';
        break;
    case SDLK_LEFT:
        *code = K_LEFT;
        break;
    case SDLK_RIGHT:
        *code = K_RIGHT;
        break;
    case SDLK_UP:
        *code = K_UP;
        break;
    case SDLK_DOWN:
        *code = K_DOWN;
        break;
    case SDLK_PAGEUP:
        *code = K_PAGE_UP;
        break;
    case SDLK_PAGEDOWN:
        *code = K_PAGE_DOWN;
        break;
    case SDLK_HOME:
        *code = K_HOME;
        break;
    case SDLK_F1:
        *code = K_F1;
        break;
    case SDLK_ESCAPE:
        *code = K_ESCAPE;
        break;
    case SDLK_RETURN:
        *code = K_ENTER;
        *ch = '\r';
        break;
    case SDLK_KP1:
    case SDLK_1:
        *code = K_1;
        *ch = K_1;
        break;
    case SDLK_KP2:
    case SDLK_2:
        *code = K_2;
        *ch = K_2;
        break;
    case SDLK_KP3:
    case SDLK_3:
        *code = K_3;
        *ch = K_3;
        break;
    case SDLK_KP4:
    case SDLK_4:
        *code = K_4;
        *ch = K_4;
        break;
    case SDLK_KP5:
    case SDLK_5:
        *code = K_5;
        *ch = K_5;
        break;
    default:
        break;
    }
}

void platform_poll_events(Client *c) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            gameshell_destroy(c);
            break;
        case SDL_KEYDOWN: {
            char ch;
            int code;
            platform_get_keycodes(&e.key.keysym, &code, &ch);
            key_pressed(c->shell, code, ch);
            break;
        }
        case SDL_KEYUP: {
            char ch;
            int code;
            platform_get_keycodes(&e.key.keysym, &code, &ch);
            key_released(c->shell, code, ch);
            break;
        }
        case SDL_MOUSEMOTION: {
            int x = e.motion.x;
            int y = e.motion.y;

            c->shell->idle_cycles = 0;
            c->shell->mouse_x = x;
            c->shell->mouse_y = y;

            if (_InputTracking.enabled) {
                inputtracking_mouse_moved(&_InputTracking, x, y);
            }
        } break;
        case SDL_MOUSEBUTTONDOWN: {
            int x = e.button.x;
            int y = e.button.y;

            c->shell->idle_cycles = 0;
            c->shell->mouse_click_x = x;
            c->shell->mouse_click_y = y;

            if (e.button.button == SDL_BUTTON_RIGHT) {
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
        case SDL_MOUSEBUTTONUP:
            c->shell->idle_cycles = 0;
            c->shell->mouse_button = 0;

            if (_InputTracking.enabled) {
                inputtracking_mouse_released(&_InputTracking, (e.button.button & SDL_BUTTON_RMASK) != 0 ? 1 : 0);
            }
            break;
        case SDL_ACTIVEEVENT:
            if (e.active.state & SDL_APPMOUSEFOCUS) {
                if (e.active.gain) {
                    if (_InputTracking.enabled) {
                        inputtracking_mouse_entered(&_InputTracking);
                    }
                } else {
                    if (_InputTracking.enabled) {
                        inputtracking_mouse_exited(&_InputTracking);
                    }
                }
            }
            if (e.active.state & SDL_APPINPUTFOCUS) {
                if (e.active.gain) {
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
                } else {
                    c->shell->has_focus = false; // mapview applet
                    if (_InputTracking.enabled) {
                        inputtracking_focus_lost(&_InputTracking);
                    }
                }
            }
            // if (e.active.state & SDL_APPACTIVE) { // NOTE: doesn't always work
            break;
        }
    }
}

uint64_t get_ticks(void) {
    return SDL_GetTicks();
}

void delay_ticks(int ticks) {
    SDL_Delay(ticks);
}
#endif
