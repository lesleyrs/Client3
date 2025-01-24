#pragma once

#include <stdbool.h>

#include "datastruct/lrucache.h"
#include "defines.h"
#include "model.h"
#include "pix24.h"
#include "pixfont.h"

#define TYPE_LAYER 0
#define TYPE_UNUSED 1 // TODO: decodes g2, gbool, center, font, shadowed, colour
#define TYPE_INV 2
#define TYPE_RECT 3
#define TYPE_TEXT 4
#define TYPE_GRAPHIC 5
#define TYPE_MODEL 6
#define TYPE_INV_TEXT 7
#define BUTTON_OK 1
#define BUTTON_TARGET 2
#define BUTTON_CLOSE 3
#define BUTTON_TOGGLE 4
#define BUTTON_SELECT 5
#define BUTTON_CONTINUE 6

typedef struct {
    int *invSlotObjId;
    int *invSlotObjCount;
    int seqFrame;
    int seqCycle;
    int id;
    int layer;
    int type;
    int buttonType;
    int clientCode;

    /* Client codes:
     * ---- friends
     * 1-200: friends list
     * 201: add friend
     * 202: delete friend
     * 203: friends list scrollbar size
     * ---- logout
     * 205: logout
     * ---- player_design
     * 300: change head (left)
     * 301: change head (right)
     * 302: change jaw (left)
     * 303: change jaw (right)
     * 304: change torso (left)
     * 305: change torso (right)
     * 306: change arms (left)
     * 307: change arms (right)
     * 308: change hands (left)
     * 309: change hands (right)
     * 310: change legs (left)
     * 311: change legs (right)
     * 312: change feet (left)
     * 313: change feet (right)
     * 314: recolour hair (left)
     * 315: recolour hair (right)
     * 316: recolour torso (left)
     * 317: recolour torso (right)
     * 318: recolour legs (left)
     * 319: recolour legs (right)
     * 320: recolour feet (left)
     * 321: recolour feet (right)
     * 322: recolour skin (left)
     * 323: recolour skin (right)
     * 324: switch to male
     * 325: switch to female
     * 326: accept design
     * 327: design preview
     * ---- ignore
     * 401-500: ignore list
     * 501: add ignore
     * 502: delete ignore
     * 503: ignore list scrollbar size
     * ---- reportabuse
     * 601: rule 1
     * 602: rule 2
     * 603: rule 3
     * 604: rule 4
     * 605: rule 5
     * 606: rule 6
     * 607: rule 7
     * 608: rule 8
     * 609: rule 9
     * 610: rule 10
     * 611: rule 11
     * 612: rule 12
     * 613: moderator mute
     * ---- welcome_screen / welcome_screen2
     * 650: last login info (has recovery questions set)
     * 651: unread messages
     * 655: last login info (no recovery questions set)
     */

    int width;
    int height;
    int x;
    int y;
    int **scripts;
    int *scriptComparator;
    int *scriptOperand;
    int overLayer;
    int scroll;
    int scrollPosition;
    bool hide;
    int *childId;
    int *childX;
    int *childY;
    int unusedShort1;
    bool unusedBoolean1;
    bool draggable;
    bool interactable;
    bool usable;
    int marginX;
    int marginY;
    Pix24 **invSlotSprite;
    int *invSlotOffsetX;
    int *invSlotOffsetY;
    char **iops;
    bool fill;
    bool center;
    bool shadowed;
    PixFont *font;
    char text[DOUBLE_STR]; // arbitrary length, had to double it to stop overflow
    char *activeText;
    int colour;
    int activeColour;
    int overColour;
    Pix24 *graphic;
    Pix24 *activeGraphic;
    Model *model;
    Model *activeModel;
    int anim;
    int activeAnim;
    int zoom;
    int xan;
    int yan;
    char *actionVerb;
    char *action;
    int actionTarget;
    char option[HALF_STR]; // arbitrary length, longest option is 30 in this rev

    int childCount;
    int comparatorCount;
    int scriptCount;
} Component;

typedef struct {
    int count;
    Component **instances;
    LruCache *imageCache;
    LruCache *modelCache;
} ComponentData;

void component_free_global(void);
void component_unpack(Jagfile *jag, Jagfile *media, PixFont **fonts);
Pix24 *component_get_image(Jagfile *media, char *sprite, int spriteId);
Model *component_get_model(int id);
Model *component_get_model2(Component *com, int primaryFrame, int secondaryFrame, bool active, bool *_free);
