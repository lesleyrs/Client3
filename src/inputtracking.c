#include "inputtracking.h"
#include "platform.h"

InputTracking _InputTracking;

void inputtracking_set_enabled(InputTracking *it) {
    it->outBuffer = packet_alloc(1);
    it->oldBuffer = NULL;
    it->lastTime = get_ticks();
    it->enabled = true;
}

void inputtracking_set_disabled(InputTracking *it) {
    it->enabled = false;
    it->outBuffer = NULL;
}

Packet *inputtracking_flush(InputTracking *it) {
    Packet *buffer = NULL;
    if (it->oldBuffer && it->enabled) {
        buffer = it->oldBuffer;
    }
    it->oldBuffer = NULL;
    return buffer;
}

Packet *inputtracking_stop(InputTracking *it) {
    Packet *buffer = NULL;
    if (it->outBuffer && it->outBuffer->pos > 0 && it->enabled) {
        buffer = it->outBuffer;
    }
    inputtracking_set_disabled(it);
    return buffer;
}

void inputtracking_ensure_capacity(InputTracking *it, int n) {
    if (it->outBuffer->pos + n >= 500) {
        Packet *buffer = it->outBuffer;
        it->outBuffer = packet_alloc(1);
        it->oldBuffer = buffer;
    }
}

void inputtracking_mouse_pressed(InputTracking *it, int x, int y, int button) {
    if (it->enabled && (x >= 0 && x < 789 && y >= 0 && y < 532)) {
        it->trackedCount++;

        uint64_t now = get_ticks();
        uint64_t delta = (now - it->lastTime) / 10L;
        if (delta > 250L) {
            delta = 250L;
        }

        it->lastTime = now;
        inputtracking_ensure_capacity(it, 5);

        if (button == 1) {
            p1(it->outBuffer, 1);
        } else {
            p1(it->outBuffer, 2);
        }

        p1(it->outBuffer, (int)delta);
        p3(it->outBuffer, x + (y << 10));
    }
}

void inputtracking_mouse_released(InputTracking *it, int button) {
    if (it->enabled) {
        it->trackedCount++;

        uint64_t now = get_ticks();
        uint64_t delta = (now - it->lastTime) / 10L;
        if (delta > 250L) {
            delta = 250L;
        }

        it->lastTime = now;
        inputtracking_ensure_capacity(it, 2);

        if (button == 1) {
            p1(it->outBuffer, 3);
        } else {
            p1(it->outBuffer, 4);
        }

        p1(it->outBuffer, (int)delta);
    }
}

void inputtracking_mouse_moved(InputTracking *it, int x, int y) {
    if (it->enabled && (x >= 0 && x < 789 && y >= 0 && y < 532)) {
        uint64_t now = get_ticks();

        if (now - it->lastMoveTime >= 50L) {
            it->lastMoveTime = now;
            it->trackedCount++;

            uint64_t delta = (now - it->lastTime) / 10L;
            if (delta > 250L) {
                delta = 250L;
            }

            it->lastTime = now;
            if (x - it->lastX < 8 && x - it->lastX >= -8 && y - it->lastY < 8 && y - it->lastY >= -8) {
                inputtracking_ensure_capacity(it, 3);
                p1(it->outBuffer, 5);
                p1(it->outBuffer, (int)delta);
                p1(it->outBuffer, x + ((y - it->lastY + 8) << 4) + 8 - it->lastX);
            } else if (x - it->lastX < 128 && x - it->lastX >= -128 && y - it->lastY < 128 && y - it->lastY >= -128) {
                inputtracking_ensure_capacity(it, 4);
                p1(it->outBuffer, 6);
                p1(it->outBuffer, (int)delta);
                p1(it->outBuffer, x + 128 - it->lastX);
                p1(it->outBuffer, y + 128 - it->lastY);
            } else {
                inputtracking_ensure_capacity(it, 5);
                p1(it->outBuffer, 7);
                p1(it->outBuffer, (int)delta);
                p3(it->outBuffer, x + (y << 10));
            }

            it->lastX = x;
            it->lastY = y;
        }
    }
}

void inputtracking_key_pressed(InputTracking *it, int key) {
    if (it->enabled) {
        it->trackedCount++;

        uint64_t now = get_ticks();
        uint64_t delta = (now - it->lastTime) / 10L;
        if (delta > 250L) {
            delta = 250L;
        }

        it->lastTime = now;
        if (key == 1000) {
            key = 11;
        } else if (key == 1001) {
            key = 12;
        } else if (key == 1002) {
            key = 14;
        } else if (key == 1003) {
            key = 15;
        } else if (key >= 1008) {
            key -= 992;
        }

        inputtracking_ensure_capacity(it, 3);
        p1(it->outBuffer, 8);
        p1(it->outBuffer, (int)delta);
        p1(it->outBuffer, key);
    }
}

void inputtracking_key_released(InputTracking *it, int key) {
    if (it->enabled) {
        it->trackedCount++;

        uint64_t now = get_ticks();
        uint64_t delta = (now - it->lastTime) / 10L;
        if (delta > 250L) {
            delta = 250L;
        }

        it->lastTime = now;
        if (key == 1000) {
            key = 11;
        } else if (key == 1001) {
            key = 12;
        } else if (key == 1002) {
            key = 14;
        } else if (key == 1003) {
            key = 15;
        } else if (key >= 1008) {
            key -= 992;
        }

        inputtracking_ensure_capacity(it, 3);
        p1(it->outBuffer, 9);
        p1(it->outBuffer, (int)delta);
        p1(it->outBuffer, key);
    }
}

void inputtracking_focus_gained(InputTracking *it) {
    if (it->enabled) {
        it->trackedCount++;

        uint64_t now = get_ticks();
        uint64_t delta = (now - it->lastTime) / 10L;
        if (delta > 250L) {
            delta = 250L;
        }

        it->lastTime = now;
        inputtracking_ensure_capacity(it, 2);
        p1(it->outBuffer, 10);
        p1(it->outBuffer, (int)delta);
    }
}

void inputtracking_focus_lost(InputTracking *it) {
    if (it->enabled) {
        it->trackedCount++;

        uint64_t now = get_ticks();
        uint64_t delta = (now - it->lastTime) / 10L;
        if (delta > 250L) {
            delta = 250L;
        }

        it->lastTime = now;
        inputtracking_ensure_capacity(it, 2);
        p1(it->outBuffer, 11);
        p1(it->outBuffer, (int)delta);
    }
}

void inputtracking_mouse_entered(InputTracking *it) {
    if (it->enabled) {
        it->trackedCount++;

        uint64_t now = get_ticks();
        uint64_t delta = (now - it->lastTime) / 10L;
        if (delta > 250L) {
            delta = 250L;
        }

        it->lastTime = now;
        inputtracking_ensure_capacity(it, 2);
        p1(it->outBuffer, 12);
        p1(it->outBuffer, (int)delta);
    }
}

void inputtracking_mouse_exited(InputTracking *it) {
    if (it->enabled) {
        it->trackedCount++;

        uint64_t now = get_ticks();
        uint64_t delta = (now - it->lastTime) / 10L;
        if (delta > 250L) {
            delta = 250L;
        }

        it->lastTime = now;
        inputtracking_ensure_capacity(it, 2);
        p1(it->outBuffer, 13);
        p1(it->outBuffer, (int)delta);
    }
}
