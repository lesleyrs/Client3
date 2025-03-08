#pragma once

#include "client.h"
#include <stdbool.h>

typedef struct {
    bool showDebug;
    bool showPerformance;
    bool cameraEditor;
    bool remember_username;
    bool remember_password;
    bool hide_dns;
    bool hide_debug_sprite;
    bool allow_commands;
    bool allow_debugprocs;
    bool disable_rsa;
    int http_port;
    int chat_era; // 0 - early beta, 1 - late beta, 2 - launch
    bool resizable;
} Custom;

bool load_ini_args(void);
void load_ini_config(Client *c);
void update_camera_editor(Client *c);
void draw_info_overlay(Client *c);
