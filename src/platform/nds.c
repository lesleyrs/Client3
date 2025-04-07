#ifdef __NDS__
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dswifi9.h>
#include <filesystem.h>
#include <nds.h>
#include <wfc.h>

#include "../gameshell.h"
#include "../pixmap.h"
#include "../platform.h"
#include "../defines.h"

static int screen_offset_x = (SCREEN_FB_WIDTH - SCREEN_WIDTH) / 2;
static int screen_offset_y = -100;

static uint16_t *fb = (uint16_t *)VRAM_A;

static const char *const signalStrength[] = {
    //---------------------------------------------------------------------------------
    "[   ]",
    "[.  ]",
    "[.i ]",
    "[.iI]",
};

//---------------------------------------------------------------------------------
static const char *const authTypes[] = {
    //---------------------------------------------------------------------------------
    "Open",
    "WEP",
    "WEP",
    "WEP",
    "WPA-PSK-TKIP",
    "WPA-PSK-AES",
    "WPA2-PSK-TKIP",
    "WPA2-PSK-AES",
};

static const char *const connStatus[] = {
    //---------------------------------------------------------------------------------
    "Disconnected :(",
    "Searching...",
    "Associating...",
    "Obtaining IP address...",
    "Connected!",
};

static void keyPressed(int c) {
    //---------------------------------------------------------------------------------
    if (c > 0)
        rs2_log("%c", c);
}

static inline WlanBssAuthType authMaskToType(unsigned mask) {
    //---------------------------------------------------------------------------------
    return mask ? (WlanBssAuthType)(31 - __builtin_clz(mask)) : WlanBssAuthType_Open;
}

static WlanBssDesc *findAP(void) {
    //---------------------------------------------------------------------------------
    int selected = 0;
    int i;
    int displaytop = 0;
    unsigned count = 0;
    WlanBssDesc *aplist = NULL;
    WlanBssDesc *apselected = NULL;

    static const WlanBssScanFilter filter = {
        .channel_mask = UINT32_MAX,
        .target_bssid = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    };

_rescan:
    if (!wfcBeginScan(&filter)) {
        return NULL;
    }

    rs2_log("Scanning APs...\n");

    while (pmMainLoop() && !(aplist = wfcGetScanBssList(&count))) {
        swiWaitForVBlank();
        scanKeys();

        if (keysDown() & KEY_START)
            exit(0);
    }

    if (!aplist || !count) {
        rs2_log("No APs detected\n");
        return NULL;
    }

    while (pmMainLoop()) {
        swiWaitForVBlank();
        scanKeys();

        u32 pressed = keysDown();
        if (pressed & KEY_START)
            exit(0);
        if (pressed & KEY_A) {
            apselected = &aplist[selected];
            break;
        }

        consoleClear();

        if (pressed & KEY_R) {
            goto _rescan;
        }

        rs2_log("%u APs detected (R = rescan)\n\n", count);

        int displayend = displaytop + 10;
        if (displayend > count)
            displayend = count;

        // display the APs to the user
        for (i = displaytop; i < displayend; i++) {
            WlanBssDesc *ap = &aplist[i];

            // display the name of the AP
            rs2_log("%s%.29s\n  %s Type:%s\n",
                    i == selected ? "*" : " ",
                    ap->ssid_len ? ap->ssid : "-- Hidden SSID --",
                    signalStrength[wlanCalcSignalStrength(ap->rssi)],
                    authTypes[authMaskToType(ap->auth_mask)]);
        }

        // move the selection asterisk
        if (pressed & KEY_UP) {
            selected--;
            if (selected < 0) {
                selected = 0;
            }

            if (selected < displaytop) {
                displaytop = selected;
            }
        }

        if (pressed & KEY_DOWN) {
            selected++;
            if (selected >= count) {
                selected = count - 1;
            }

            displaytop = selected - 9;
            if (displaytop < 0) {
                displaytop = 0;
            }
        }
    }

    return apselected;
}

bool platform_init(void) {
    consoleDemoInit();
    videoSetMode(MODE_FB0);
    vramSetBankA(VRAM_A_LCD);
    if (!isDSiMode()) {
        rs2_error("Unsupported: NDS detected!\n\nThis requires 3DS DSI emulation\n\n");
        return false;
    }
    if (!nitroFSInit(NULL)) {
        rs2_error("Nitro init failed\n");
        return false;
    }
    chdir("nitro:/");

    TIMER0_CR = TIMER_ENABLE | TIMER_DIV_1024;
    // TIMER1_CR = TIMER_ENABLE | TIMER_CASCADE;

    Keyboard *kb = keyboardDemoInit();
    kb->OnKeyPressed = keyPressed;

    // TODO move to clientstream_init?
    if (!Wifi_InitDefault(false)) {
        rs2_error("Wifi init fail\n");
        return false;
    }
    // TODO melonds dsi uses firmware wifi settings so we can't connect to melon ap directly
    // if (!Wifi_InitDefault(WFC_CONNECT)) {
    //     rs2_error("Failed to connect!\n");
    //     return false;
    // } else {
    //     rs2_log("Connected!\n");
    // }

    char instr[64];
    unsigned inlen;

    // Auth data must be in main RAM, not DTCM stack
    static WlanAuthData auth;

    consoleClear();
    consoleSetWindow(NULL, 0, 0, 32, 24);

    WlanBssDesc *ap = findAP();
    if (!ap) {
        rs2_error("No AP found!\n");
        return false;
    }

    consoleClear();
    consoleSetWindow(NULL, 0, 0, 32, 10);

    if (!ap->ssid_len) {
        rs2_log("Enter hidden SSID name\n");
        bool ok;
        do {
            if (!fgets(instr, sizeof(instr), stdin)) {
                exit(0);
            }

            instr[strcspn(instr, "\n")] = 0;
            inlen = strlen(instr);

            ok = inlen && inlen <= WLAN_MAX_SSID_LEN;
            if (!ok) {
                rs2_error("Invalid SSID\n");
            }
        } while (!ok);

        memcpy(ap->ssid, instr, inlen);
        ap->ssid_len = inlen;
    }

    rs2_log("Connecting to %.*s\n", ap->ssid_len, ap->ssid);
    ap->auth_type = authMaskToType(ap->auth_mask);
    memset(&auth, 0, sizeof(auth));

    if (ap->auth_type != WlanBssAuthType_Open) {
        rs2_log("Enter %s key\n", authTypes[ap->auth_type]);
        bool ok;
        do {
            if (!fgets(instr, sizeof(instr), stdin)) {
                exit(0);
            }

            instr[strcspn(instr, "\n")] = 0;
            inlen = strlen(instr);

            ok = true;
            if (ap->auth_type < WlanBssAuthType_WPA_PSK_TKIP) {
                if (inlen == WLAN_WEP_40_LEN) {
                    ap->auth_type = WlanBssAuthType_WEP_40;
                } else if (inlen == WLAN_WEP_104_LEN) {
                    ap->auth_type = WlanBssAuthType_WEP_104;
                } else if (inlen == WLAN_WEP_128_LEN) {
                    ap->auth_type = WlanBssAuthType_WEP_128;
                } else {
                    ok = false;
                }
            } else if (inlen < 1 || inlen >= WLAN_WPA_PSK_LEN) {
                ok = false;
            }

            if (!ok) {
                rs2_error("Invalid key!\n");
            }
        } while (!ok);

        if (ap->auth_type < WlanBssAuthType_WPA_PSK_TKIP) {
            memcpy(auth.wep_key, instr, inlen);
        } else {
            rs2_log("Deriving PMK, please wait\n");
            wfcDeriveWpaKey(&auth, ap->ssid, ap->ssid_len, instr, inlen);
        }
    }

    if (!wfcBeginConnect(ap, &auth)) {
        rs2_error("wfcBeginConnect(): failed!");
        return false;
    }

    bool is_connect = false;
    while (pmMainLoop()) {
        swiWaitForVBlank();
        scanKeys();

        if (keysDown() & KEY_START)
            exit(0);

        int status = Wifi_AssocStatus();

        consoleClear();
        rs2_log("%s\n", connStatus[status]);

        is_connect = status == ASSOCSTATUS_ASSOCIATED;
        if (is_connect || status == ASSOCSTATUS_DISCONNECTED)
            break;
    }

    if (is_connect) {
        unsigned ip = Wifi_GetIP();
        rs2_log("Our IP: %u.%u.%u.%u\n", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
    }

    consoleDemoInit();
    lcdMainOnBottom();
    return true;
}
void platform_new(int width, int height) {
    (void)width, (void)height;
}
void platform_free(void) {
    Wifi_DisconnectAP();
}
void platform_set_wave_volume(int wavevol) {
}
void platform_play_wave(int8_t *src, int length) {
}
void platform_set_midi_volume(float midivol) {
}
void platform_set_jingle(int8_t *src, int len) {
}
void platform_set_midi(const char *name, int crc, int len) {
}
void platform_stop_midi(void) {
}
void set_pixels(PixMap *pixmap, int x, int y) {
    for (int row = 0; row < pixmap->height; row++) {
        int screen_y = y + row + screen_offset_y;
        if (screen_y < 0)
            continue;
        if (screen_y >= SCREEN_FB_HEIGHT)
            break;

        for (int col = 0; col < pixmap->width; col++) {
            int screen_x = x + col + screen_offset_x;
            if (screen_x < 0)
                continue;
            if (screen_x >= SCREEN_FB_WIDTH)
                break;

            int pixel = pixmap->pixels[row * pixmap->width + col];
            uint8_t r = (pixel >> 16) & 0xff;
            uint8_t g = (pixel >> 8) & 0xff;
            uint8_t b = pixel & 0xff;
            fb[screen_y * SCREEN_FB_WIDTH + screen_x] = RGB8(r, g, b);
        }
    }
}
void platform_poll_events(Client *c) {
    // while (pmMainLoop()) {
    //     swiWaitForVBlank();
    //     scanKeys();
    //     int pressed = keysDown();
    //     if (pressed & KEY_START)
    //         break;
    // }
}
void platform_blit_surface(int x, int y, int w, int h, Surface *surface) {
}
void platform_update_surface(void) {
}
void platform_draw_rect(int x, int y, int w, int h, int color) {
}
void platform_fill_rect(int x, int y, int w, int h, int color) {
}
// NOTE: timers are untested
#define timers2ms(tlow, thigh) (tlow | (thigh << 16)) >> 5
uint64_t get_ticks(void) {
    return timers2ms(TIMER0_DATA, TIMER1_DATA);
}
void delay_ticks(int ticks) {
    uint32_t now;
    now = timers2ms(TIMER0_DATA, TIMER1_DATA);
    while ((uint32_t)timers2ms(TIMER0_DATA, TIMER1_DATA) < now + ticks)
        ;
}
#endif
