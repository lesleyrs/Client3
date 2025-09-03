#ifdef DUMMY
#include "../client.h"
#include "../custom.h"
#include "../defines.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"
#include "../thirdparty/bzip.h"

extern ClientData _Client;
extern InputTracking _InputTracking;
extern Custom _Custom;

bool platform_init(void) {
    return true;
}

void platform_new(GameShell *shell) {
    (void)shell;
}

void platform_free(void) {
}

void platform_update_surface(void) {
}

void platform_blit_surface(Surface *surface, int x, int y) {
    platform_set_pixels(NULL, surface, x, y, false);
}

void platform_poll_events(Client *c) {
    (void)c;
}

uint64_t rs2_now(void) {
    return 0;
}

void rs2_sleep(int ms) {
    (void)ms;
}

void platform_set_wave_volume(int wavevol) {
    (void)wavevol;
}

void platform_play_wave(int8_t *src, int length) {
    (void)src, (void)length;
}

void platform_set_midi_volume(float midivol) {
    (void)midivol;
}

void platform_set_jingle(int8_t *src, int len) {
    (void)src, (void)len;
}

void platform_set_midi(const char *name, int crc, int len) {
    (void)name, (void)crc, (void)len;
}

void platform_stop_midi(void) {
}
#endif
