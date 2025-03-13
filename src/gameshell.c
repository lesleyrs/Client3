#include <stdio.h>
#include <stdlib.h>

#include "client.h"
#include "gameshell.h"
#include "inputtracking.h"
#include "pixmap.h"
#include "platform.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "defines.h"
#include "thirdparty/stb_truetype.h"

extern InputTracking _InputTracking;

GameShell *gameshell_new(void) {
    GameShell *shell = calloc(1, sizeof(GameShell));
    shell->deltime = 20;
    shell->mindel = 1;
    shell->otim = calloc(10, sizeof(uint64_t));
    // TODO remove?
    // shell->sprite_cache = calloc(6, sizeof(Sprite));
    shell->refresh = true;
    shell->action_key = calloc(128, sizeof(int));
    shell->key_queue = calloc(128, sizeof(int));
    shell->key_queue_read_pos = 0;
    shell->key_queue_write_pos = 0;
    return shell;
}

void gameshell_free(GameShell *shell) {
    platform_free();
    if (shell->draw_area) {
        pixmap_free(shell->draw_area);
    }
    free(shell->otim);
    free(shell->action_key);
    free(shell->key_queue);
    free(shell);
}

void gameshell_init_application(Client *c, int width, int height) {
    c->shell->screen_width = width;
    c->shell->screen_height = height;
    platform_new(c->shell->screen_width, c->shell->screen_height);
#ifdef client
    c->shell->draw_area = NULL;
#else
    c->shell->draw_area = pixmap_new(c->shell->screen_width, c->shell->screen_height);
#endif
    gameshell_run(c);
}

void gameshell_run(Client *c) {
    gameshell_draw_progress(c, "Loading...", 0);
    client_load(c);

    int opos = 0;
    int ratio = 256;
    int delta = 1;
    int count = 0;
    for (int i = 0; i < 10; i++) {
        c->shell->otim[i] = get_ticks();
    }
    uint64_t ntime;
    while (c->shell->state >= 0) {
        if (c->shell->state > 0) {
            c->shell->state--;
            if (c->shell->state == 0) {
                gameshell_shutdown(c);
                return;
            }
        }
        int lastRatio = ratio;
        int lastDelta = delta;
        ratio = 300;
        delta = 1;
        ntime = get_ticks();
        if (c->shell->otim[opos] == 0L) {
            ratio = lastRatio;
            delta = lastDelta;
        } else if (ntime > c->shell->otim[opos]) {
            ratio = (int)((c->shell->deltime * 2560L) / (ntime - c->shell->otim[opos]));
        }
        if (ratio < 25) {
            ratio = 25;
        }
        if (ratio > 256) {
            ratio = 256;
            delta = (int)((uint64_t)c->shell->deltime - (ntime - c->shell->otim[opos]) / 10L);
        }
        c->shell->otim[opos] = ntime;
        opos = (opos + 1) % 10;
        if (delta > 1) {
            for (int i = 0; i < 10; i++) {
                if (c->shell->otim[i] != 0L) {
                    c->shell->otim[i] += delta;
                }
            }
        }
        if (delta < c->shell->mindel) {
            delta = c->shell->mindel;
        }

        delay_ticks(delta);
        while (count < 256) {
            client_update(c);
            c->shell->mouse_click_button = 0;
            c->shell->key_queue_read_pos = c->shell->key_queue_write_pos;
            count += ratio;
        }
        count &= 0xff;
        if (c->shell->deltime > 0) {
            c->shell->fps = ratio * 1000 / (c->shell->deltime * 256);
        }
        client_draw(c);
        client_run_flames(c); // NOTE: random placement of run_flames

        // TODO temp
        #if defined(__3DS__) || defined(__WIIU__) || defined(__SWITCH__) || defined(__PSP__) || defined(__DREAMCAST__)
        static bool loggedin;
        if (!loggedin) {
            client_login(c, c->username, c->password, false);
            loggedin = true;
        }
        #endif
    }
    if (c->shell->state == -1) {
        gameshell_shutdown(c);
    }
}

void gameshell_destroy(Client *c) {
    c->shell->state = -1;
    // delay_ticks(5000); // NOTE really this long?
    // gameshell_shutdown(c);
}

void gameshell_shutdown(Client *c) {
    c->shell->state = -2;
    client_unload(c);
    // delay_ticks(1000); // NOTE: original
    exit(0);
}

void gameshell_set_framerate(GameShell *shell, int fps) {
    shell->deltime = 1000 / fps;
}

void key_pressed(GameShell *shell, int code, int ch) {
    shell->idle_cycles = 0;

    // TODO: check
    // if (e.ctrlKey) {
    //     if ((ch >= 'A'.charCodeAt(0) && ch <= ']'.charCodeAt(0)) || ch == '_'.charCodeAt(0)) {
    //         ch -= 'A'.charCodeAt(0) - 1;
    //     } else if (ch >= 'a'.charCodeAt(0) && ch <= 'z'.charCodeAt(0)) {
    //         ch -= 'a'.charCodeAt(0) - 1;
    //     }
    // }

    // rs2_log("%i %i\n", code, ch);
    if (ch < 30) {
        ch = 0;
    }

    if (code == 37) {
        // KEY_LEFT
        ch = 1;
    } else if (code == 39) {
        // KEY_RIGHT
        ch = 2;
    } else if (code == 38) {
        // KEY_UP
        ch = 3;
    } else if (code == 40) {
        // KEY_DOWN
        ch = 4;
    } else if (code == 17) {
        // CONTROL
        ch = 5;
    } else if (code == 16) {
        // SHIFT
        ch = 6; // (custom)
    } else if (code == 18) {
        // ALT
        ch = 7; // (custom)
    } else if (code == 8) {
        // BACKSPACE
        ch = 8;
    } else if (code == 127) {
        // DELETE
        ch = 8;
    } else if (code == 9) {
        ch = 9;
    } else if (code == 10) {
        // ENTER
        ch = 10;
    } else if (code == 13) { // NOTE: needed for windows?
        // ENTER
        ch = 13;
        // } else if (code >= 112 && code <= 123) {
        // 	ch = code + 1008 - 112;
    } else if (code == 36) {
        ch = 1000;
    } else if (code == 35) {
        ch = 1001;
    } else if (code == 33) {
        ch = 1002;
    } else if (code == 34) {
        ch = 1003;
    }

    if (ch > 0 && ch < 128) {
        shell->action_key[ch] = 1;
    }

    if (ch > 4) {
        shell->key_queue[shell->key_queue_write_pos] = ch;
        shell->key_queue_write_pos = shell->key_queue_write_pos + 1 & 0x7f;
    }

    if (_InputTracking.enabled) {
        inputtracking_key_pressed(&_InputTracking, ch);
    }
}

void key_released(GameShell *shell, int code, int ch) {
    shell->idle_cycles = 0;

    if (ch < 30) {
        ch = 0;
    }

    if (code == 37) {
        // KEY_LEFT
        ch = 1;
    } else if (code == 39) {
        // KEY_RIGHT
        ch = 2;
    } else if (code == 38) {
        // KEY_UP
        ch = 3;
    } else if (code == 40) {
        // KEY_DOWN
        ch = 4;
    } else if (code == 17) {
        // CONTROL
        ch = 5;
    } else if (code == 16) {
        // SHIFT
        ch = 6; // (custom)
    } else if (code == 18) {
        // ALT
        ch = 7; // (custom)
    } else if (code == 8) {
        // BACKSPACE
        ch = 8;
    } else if (code == 127) {
        // DELETE
        ch = 8;
    } else if (code == 9) {
        ch = 9;
    } else if (code == 10) {
        // ENTER
        ch = 10;
        // } else if (code >= 112 && code <= 123) {
        // 	ch = code + 1008 - 112;
    } else if (code == 36) {
        ch = 1000;
    } else if (code == 35) {
        ch = 1001;
    } else if (code == 33) {
        ch = 1002;
    } else if (code == 34) {
        ch = 1003;
    }

    if (ch > 0 && ch < 128) {
        shell->action_key[ch] = 0;
    }

    if (_InputTracking.enabled) {
        inputtracking_key_released(&_InputTracking, ch);
    }
}

int poll_key(GameShell *shell) {
    int key = -1;
    if (shell->key_queue_write_pos != shell->key_queue_read_pos) {
        key = shell->key_queue[shell->key_queue_read_pos];
        shell->key_queue_read_pos = shell->key_queue_read_pos + 1 & 0x7f;
    }
    return key;
}

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

void gameshell_draw_string(Client *c, const char *str, int x, int y, int color, const char *font_name, bool font_bold, int font_size) {
    (void)font_name, (void)font_bold;
    // TODO: abstract this
    // TODO: fontname and bold ignored rn it's always bold
    FILE *file = NULL;
#if defined(_WIN32) || defined(_WIN64)
    // c:/windows/fonts/ - arialbd
    file = fopen("c:/windows/fonts/arialbd.ttf", "rb");
#elif defined(__linux__)
    // /usr/share/fonts - dejavu/liberation sans
    // FILE *file = fopen("/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf", "rb");
    file = fopen("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "rb");
#elif defined(__APPLE__) && defined(__MACH__)
// /system/library/fonts/ - helvetica neue
#elif defined(__FreeBSD__)
// /usr/local/share/fonts/
#else
    file = fopen("Roboto/Roboto-Bold.ttf", "rb");
#endif

    if (!file) {
        file = fopen("Roboto/Roboto-Bold.ttf", "rb");
        if (!file) {
            rs2_error("Failed to open font file\n");
            return;
        }
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t *buffer = malloc(size);
    if (fread(buffer, 1, size, file) != size) {
        rs2_error("Failed to read file\n", strerror(errno));
    }
    fclose(file);

    stbtt_fontinfo font;
    stbtt_InitFont(&font, buffer, stbtt_GetFontOffsetForIndex(buffer, 0));

    float scale = stbtt_ScaleForPixelHeight(&font, (float)font_size);

    float xpos = 0;
    int ch = 0;
    // NOTE: this could probably be improved a lot
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

        // TODO: is this centering correct? maybe few pixels to the left?
        if (x == -1) {
            platform_blit_surface((c->shell->screen_width - ttf_string_width(&font, str, scale)) / 2 + (int)xpos + x0, y + y0, width, height, surface);
        } else {
            platform_blit_surface(x + (int)xpos + x0, y + y0, width, height, surface);
        }
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

void gameshell_draw_progress(Client *c, const char *message, int progress) {
    // NOTE there's no update or paint to call refresh, only focus gained event
    if (c->shell->refresh) {
        platform_fill_rect(0, 0, c->shell->screen_width, c->shell->screen_height, BLACK);
        c->shell->refresh = false;
    }

    int y = c->shell->screen_height / 2 - 18;

    platform_fill_rect(c->shell->screen_width / 2 - 152, y, 304, 34, PROGRESS_RED); // NOTE: actually drawRect, but seems to be same effect
    platform_fill_rect(c->shell->screen_width / 2 - 150, y + 2, progress * 3, 30, PROGRESS_RED);
    platform_fill_rect(c->shell->screen_width / 2 + progress * 3 - 150, y + 2, 300 - progress * 3, 30, BLACK);

    int color = WHITE;
    gameshell_draw_string(c, message, -1, y + 22, HELVETICA_BOLD_13);

    platform_update_surface();
}
