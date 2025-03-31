#pragma once

#include <stdbool.h>

#include "platform.h"

struct GameShell {
    int state;
    int fps;
    int screen_width;
    int screen_height;
    PixMap *draw_area;
    int idle_cycles;
    int mouse_button;
    int mouse_x;
    int mouse_y;
    int mouse_click_button;
    int mouse_click_x;
    int mouse_click_y;
    int deltime;
    int mindel;
    uint64_t *otim;
    bool refresh;
    int *action_key;
    int *key_queue;
    int key_queue_read_pos;
    int key_queue_write_pos;
    bool has_focus;
};

GameShell *gameshell_new(void);
void gameshell_free(GameShell *shell);
void gameshell_init_application(Client *c, int width, int height);
void gameshell_run(Client *c);
void gameshell_destroy(Client *c);
void gameshell_shutdown(Client *c);
void gameshell_set_framerate(GameShell *shell, int fps);
void gameshell_draw_progress(GameShell *shell, const char *message, int progress);
void gameshell_draw_string(GameShell *shell, const char *str, int x, int y, int color, bool bold, int size);
int poll_key(GameShell *shell);
void key_pressed(GameShell *shell, int code, int ch);
void key_released(GameShell *shell, int code, int ch);
