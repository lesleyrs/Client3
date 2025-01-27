#pragma once

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 260
#endif

#ifndef RSA_BUF_LEN
#define RSA_BUF_LEN 128
#endif

// arbitrary to fix -Wall possible overflow warnings
// NOTE maybe change the ones using half_str to use strncpy but yolo
// NOTE could also double both buf sizes to be 100% sure, would waste memory though
#define HALF_STR (CHAT_LENGTH / 2)
#define DOUBLE_STR (CHAT_LENGTH * 2)
// arbitrary + null terminator
#define MAX_STR (CHAT_LENGTH + 1)

#define HELVETICA_BOLD_12 color, "Helvetica", true, 12
#define HELVETICA_BOLD_13 color, "Helvetica", true, 13
#define HELVETICA_BOLD_16 color, "Helvetica", true, 16
#define HELVETICA_BOLD_20 color, "Helvetica", true, 20

#define MAX_CHATS 50

#define LOCBUFFER_COUNT 100
#define MAX_NPC_COUNT 8192
#define MAX_PLAYER_COUNT 2048
#define LOCAL_PLAYER_INDEX 2047
#define VARPS_COUNT 2000
#define BFS_STEP_SIZE 4000
#define FLAME_BUFFER_SIZE 32768
#define MENU_OPTION_LENGTH 500
#define CHATBACK_LENGTH 10
#define REPORT_ABUSE_LENGTH 12
#define CHAT_LENGTH 80
#define USERNAME_LENGTH 12
#define PASSWORD_LENGTH 20

// ---- these are in rgb 24 bits
#define RED 0xff0000      // 16711680
#define GREEN 0xff00      // 65280
#define BLUE 0xff         // 255
#define YELLOW 0xffff00   // 16776960
#define CYAN 0xffff       // 65535
#define MAGENTA 0xff00ff  // 16711935
#define WHITE 0xffffff    // 16777215
#define BLACK 0x0         // 0
#define LIGHTRED 0xff9040 // 16748608
#define DARKRED 0x800000  // 8388608
#define DARKBLUE 0x80     // 128
#define ORANGE1 0xffb000  // 16756736
#define ORANGE2 0xff7000  // 16740352
#define ORANGE3 0xff3000  // 16723968
#define GREEN1 0xc0ff00   // 12648192
#define GREEN2 0x80ff00   // 8453888
#define GREEN3 0x40ff00   // 4259584

// other
#define PROGRESS_RED 0x8c1111              // 9179409
#define OPTIONS_MENU 0x5d5447              // 6116423
#define SCROLLBAR_TRACK 0x23201b           // 2301979
#define SCROLLBAR_GRIP_FOREGROUND 0x4d4233 // 5063219
#define SCROLLBAR_GRIP_HIGHLIGHT 0x766654  // 7759444
#define SCROLLBAR_GRIP_LOWLIGHT 0x332d25   // 3353893
#define TRADE_MESSAGE 0x800080             // 8388736
#define DUEL_MESSAGE 0xcbb789              // 13350793

// ---- these are in hsl 16 bits
// hair
#define HAIR_DARK_BROWN 6798
#define HAIR_WHITE 107
#define HAIR_LIGHT_GREY 10283
#define HAIR_DARK_GREY 16
#define HAIR_APRICOT 4797
#define HAIR_STRAW 7744
#define HAIR_LIGHT_BROWN 5799
#define HAIR_BROWN 4634
#define HAIR_TURQUOISE 33697
#define HAIR_GREEN 22433
#define HAIR_GINGER 2983
#define HAIR_MAGENTA 54193

// body
#define BODY_KHAKI 8741
#define BODY_CHARCOAL 12
#define BODY_CRIMSON 64030
#define BODY_NAVY 43162
#define BODY_STRAW 7735
#define BODY_WHITE 8404
#define BODY_RED 1701
#define BODY_BLUE 38430
#define BODY_GREEN 24094
#define BODY_YELLOW 10153
#define BODY_PURPLE 56621
#define BODY_ORANGE 4783
#define BODY_ROSE 1341
#define BODY_LIME 16578
#define BODY_CYAN 35003
#define BODY_EMERALD 25239

#define BODY_RECOLOR_KHAKI 9104
#define BODY_RECOLOR_CHARCOAL 10275
#define BODY_RECOLOR_CRIMSON 7595
#define BODY_RECOLOR_NAVY 3610
#define BODY_RECOLOR_STRAW 7975
#define BODY_RECOLOR_WHITE 8526
#define BODY_RECOLOR_RED 918
#define BODY_RECOLOR_BLUE 38802
#define BODY_RECOLOR_GREEN 24466
#define BODY_RECOLOR_YELLOW 10145
#define BODY_RECOLOR_PURPLE 58654
#define BODY_RECOLOR_ORANGE 5027
#define BODY_RECOLOR_ROSE 1457
#define BODY_RECOLOR_LIME 16565
#define BODY_RECOLOR_CYAN 34991
#define BODY_RECOLOR_EMERALD 25486

// feet
#define FEET_BROWN 4626
#define FEET_KHAKI 11146
#define FEET_ASHEN 6439
#define FEET_DARK 12
#define FEET_TERRACOTTA 4758
#define FEET_GREY 10270

// skin
#define SKIN 4574
#define SKIN_DARKER 4550
#define SKIN_DARKER_DARKER 4537
#define SKIN_DARKER_DARKER_DARKER 5681
#define SKIN_DARKER_DARKER_DARKER_DARKER 5673
#define SKIN_DARKER_DARKER_DARKER_DARKER_DARKER 5790
#define SKIN_DARKER_DARKER_DARKER_DARKER_DARKER_DARKER 6806
#define SKIN_DARKER_DARKER_DARKER_DARKER_DARKER_DARKER_DARKER 8076
