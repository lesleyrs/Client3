#pragma once

#include <stdbool.h>

#include "clientstream.h"
#include "collisionmap.h"
#include "component.h"
#include "defines.h"
#include "jagfile.h"
#include "npcentity.h"
#include "packet.h"
#include "pix24.h"
#include "pix8.h"
#include "pixfont.h"
#include "playerentity.h"
#include "world3d.h"
#include "pixmap.h"

typedef struct {
    // from signlink
    int clientversion;
    int uid;
    const char *dns;
    char socketip[MAX_STR]; // arbitrary size

    // client statics
    int nodeid;
    int portoff;
    bool members;
    bool lowmem;
    int loop_cycle;
    bool started;
    char rsa_modulus[512 + 1];
    char rsa_exponent[128 + 1];
    int levelExperience[99];
    int oplogic1;
    int oplogic2;
    int oplogic3;
    int oplogic4;
    int oplogic5;
    int oplogic6;
    int oplogic7;
    int oplogic8;
    int oplogic9;
    int cyclelogic1;
    int cyclelogic2;
    int cyclelogic3;
    int cyclelogic4;
    int cyclelogic5;
    int cyclelogic6;
} ClientData;

struct Client {
    GameShell *shell;
    int archive_checksum[9];
    Jagfile *archive_title;
    PixFont *font_plain11;
    PixFont *font_plain12;
    PixFont *font_bold12;
    PixFont *font_quill8;
    PixMap *image_title0;
    PixMap *image_title1;
    PixMap *image_title2;
    PixMap *image_title3;
    PixMap *image_title4;
    PixMap *image_title5;
    PixMap *image_title6;
    PixMap *image_title7;
    PixMap *image_title8;
    bool redraw_background;
    bool flame_active;
    PixMap *area_chatback;
    PixMap *area_mapback;
    PixMap *area_sidebar;
    PixMap *area_viewport;
    PixMap *area_backbase1;
    PixMap *area_backbase2;
    PixMap *area_backhmid1;
    Pix8 *image_titlebox;
    Pix8 *image_titlebutton;
    Pix8 **image_runes;
    Pix24 *image_flames_left;
    Pix24 *image_flames_right;
    int *flame_gradient0;
    int *flame_gradient1;
    int *flame_gradient2;
    int *flame_gradient;
    int *flame_buffer0;
    int *flame_buffer1;
    int *flame_buffer2;
    int *flame_buffer3;
    int flame_cycle0;
    int flame_line_offset[256];
    int flameGradientCycle0;
    int flameGradientCycle1;
    int error_started;
    int error_loading;
    int error_host;
    int ingame;
    int drag_cycles;
    int title_screen_state;
    const char *login_message0;
    const char *login_message1;
    int title_login_field;
    char username[USERNAME_LENGTH + 1];
    char password[PASSWORD_LENGTH + 1];
    Pix24 *image_minimap;
    Pix8 *image_invback;
    Pix8 *image_chatback;
    Pix8 *image_mapback;
    Pix8 *image_backbase1;
    Pix8 *image_backbase2;
    Pix8 *image_backhmid1;
    Pix8 **image_sideicons;
    Pix24 *image_compass;
    Pix8 **image_mapscene;
    Pix24 **image_mapfunction;
    Pix24 **image_hitmarks;
    Pix24 **image_headicons;
    Pix24 *image_mapflag;
    Pix24 **image_crosses;
    Pix24 *image_mapdot0;
    Pix24 *image_mapdot1;
    Pix24 *image_mapdot2;
    Pix24 *image_mapdot3;
    Pix8 *image_scrollbar0;
    Pix8 *image_scrollbar1;
    Pix8 *image_redstone1;
    Pix8 *image_redstone2;
    Pix8 *image_redstone3;
    Pix8 *image_redstone1h;
    Pix8 *image_redstone2h;
    Pix8 *image_redstone1v;
    Pix8 *image_redstone2v;
    Pix8 *image_redstone3v;
    Pix8 *image_redstone1hv;
    Pix8 *image_redstone2hv;
    PixMap *area_backleft1;
    PixMap *area_backleft2;
    PixMap *area_backright1;
    PixMap *area_backright2;
    PixMap *area_backtop1;
    PixMap *area_backtop2;
    PixMap *area_backvmid1;
    PixMap *area_backvmid2;
    PixMap *area_backvmid3;
    PixMap *area_backhmid2;
    int *compass_mask_line_offsets;
    int *compass_mask_line_lengths;
    int *minimap_mask_line_offsets;
    int *minimap_mask_line_lengths;
    int *area_chatback_offsets;
    int *area_sidebar_offsets;
    int *area_viewport_offsets;
    int64_t server_seed;

    bool rights;
    Packet *out;
    Packet *in;
    Packet *login;
    struct isaac random_in;
    int packet_type;
    int last_packet_type0;
    int last_packet_type1;
    int last_packet_type2;
    int packet_size;
    int idle_net_cycles;
    int system_update_timer;
    int idle_timeout;
    int hint_type;
    int menu_size;
    bool menu_visible;

    int obj_selected;
    int spell_selected;
    int scene_state;
    int wave_count;

    int camera_anticheat_offset_x;
    int camera_anticheat_offset_z;
    int camera_anticheat_angle;
    int minimap_anticheat_angle;
    int minimap_zoom;
    int orbit_camera_yaw;
    int orbit_camera_pitch; // 128

    int minimap_level; // -1
    int flag_scene_tile_x;
    int flag_scene_tile_z;

    int player_count;
    int npc_count;

    int friend_count;
    int sticky_chat_interface_id;
    int chat_interface_id;
    int viewport_interface_id;
    int sidebar_interface_id;
    bool pressed_continue_option;
    int selected_tab;
    bool chatback_input_open;
    bool show_social_input;
    const char *modal_message;
    int in_multizone;
    int flashing_tab;
    bool design_gender_male;
    int *design_colors;

    ClientStream *stream;

    bool redraw_sidebar;
    bool redraw_chatback;
    bool redraw_sideicons;
    bool redraw_privacy_settings;

    int menu_area;
    int scene_delta;
    int tab_interface_id[15];
    int menu_x;
    int menu_y;
    int menu_width;
    int menu_height;
    char menu_option[MENU_OPTION_LENGTH][MAX_STR];
    int selected_area;
    int obj_drag_area;
    int public_chat_setting;
    int private_chat_setting;
    int trade_chat_setting;
    char social_message[MAX_STR]; // random length
    char social_input[CHAT_LENGTH + 1];
    char chatback_input[CHATBACK_LENGTH + 1];
    int camera_moved_write;

    // TODO snakecase/check if all inited below
    int menu_action[500];
    int menuParamA[500];
    int menuParamB[500];
    int menuParamC[500];
    int social_action;
    PlayerEntity *players[MAX_PLAYER_COUNT];
    Packet *player_appearance_buffer[MAX_PLAYER_COUNT];

    NpcEntity *npcs[MAX_NPC_COUNT];

    int local_pid; //-1
    PlayerEntity *local_player;
    LinkList *projectiles;
    LinkList *spotanims;
    LinkList *merged_locations;
    LinkList *spawned_locations;
    LinkList ****level_obj_stacks;
    int8_t (*levelTileFlags)[COLLISIONMAP_SIZE][COLLISIONMAP_SIZE];
    int (*levelHeightmap)[COLLISIONMAP_SIZE + 1][COLLISIONMAP_SIZE + 1];
    World3D *scene;
    CollisionMap *levelCollisionMap[COLLISIONMAP_LEVELS];
    int skillLevel[50];
    int skillBaseLevel[50];
    int skillExperience[50];
    int varps[VARPS_COUNT];
    int energy;
    int weightCarried;
    Component *chat_interface;
    int chat_scroll_height; // 78
    int chat_scroll_offset;
    bool scrollGrabbed;
    int scrollInputPadding;
    int activeMapFunctionCount;
    int activeMapFunctionX[1000];
    int activeMapFunctionZ[1000];
    Pix24 *activeMapFunctions[1000];
    int currentLevel;
    int npc_ids[MAX_NPC_COUNT];
    int player_ids[MAX_PLAYER_COUNT];
    int64_t friendName37[100];
    char *friendName[100];
    int friendWorld[100];
    int flagSceneTileX;
    int flagSceneTileZ;
    int message_type[100];
    char message_sender[100][USERNAME_LENGTH + 1];
    char message_text[100][CHAT_LENGTH + 1];
    char chat_typed[CHAT_LENGTH + 1];
    int split_private_chat;
    int scene_cycle;
    bool cutscene;
    int cameraPitchClamp;
    int orbitCameraX;
    int orbitCameraZ;
    int cameraX;
    int cameraY;
    int cameraZ;
    int cameraPitch;
    int cameraYaw;
    int orbitCameraYawVelocity;
    int orbitCameraPitchVelocity;
    bool cameraModifierEnabled[5];
    int cameraModifierJitter[5];
    int cameraModifierCycle[5];
    int cameraModifierWobbleSpeed[5];
    int cameraModifierWobbleScale[5];
    int tileLastOccupiedCycle[COLLISIONMAP_SIZE][COLLISIONMAP_SIZE];
    LinkList *locList;
    int heartbeatTimer;
    int varCache[VARPS_COUNT];
    int sceneCenterZoneX;
    int sceneCenterZoneZ;
    int sceneBaseTileX;
    int sceneBaseTileZ;
    int8_t **sceneMapLandData;
    int8_t **sceneMapLocData;
    int *sceneMapIndex;
    int *sceneMapLandDataIndexLength;
    int *sceneMapLocDataIndexLength;
    int sceneMapIndexLength;
    int mapLastBaseX;
    int mapLastBaseZ;
    bool update_design_model;
    int designIdentikits[7];
    int entityUpdateIds[MAX_PLAYER_COUNT];
    int entityUpdateCount;
    int entityRemovalIds[1000];
    int entityRemovalCount;
    int hint_npc;
    int hint_player;
    int hint_offset_x;
    int hint_offset_z;
    int hint_height;
    int hint_tile_x;
    int hint_tile_z;
    int baseX;
    int baseZ;
    int lastAddress;
    int daysSinceLastLogin;
    int daysSinceRecoveriesChanged;
    int unreadMessages;
    char reportAbuseInput[REPORT_ABUSE_LENGTH + 1];
    bool reportAbuseMuteOption;
    int cutsceneSrcLocalTileX;
    int cutsceneSrcLocalTileZ;
    int cutsceneSrcHeight;
    int cutsceneMoveSpeed;
    int cutsceneMoveAcceleration;
    int cutsceneDstLocalTileX;
    int cutsceneDstLocalTileZ;
    int cutsceneDstHeight;
    int cutsceneRotateSpeed;
    int cutsceneRotateAcceleration;
    int ignoreCount;
    int64_t ignoreName37[100];
    int overrideChat;
    int messageIds[100];
    int privateMessageCount;
    int cross_mode;
    int cross_cycle;
    int selected_cycle;
    int obj_drag_cycles;
    int objGrabX;
    int objGrabY;
    bool objGrabThreshold;
    int hoveredSlotParentId;
    int lastHoveredInterfaceId;
    int viewportHoveredInterfaceIndex;
    int hoveredSlot;
    int objDragSlot;
    int objDragInterfaceId;
    int mouseButtonsOption;
    int crossX;
    int crossY;
    int cameraOffsetCycle;
    int objSelectedInterface;
    int objSelectedSlot;
    const char *objSelectedName;
    int activeSpellFlags;
    char spellCaption[HALF_STR];
    int chatHoveredInterfaceIndex;
    int wildernessLevel;
    int worldLocationState;
    int cameraOffsetXModifier;   // 2
    int cameraOffsetZModifier;   // 2
    int cameraOffsetYawModifier; // 1
    int minimapAngleModifier;    // 2
    int minimapZoomModifier;     // 1
    int sidebarHoveredInterfaceIndex;
    Pix24 *genderButtonImage0;
    Pix24 *genderButtonImage1;
    int chatEffects;
    int bfsDirection[COLLISIONMAP_SIZE][COLLISIONMAP_SIZE];
    int bfsCost[COLLISIONMAP_SIZE][COLLISIONMAP_SIZE];
    int bfsStepX[BFS_STEP_SIZE];
    int bfsStepZ[BFS_STEP_SIZE];
    int tryMoveNearest;
    int selectedInterface;
    int selectedItem;
    int objInterface;
    int activeSpellId;
    int64_t social_name37;
    int reportAbuseInterfaceID;
    int chatCount;
    int projectX; //-1
    int projectY; //-1
    int chatWidth[MAX_CHATS];
    int chatHeight[MAX_CHATS];
    int chatX[MAX_CHATS];
    int chatY[MAX_CHATS];
    int chatColors[MAX_CHATS];
    int chatStyles[MAX_CHATS];
    int chatTimers[MAX_CHATS];
    char chats[MAX_CHATS][MAX_STR];
    int8_t *textureBuffer; // 16384
    bool wave_enabled;     // true
    int wave_delay[50];
    int wave_ids[50];
    int wave_loops[50];
    int last_wave_id;    //-1
    int last_wave_loops; //-1
    uint64_t last_wave_start_time;
    int last_wave_length;
    int nextMusicDelay;
    int midiActive; // true
    int midiCrc;
    int midiSize;
    char currentMidi[MAX_STR];
};

void client_init_global(void);
Client *client_new(void);
void client_load(Client *c);
void client_update(Client *c);
void client_draw(Client *c);
void client_unload(Client *c);
void client_free(Client *c);
Jagfile *load_archive(Client *c, const char *name, int crc, const char *display_name, int progress);
void client_load_title_background(Client *c);
void client_load_title_images(Client *c);
void client_draw_progress(Client *c, const char *message, int progress);
void client_load_title(Client *c);
void client_draw_title_screen(Client *c);
void client_draw_error(Client *c);
void client_update_flame_buffer(Client *c, Pix8 *image);
void client_update_title(Client *c);
void client_login(Client *c, const char *username, const char *password, bool reconnect);
void client_draw_game(Client *c);
void client_validate_character_design(Client *c);
void client_prepare_game_screen(Client *c);
void client_unload_title(Client *c);
void client_draw_sidebar(Client *c);
void client_draw_menu(Client *c);
void client_draw_chatback(Client *c);
void client_draw_minimap(Client *c);
void client_draw_scene(Client *c);
void client_update_game(Client *c);
void client_try_reconnect(Client *c);
void client_logout(Client *c);
void reset_interface_animation(int id);
bool client_read(Client *c);
int client_execute_clientscript1(Client *c, Component *component, int scriptId);
bool client_execute_interface_script(Client *c, Component *com);
bool client_update_interface_animation(Client *c, int id, int delta);
void client_draw_on_minimap(Client *c, int dy, Pix24 *image, int dx);
void client_handle_scroll_input(Client *c, int mouseX, int mouseY, int scrollableHeight, int height, bool redraw, int left, int top, Component *component);
void client_draw_scrollbar(Client *c, int x, int y, int scrollY, int scrollHeight, int height);
bool client_is_friend(Client *c, const char *username);
void orbitCamera(Client *c, int targetX, int targetY, int targetZ, int yaw, int pitch, int distance);
int getHeightmapY(Client *c, int level, int sceneX, int sceneZ);
int getTopLevel(Client *c);
int getTopLevelCutscene(Client *c);
void pushPlayers(Client *c);
void pushNpcs(Client *c);
void pushProjectiles(Client *c);
void pushSpotanims(Client *c);
void pushLocs(Client *c);
int getInfo(World3D *scene, int level, int x, int z, int bitset);
void updateVarp(Client *c, int id);
void client_add_message(Client *c, int type, const char *text, const char *sender);
void getNpcPos(Client *c, Packet *buf, int size);
void getNpcPosExtended(Client *c, Packet *buf, int size);
void getNpcPosNewVis(Client *c, Packet *buf, int size);
void getNpcPosOldVis(Client *c, Packet *buf, int size);
void readZonePacket(Client *c, Packet *buf, int opcode);
void sortObjStacks(Client *c, int x, int z);
void addLoc(Client *c, int level, int x, int z, int id, int angle, int shape, int layer);
void closeInterfaces(Client *c);
void createMinimap(Client *c, int level);
void drawMinimapLoc(Client *c, int tileX, int tileZ, int level, int wallRgb, int doorRgb);
void getPlayer(Client *c, Packet *buf, int size);
void getPlayerExtended2(Client *c, PlayerEntity *player, int index, int mask, Packet *buf);
void getPlayerExtended(Client *c, Packet *buf, int size);
void getPlayerNewVis(Client *c, int size, Packet *buf);
void getPlayerOldVis(Client *c, Packet *buf, int size);
void getPlayerLocal(Client *c, Packet *buf, int size);
void handleScrollInput(Client *c, int mouseX, int mouseY, int scrollableHeight, int height, bool redraw, int left, int top, Component *component);
bool handleSocialMenuOption(Client *c, Component *component);
void handleInterfaceInput(Client *c, Component *com, int mouseX, int mouseY, int x, int y, int scrollPosition);
void handlePrivateChatInput(Client *c, int mouse_x, int mouse_y);
void addNpcOptions(Client *cl, NpcType *npc, int a, int b, int c);
void addPlayerOptions(Client *cl, PlayerEntity *player, int a, int b, int c);
void client_update_interface_content(Client *c, Component *component);
bool isAddFriendOption(Client *c, int option);
void showContextMenu(Client *c);
void updateMergeLocs(Client *c);
void updateEntityChats(Client *c);
void updatePlayers(Client *c);
void handleViewportOptions(Client *c);
bool handleInterfaceAction(Client *c, Component *com);
void handleChatMouseInput(Client *c, int mouseX, int mouseY);
void client_handle_input(Client *c);
void projectFromGround(Client *c, PathingEntity *entity, int height);
void projectFromGround2(Client *c, int x, int height, int z);
void project(Client *c, int x, int y, int z);
void client_run_flames(Client *c);
void client_set_lowmem(void);
void client_set_highmem(void);
void* client_openurl(const char* name, int* size);
