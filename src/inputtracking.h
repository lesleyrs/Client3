#pragma once

#include <stdbool.h>

#include "packet.h"

typedef struct {
    bool enabled;
    Packet *outBuffer; //=null
    Packet *oldBuffer; //=null
    uint64_t lastTime;
    int trackedCount;
    uint64_t lastMoveTime;
    int lastX;
    int lastY;
} InputTracking;

// all these are static synchronized in java
void inputtracking_set_enabled(InputTracking *it);
void inputtracking_set_disabled(InputTracking *it);
Packet *inputtracking_flush(InputTracking *it);
Packet *inputtracking_stop(InputTracking *it);
void inputtracking_ensure_capacity(InputTracking *it, int n);
void inputtracking_mouse_pressed(InputTracking *it, int x, int y, int button);
void inputtracking_mouse_released(InputTracking *it, int button);
void inputtracking_mouse_moved(InputTracking *it, int x, int y);
void inputtracking_key_pressed(InputTracking *it, int key);
void inputtracking_key_released(InputTracking *it, int key);
void inputtracking_focus_gained(InputTracking *it);
void inputtracking_focus_lost(InputTracking *it);
void inputtracking_mouse_entered(InputTracking *it);
void inputtracking_mouse_exited(InputTracking *it);
