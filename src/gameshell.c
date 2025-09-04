#include <stdlib.h>

#include "client.h"
#include "defines.h"
#include "gameshell.h"
#include "inputtracking.h"
#include "pixmap.h"
#include "platform.h"

#ifdef __3DS__
#include <3ds.h>
#endif

extern InputTracking _InputTracking;

bool update_touch = false;
int last_touch_x = 0;
int last_touch_y = 0;
int last_touch_button = 0;

static void gameshell_update_touch(Client *c) {
    if (update_touch) {
        c->shell->mouse_click_x = last_touch_x;
        c->shell->mouse_click_y = last_touch_y;
        c->shell->mouse_click_button = last_touch_button;
        update_touch = false;
    }
}

GameShell *gameshell_new(void) {
    GameShell *shell = calloc(1, sizeof(GameShell));
    shell->deltime = 20;
    shell->mindel = 1;
    shell->otim = calloc(10, sizeof(uint64_t));
    // shell->sprite_cache = calloc(6, sizeof(Pix24));
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
    platform_new(c->shell);
#if defined(playground) || defined(mapview)
    c->shell->draw_area = pixmap_new(c->shell->screen_width, c->shell->screen_height);
#endif
    gameshell_run(c);
}

void gameshell_run(Client *c) {
    gameshell_draw_progress(c->shell, "Loading...", 0);
    client_load(c);

    int opos = 0;
    int ratio = 256;
    int delta = 1;
    int count = 0;
    for (int i = 0; i < 10; i++) {
        c->shell->otim[i] = rs2_now();
    }
    int64_t ntime;
    while (c->shell->state >= 0) {
#ifdef __3DS__
        if (!aptMainLoop()) {
            return;
        }
#endif
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
        ntime = rs2_now();
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
            delta = (int)((int64_t)c->shell->deltime - (ntime - c->shell->otim[opos]) / 10L);
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

        rs2_sleep(delta);
        while (count < 256) {
            platform_poll_events(c);
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
        client_run_flames(c);      // NOTE: random placement of run_flames
        gameshell_update_touch(c); // update mouse after client_draw_scene to fix model picking (not needed for touch on release like client-ts)
        platform_update_surface();
    }
    if (c->shell->state == -1) {
        gameshell_shutdown(c);
    }
}

void gameshell_destroy(Client *c) {
    c->shell->state = -1;
    // rs2_sleep(5000); // NOTE really this long?
    // gameshell_shutdown(c);
}

void gameshell_shutdown(Client *c) {
    c->shell->state = -2;
    client_unload(c);
    // rs2_sleep(1000); // NOTE: original
    exit(0);
}

void gameshell_set_framerate(GameShell *shell, int fps) {
    shell->deltime = 1000 / fps;
}

void key_pressed(GameShell *shell, int code, int ch) {
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
    } else if (code == 13) { // needed for windows?
        // ENTER
        ch = 13;
#ifdef __EMSCRIPTEN__
    } else if (code >= 112 && code <= 123) {
        ch = code + 1008 - 112;
#endif
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
    } else if (code == 13) { // needed for windows?
        // ENTER
        ch = 13;
#ifdef __EMSCRIPTEN__
    } else if (code >= 112 && code <= 123) {
        ch = code + 1008 - 112;
#endif
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

void gameshell_draw_progress(GameShell *shell, const char *message, int progress) {
    // NOTE there's no update or paint to call refresh, only focus gained event
    if (shell->refresh) {
        platform_set_color(BLACK);
        platform_fill_rect(0, 0, shell->screen_width, shell->screen_height);
        shell->refresh = false;
    }

    int y = shell->screen_height / 2 - 18;

    // rgb 140, 17, 17 but we only take hex for simplicity
    platform_set_color(PROGRESS_RED);
    platform_draw_rect(shell->screen_width / 2 - 152, y, 304, 34);
    platform_fill_rect(shell->screen_width / 2 - 150, y + 2, progress * 3, 30);
    platform_set_color(BLACK);
    platform_fill_rect(shell->screen_width / 2 + progress * 3 - 150, y + 2, 300 - progress * 3, 30);

    platform_set_font("Helvetica", true, 13);
    platform_set_color(WHITE);
    platform_draw_string(message, (shell->screen_width - platform_string_width(message)) / 2, y + 22);

    platform_update_surface();
}
