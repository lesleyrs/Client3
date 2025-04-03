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
    int http_port;
    int chat_era; // 0 - early beta, 1 - late beta, 2 - launch
    bool resizable;
} Custom;

bool load_ini_args(void);
void load_ini_config(Client *c);
void update_camera_editor(Client *c);
void draw_info_overlay(Client *c);

// touch
bool insideChatInputArea(Client *c);
bool insideChatPopupArea(Client *c);
bool insideReportInterfaceTextArea(Client *c);
bool insideUsernameArea(Client *c);
bool inPasswordArea(Client *c);
bool insideMobileInputArea(Client *c);
bool insideTabArea(Client *c);
bool insideViewportArea(Client *c);
