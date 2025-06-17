#ifdef client
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_arch_dreamcast) || defined(__NDS__)
#include <malloc.h>
#endif

#include "../allocator.h"
#include "../animbase.h"
#include "../animframe.h"
#include "../client.h"
#include "../clientstream.h"
#include "../collisionmap.h"
#include "../component.h"
#include "../custom.h"
#include "../datastruct/jstring.h"
#include "../datastruct/linklist.h"
#include "../defines.h"
#include "../flotype.h"
#include "../gameshell.h"
#include "../idktype.h"
#include "../inputtracking.h"
#include "../jagfile.h"
#include "../locaddentity.h"
#include "../locentity.h"
#include "../locmergeentity.h"
#include "../loctype.h"
#include "../model.h"
#include "../npcentity.h"
#include "../npctype.h"
#include "../objstackentity.h"
#include "../objtype.h"
#include "../packet.h"
#include "../pix24.h"
#include "../pix3d.h"
#include "../pix8.h"
#include "../pixmap.h"
#include "../platform.h"
#include "../playerentity.h"
#include "../projectileentity.h"
#include "../protocol.h"
#include "../seqtype.h"
#include "../sound/wave.h"
#include "../spotanimentity.h"
#include "../spotanimtype.h"
#include "../thirdparty/bzip.h"
#include "../thirdparty/ini.h"
#include "../thirdparty/isaac.h"
#include "../varptype.h"
#include "../wordenc/wordfilter.h"
#include "../wordenc/wordpack.h"
#include "../world.h"
#include "../world3d.h"

extern int DESIGN_BODY_COLOR_LENGTH[];
extern int *DESIGN_BODY_COLOR[];
extern int DESIGN_HAIR_COLOR[];

extern Pix2D _Pix2D;
extern Pix3D _Pix3D;
extern InputTracking _InputTracking;
extern ModelData _Model;
extern ObjTypeData _ObjType;
extern ComponentData _Component;
extern IdkTypeData _IdkType;
extern VarpTypeData _VarpType;
extern SeqTypeData _SeqType;
extern LocTypeData _LocType;
extern SpotAnimTypeData _SpotAnimType;
extern NpcTypeData _NpcType;
extern PlayerEntityData _PlayerEntity;
extern WaveData _Wave;
extern WorldData _World;
extern SceneData _World3D;
extern Custom _Custom;

ClientData _Client = {
    .clientversion = 225,
    .members = true,
    .nodeid = 10,
    .socketip = "localhost",
#ifdef WITH_RSA_BIGINT
    // original rsa keys in dec, only used with js bigints: openssl with dec requires bigger result array and it doesn't work with rsa-tiny
    .rsa_exponent = "58778699976184461502525193738213253649000149147835990136706041084440742975821",
    .rsa_modulus = "7162900525229798032761816791230527296329313291232324290237849263501208207972894053929065636522363163621000728841182238772712427862772219676577293600221789",
#else
    // original rsa keys in hex
    .rsa_exponent = "81f390b2cf8ca7039ee507975951d5a0b15a87bf8b3f99c966834118c50fd94d", // pad exponent to an even number (prefix a 0 if needed)
    .rsa_modulus = "88c38748a58228f7261cdc340b5691d7d0975dee0ecdb717609e6bf971eb3fe723ef9d130e4686813739768ad9472eb46d8bfcc042c1a5fcb05e931f632eea5d",
#endif
};

const int CHAT_COLORS[6] = {YELLOW, RED, GREEN, CYAN, MAGENTA, WHITE};
const int LOC_SHAPE_TO_LAYER[23] = {0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3};

// TODO add more static funcs here
static void client_draw_interface(Client *c, Component *com, int x, int y, int scrollY);
static void client_scenemap_free(Client *c);
static void client_build_scene(Client *c);
static void client_clear_caches(void);
static void client_update_orbit_camera(Client *c);

void client_init_global(void) {
    int acc = 0;
    for (int i = 0; i < 99; i++) {
        int level = i + 1;
        int delta = (int)((double)level + pow(2.0, (double)level / 7.0) * 300.0);
        acc += delta;
        _Client.levelExperience[i] = acc / 4;
    }
}

void client_load(Client *c) {
// TODO missing bits
// String vendor = System.getProperties().getProperty("java.vendor");
// if (vendor.toLowerCase().indexOf("sun") != -1 || vendor.toLowerCase().indexOf("apple") != -1) {
// 	signlink.sunjava = true;
// }
// if (signlink.sunjava) {
// 	super.mindel = 5;
// }

// NOTE: avoid blocking midi for emscripten due to slow loading on web
#ifndef __EMSCRIPTEN__
    if (!_Client.lowmem) {
        platform_set_midi("scape_main", 12345678, 40000);
    }
#endif

    if (_Client.started) {
        c->error_started = true;
        return;
    }

    _Client.started = true;

    // bool good = c->shell->window;
    // const char* host = this.getHost();
    // if (host.endsWith("2004scape.org")) {
    // 	// intended domain for players
    // 	good = true;
    // }
    // if (host.endsWith("localhost") || host.endsWith("127.0.0.1")) {
    // 	// allow localhost
    // 	good = true;
    // }
    // if (host.startsWith("192.168.") || host.startsWith("172.16.") || host.startsWith("10.")) {
    // 	// allow lan
    // 	good = true;
    // }
    // if (!good) {
    // 	this.errorHost = true;
    // 	return;
    // }

#if defined(__EMSCRIPTEN__) && (!defined(SDL) || SDL == 0)
    int retry = 5;
#endif
    c->archive_checksum[8] = 0;
    while (c->archive_checksum[8] == 0) {
        client_draw_progress(c, "Connecting to fileserver", 10);
#if defined(__EMSCRIPTEN__) && (!defined(SDL) || SDL == 0)
        char message[PATH_MAX];
        sprintf(message, "crc%d", (int)(jrand() * 9.9999999e7));
        int size = 0;
        int8_t *buffer = client_openurl(message, &size);
        if (!buffer) {
            for (int i = retry; i > 0; i--) {
                sprintf(message, "Error loading - Will retry in %d secs.", i);
                client_draw_progress(c, message, 10);
                rs2_sleep(1000);
            }
            retry *= 2;
            if (retry > 60) {
                retry = 60;
            }
        } else {
            Packet *checksums = packet_new(buffer, size); // 36
            for (int i = 0; i < 9; i++) {
                c->archive_checksum[i] = g4(checksums);
            }
        }
#else
        // TODO: hardcoded for now add openurl
        c->archive_checksum[0] = 0;
        c->archive_checksum[1] = -430779560;
        c->archive_checksum[2] = -1494598746;
        c->archive_checksum[3] = 251806152;
        c->archive_checksum[4] = -343404987;
        c->archive_checksum[5] = -2000991154;
        c->archive_checksum[6] = 1703545114;
        c->archive_checksum[7] = 1570981179;
        c->archive_checksum[8] = -1532605973;
#endif
    }

    c->archive_title = load_archive(c, "title", c->archive_checksum[1], "title screen", 10);
    if (!c->archive_title) {
        c->error_loading = true;
        return;
    }
    c->font_plain11 = pixfont_from_archive(c->archive_title, "p11");
    c->font_plain12 = pixfont_from_archive(c->archive_title, "p12");
    c->font_bold12 = pixfont_from_archive(c->archive_title, "b12");
    c->font_quill8 = pixfont_from_archive(c->archive_title, "q8");
    client_load_title_background(c);
    client_load_title_images(c);

    Jagfile *config = load_archive(c, "config", c->archive_checksum[2], "config", 15);
    Jagfile *inter = load_archive(c, "interface", c->archive_checksum[3], "interface", 20);
    Jagfile *media = load_archive(c, "media", c->archive_checksum[4], "2d graphics", 30);
    Jagfile *models = load_archive(c, "models", c->archive_checksum[5], "3d graphics", 40);
    Jagfile *textures = load_archive(c, "textures", c->archive_checksum[6], "textures", 60);
    Jagfile *wordenc = load_archive(c, "wordenc", c->archive_checksum[7], "chat system", 65);
    Jagfile *sounds = load_archive(c, "sounds", c->archive_checksum[8], "sound effects", 70);

    if (!config || !inter || !media || !models || !textures || !wordenc || !sounds) {
        c->error_loading = true;
        return;
    }

    c->levelTileFlags = calloc(4, sizeof(*c->levelTileFlags));
    c->levelHeightmap = calloc(4, sizeof(*c->levelHeightmap));
    c->scene = world3d_new(c->levelHeightmap, 104, 4, 104);
    for (int level = 0; level < 4; level++) {
        c->levelCollisionMap[level] = collisionmap_new(104, 104);
    }
    c->image_minimap = pix24_new(512, 512, false);
    client_draw_progress(c, "Unpacking media", 75);
    c->image_invback = pix8_from_archive(media, "invback", 0);
    c->image_chatback = pix8_from_archive(media, "chatback", 0);
    c->image_mapback = pix8_from_archive(media, "mapback", 0);
    c->image_backbase1 = pix8_from_archive(media, "backbase1", 0);
    c->image_backbase2 = pix8_from_archive(media, "backbase2", 0);
    c->image_backhmid1 = pix8_from_archive(media, "backhmid1", 0);
    for (int i = 0; i < 13; i++) {
        c->image_sideicons[i] = pix8_from_archive(media, "sideicons", i);
    }
    c->image_compass = pix24_from_archive(media, "compass", 0);

    for (int i = 0; i < 50; i++) {
        if (_Custom.hide_debug_sprite) {
            if (i == 22) {
                // weird debug sprite along water
                continue;
            }
        }

        c->image_mapscene[i] = pix8_from_archive(media, "mapscene", i);
        if (!c->image_mapscene[i]) {
            break;
        }
    }
    for (int i = 0; i < 50; i++) {
        c->image_mapfunction[i] = pix24_from_archive(media, "mapfunction", i);
        if (!c->image_mapfunction[i]) {
            break;
        }
    }
    for (int i = 0; i < 20; i++) {
        c->image_hitmarks[i] = pix24_from_archive(media, "hitmarks", i);
        if (!c->image_hitmarks[i]) {
            break;
        }
    }
    for (int i = 0; i < 20; i++) {
        c->image_headicons[i] = pix24_from_archive(media, "headicons", i);
        if (!c->image_headicons[i]) {
            break;
        }
    }
    c->image_mapflag = pix24_from_archive(media, "mapflag", 0);
    for (int i = 0; i < 8; i++) {
        c->image_crosses[i] = pix24_from_archive(media, "cross", i);
    }
    c->image_mapdot0 = pix24_from_archive(media, "mapdots", 0);
    c->image_mapdot1 = pix24_from_archive(media, "mapdots", 1);
    c->image_mapdot2 = pix24_from_archive(media, "mapdots", 2);
    c->image_mapdot3 = pix24_from_archive(media, "mapdots", 3);
    c->image_scrollbar0 = pix8_from_archive(media, "scrollbar", 0);
    c->image_scrollbar1 = pix8_from_archive(media, "scrollbar", 1);
    c->image_redstone1 = pix8_from_archive(media, "redstone1", 0);
    c->image_redstone2 = pix8_from_archive(media, "redstone2", 0);
    c->image_redstone3 = pix8_from_archive(media, "redstone3", 0);
    c->image_redstone1h = pix8_from_archive(media, "redstone1", 0);
    pix8_flip_horizontally(c->image_redstone1h);
    c->image_redstone2h = pix8_from_archive(media, "redstone2", 0);
    pix8_flip_horizontally(c->image_redstone2h);
    c->image_redstone1v = pix8_from_archive(media, "redstone1", 0);
    pix8_flip_vertically(c->image_redstone1v);
    c->image_redstone2v = pix8_from_archive(media, "redstone2", 0);
    pix8_flip_vertically(c->image_redstone2v);
    c->image_redstone3v = pix8_from_archive(media, "redstone3", 0);
    pix8_flip_vertically(c->image_redstone3v);
    c->image_redstone1hv = pix8_from_archive(media, "redstone1", 0);
    pix8_flip_horizontally(c->image_redstone1hv);
    pix8_flip_vertically(c->image_redstone1hv);
    c->image_redstone2hv = pix8_from_archive(media, "redstone2", 0);
    pix8_flip_horizontally(c->image_redstone2hv);
    pix8_flip_vertically(c->image_redstone2hv);
    Pix24 *backleft1 = pix24_from_archive(media, "backleft1", 0);
    c->area_backleft1 = pixmap_new(backleft1->width, backleft1->height);
    pix24_blit_opaque(backleft1, 0, 0);
    Pix24 *backleft2 = pix24_from_archive(media, "backleft2", 0);
    c->area_backleft2 = pixmap_new(backleft2->width, backleft2->height);
    pix24_blit_opaque(backleft2, 0, 0);
    Pix24 *backright1 = pix24_from_archive(media, "backright1", 0);
    c->area_backright1 = pixmap_new(backright1->width, backright1->height);
    pix24_blit_opaque(backright1, 0, 0);
    Pix24 *backright2 = pix24_from_archive(media, "backright2", 0);
    c->area_backright2 = pixmap_new(backright2->width, backright2->height);
    pix24_blit_opaque(backright2, 0, 0);
    Pix24 *backtop1 = pix24_from_archive(media, "backtop1", 0);
    c->area_backtop1 = pixmap_new(backtop1->width, backtop1->height);
    pix24_blit_opaque(backtop1, 0, 0);
    Pix24 *backtop2 = pix24_from_archive(media, "backtop2", 0);
    c->area_backtop2 = pixmap_new(backtop2->width, backtop2->height);
    pix24_blit_opaque(backtop2, 0, 0);
    Pix24 *backvmid1 = pix24_from_archive(media, "backvmid1", 0);
    c->area_backvmid1 = pixmap_new(backvmid1->width, backvmid1->height);
    pix24_blit_opaque(backvmid1, 0, 0);
    Pix24 *backvmid2 = pix24_from_archive(media, "backvmid2", 0);
    c->area_backvmid2 = pixmap_new(backvmid2->width, backvmid2->height);
    pix24_blit_opaque(backvmid2, 0, 0);
    Pix24 *backvmid3 = pix24_from_archive(media, "backvmid3", 0);
    c->area_backvmid3 = pixmap_new(backvmid3->width, backvmid3->height);
    pix24_blit_opaque(backvmid3, 0, 0);
    Pix24 *backhmid2 = pix24_from_archive(media, "backhmid2", 0);
    c->area_backhmid2 = pixmap_new(backhmid2->width, backhmid2->height);
    pix24_blit_opaque(backhmid2, 0, 0);

    int rand_r = (int)(jrand() * 21.0) - 10;
    int rand_g = (int)(jrand() * 21.0) - 10;
    int rand_b = (int)(jrand() * 21.0) - 10;
    int _rand = (int)(jrand() * 41.0) - 20;
    for (int i = 0; i < 50; i++) {
        if (c->image_mapfunction[i]) {
            pix24_translate(c->image_mapfunction[i], rand_r + _rand, rand_g + _rand, rand_b + _rand);
        }

        if (c->image_mapscene[i]) {
            pix8_translate(c->image_mapscene[i], rand_r + _rand, rand_g + _rand, rand_b + _rand);
        }
    }

    client_draw_progress(c, "Unpacking textures", 80);
    pix3d_unpack_textures(textures);
    pix3d_set_brightness(0.8);
    pix3d_init_pool(PIX3D_POOL_COUNT);

    client_draw_progress(c, "Unpacking models", 83);
    model_unpack(models);
    animbase_unpack(models);
    animframe_unpack(models);

    client_draw_progress(c, "Unpacking config", 86);
    seqtype_unpack(config);
    loctype_unpack(config);
    flotype_unpack(config);
    objtype_unpack(config);
    npctype_unpack(config);
    idktype_unpack(config);
    spotanimtype_unpack(config);
    varptype_unpack(config);

    _ObjType.membersWorld = _Client.members;
    if (!_Client.lowmem) {
        client_draw_progress(c, "Unpacking sounds", 90);
        Packet *sound_dat = jagfile_to_packet(sounds, "sounds.dat");
        wave_unpack(sound_dat);
        packet_free(sound_dat);
    }

    client_draw_progress(c, "Unpacking interfaces", 92);
    PixFont *fonts[] = {c->font_plain11, c->font_plain12, c->font_bold12, c->font_quill8};
    component_unpack(inter, media, fonts);

    client_draw_progress(c, "Preparing game engine", 97);
    for (int y = 0; y < 33; y++) {
        int left = 999;
        int right = 0;
        for (int x = 0; x < 35; x++) {
            if (c->image_mapback->pixels[x + y * c->image_mapback->width] == 0) {
                if (left == 999) {
                    left = x;
                }
            } else if (left != 999) {
                right = x;
                break;
            }
        }
        c->compass_mask_line_offsets[y] = left;
        c->compass_mask_line_lengths[y] = right - left;
    }

    for (int y = 9; y < 160; y++) {
        int left = 999;
        int right = 0;
        for (int x = 10; x < 168; x++) {
            if (c->image_mapback->pixels[x + y * c->image_mapback->width] == 0 && (x > 34 || y > 34)) {
                if (left == 999) {
                    left = x;
                }
            } else if (left != 999) {
                right = x;
                break;
            }
        }
        c->minimap_mask_line_offsets[y - 9] = left - 21;
        c->minimap_mask_line_lengths[y - 9] = right - left;
    }

    pix3d_init3d(479, 96);
    c->area_chatback_offsets = _Pix3D.line_offset;
    pix3d_init3d(190, 261);
    c->area_sidebar_offsets = _Pix3D.line_offset;
    pix3d_init3d(512, 334);
    c->area_viewport_offsets = _Pix3D.line_offset;

    int *distance = malloc(9 * sizeof(int));
    for (int x = 0; x < 9; x++) {
        int angle = x * 32 + 128 + 15;
        int offset = angle * 3 + 600;
        int sin = _Pix3D.sin_table[angle];
        distance[x] = offset * sin >> 16;
    }

    world3d_init(512, 334, 500, 800, distance);
    free(distance);
    // TODO wordfilter
    // wordfilter_unpack(wordenc);

    pix24_free(backleft1);
    pix24_free(backleft2);
    pix24_free(backright1);
    pix24_free(backright2);
    pix24_free(backtop1);
    pix24_free(backtop2);
    pix24_free(backvmid1);
    pix24_free(backvmid2);
    pix24_free(backvmid3);
    pix24_free(backhmid2);

    jagfile_free(config);
    jagfile_free(inter);
    jagfile_free(media);
    jagfile_free(models);
    jagfile_free(textures);
    jagfile_free(wordenc);
    jagfile_free(sounds);
    // } catch (Exception ex) {
    // 	ex.printStackTrace();
    // 	this.errorLoading = true;
    // }

// NOTE: we can't grow it so it needs to fit the max usage, left value is shifted to MiB (arbitrary value)
#if defined(_arch_dreamcast) || defined(__NDS__)
    malloc_stats();
    if (!bump_allocator_init(8 << 20)) {
#else
    if (!(_Client.lowmem ? bump_allocator_init(16 << 20) : bump_allocator_init(32 << 20))) {
#endif
        c->error_loading = true;
    }

    // network init happens here after game loads instead of in platform_init because being connected disables fast-forward in emulators
    if (!clientstream_init()) {
        c->error_loading = true;
    }

#ifdef __EMSCRIPTEN__
    if (!_Client.lowmem) {
        platform_set_midi("scape_main", 12345678, 40000);
    }
#endif

// TODO temp: wait for wiiu and switch touch input fixes, melonds 32mb emulation
#if defined(__WIIU__) || defined(__SWITCH__) || defined(__NDS__)
    client_login(c, c->username, c->password, false);
#endif

#if defined(_arch_dreamcast) || defined(__NDS__)
    // it's fine for the consoles memory to be full here, it frees the login screen after this
    malloc_stats();
#endif
}

void client_load_title_background(Client *c) {
    Pix24 *title = pix24_from_jpeg(c->archive_title, "title.dat");
    pixmap_bind(c->image_title0);
    pix24_blit_opaque(title, 0, 0);

    pixmap_bind(c->image_title1);
    pix24_blit_opaque(title, -661, 0);

    pixmap_bind(c->image_title2);
    pix24_blit_opaque(title, -128, 0);

    pixmap_bind(c->image_title3);
    pix24_blit_opaque(title, -214, -386);

    pixmap_bind(c->image_title4);
    pix24_blit_opaque(title, -214, -186);

    pixmap_bind(c->image_title5);
    pix24_blit_opaque(title, 0, -265);

    pixmap_bind(c->image_title6);
    pix24_blit_opaque(title, -574, -265);

    pixmap_bind(c->image_title7);
    pix24_blit_opaque(title, -128, -186);

    pixmap_bind(c->image_title8);
    pix24_blit_opaque(title, -574, -186);

    int *mirror = malloc(title->width * sizeof(int));
    for (int y = 0; y < title->height; y++) {
        for (int x = 0; x < title->width; x++) {
            mirror[x] = title->pixels[title->width + title->width * y - x - 1];
        }

        if (title->width >= 0) {
            memcpy(&title->pixels[title->width * y], mirror, title->width * sizeof(int));
        }
    }
    free(mirror);

    pixmap_bind(c->image_title0);
    pix24_blit_opaque(title, 394, 0);

    pixmap_bind(c->image_title1);
    pix24_blit_opaque(title, -267, 0);

    pixmap_bind(c->image_title2);
    pix24_blit_opaque(title, 266, 0);

    pixmap_bind(c->image_title3);
    pix24_blit_opaque(title, 180, -386);

    pixmap_bind(c->image_title4);
    pix24_blit_opaque(title, 180, -186);

    pixmap_bind(c->image_title5);
    pix24_blit_opaque(title, 394, -265);

    pixmap_bind(c->image_title6);
    pix24_blit_opaque(title, -180, -265);

    pixmap_bind(c->image_title7);
    pix24_blit_opaque(title, 212, -186);

    pixmap_bind(c->image_title8);
    pix24_blit_opaque(title, -180, -186);

    pix24_free(title);
    title = pix24_from_archive(c->archive_title, "logo", 0);
    pixmap_bind(c->image_title2);
    pix24_draw(title, c->shell->screen_width / 2 - title->width / 2 - 128, 18);

    pix24_free(title);
    title = NULL;
}

void client_update_flame_buffer(Client *c, Pix8 *image) {
    int flame_height = 256;
    memset(c->flame_buffer0, 0, FLAME_BUFFER_SIZE * sizeof(int));

    for (int i = 0; i < 5000; i++) {
        int index = (int)(jrand() * 128.0 * (double)flame_height);
        c->flame_buffer0[index] = (int)(jrand() * 256.0);
    }

    for (int i = 0; i < 20; i++) {
        for (int y = 1; y < flame_height - 1; y++) {
            for (int x = 1; x < 127; x++) {
                int index = x + (y << 7);
                c->flame_buffer1[index] = (c->flame_buffer0[index - 1] + c->flame_buffer0[index + 1] + c->flame_buffer0[index - 128] + c->flame_buffer0[index + 128]) / 4;
            }
        }

        int *last = c->flame_buffer0;
        c->flame_buffer0 = c->flame_buffer1;
        c->flame_buffer1 = last;
    }

    if (image) {
        int off = 0;

        for (int y = 0; y < image->height; y++) {
            for (int x = 0; x < image->width; x++) {
                if (image->pixels[off++] != 0) {
                    int x0 = x + image->crop_x + 16;
                    int y0 = y + image->crop_y + 16;
                    int index = x0 + (y0 << 7);
                    c->flame_buffer0[index] = 0;
                }
            }
        }
    }
}

void client_load_title_images(Client *c) {
    c->image_titlebox = pix8_from_archive(c->archive_title, "titlebox", 0);
    c->image_titlebutton = pix8_from_archive(c->archive_title, "titlebutton", 0);
#ifdef DISABLE_FLAMES
    // TODO: redraw behind "flames" when there's any spare memory, gets freed after login
    // c->image_flames_left = pix24_new(128, 265, false);
    // c->image_flames_right = pix24_new(128, 265, false);
    // memcpy(c->image_flames_left->pixels, c->image_title0->pixels, 33920 * sizeof(int));
    // memcpy(c->image_flames_right->pixels, c->image_title1->pixels, 33920 * sizeof(int));
#else
    c->image_runes = calloc(12, sizeof(Pix8 *));
    for (int i = 0; i < 12; i++) {
        c->image_runes[i] = pix8_from_archive(c->archive_title, "runes", i);
    }
    c->image_flames_left = pix24_new(128, 265, false);
    c->image_flames_right = pix24_new(128, 265, false);
    memcpy(c->image_flames_left->pixels, c->image_title0->pixels, 33920 * sizeof(int));
    memcpy(c->image_flames_right->pixels, c->image_title1->pixels, 33920 * sizeof(int));
    c->flame_gradient0 = calloc(256, sizeof(int));
    for (int i = 0; i < 64; i++) {
        c->flame_gradient0[i] = i * 262144;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient0[i + 64] = i * 1024 + RED;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient0[i + 128] = i * 4 + YELLOW;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient0[i + 192] = WHITE;
    }
    c->flame_gradient1 = calloc(256, sizeof(int));
    for (int i = 0; i < 64; i++) {
        c->flame_gradient1[i] = i * 1024;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient1[i + 64] = i * 4 + GREEN;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient1[i + 128] = i * 262144 + CYAN;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient1[i + 192] = WHITE;
    }
    c->flame_gradient2 = calloc(256, sizeof(int));
    for (int i = 0; i < 64; i++) {
        c->flame_gradient2[i] = i * 4;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient2[i + 64] = i * 262144 + BLUE;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient2[i + 128] = i * 1024 + MAGENTA;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient2[i + 192] = WHITE;
    }
    c->flame_gradient = calloc(256, sizeof(int));
    c->flame_buffer0 = calloc(FLAME_BUFFER_SIZE, sizeof(int));
    c->flame_buffer1 = calloc(FLAME_BUFFER_SIZE, sizeof(int));
    client_update_flame_buffer(c, NULL);
    c->flame_buffer3 = calloc(FLAME_BUFFER_SIZE, sizeof(int));
    c->flame_buffer2 = calloc(FLAME_BUFFER_SIZE, sizeof(int));
    client_draw_progress(c, "Connecting to fileserver", 10);
    if (!c->flame_active) {
        c->flame_active = true;
    }
#endif
}

static void client_update_flames(Client *c) {
    int height = 256;
    for (int x = 10; x < 117; x++) {
        int _rand = (int)(jrand() * 100.0);
        if (_rand < 50) {
            c->flame_buffer3[x + ((height - 2) << 7)] = 255;
        }
    }

    for (int l = 0; l < 100; l++) {
        int x = (int)(jrand() * 124.0) + 2;
        int y = (int)(jrand() * 128.0) + 128;
        int index = x + (y << 7);
        c->flame_buffer3[index] = 192;
    }

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < 127; x++) {
            int index = x + (y << 7);
            c->flame_buffer2[index] = (c->flame_buffer3[index - 1] + c->flame_buffer3[index + 1] + c->flame_buffer3[index - 128] + c->flame_buffer3[index + 128]) / 4;
        }
    }

    c->flame_cycle0 += 128;
    if (c->flame_cycle0 > FLAME_BUFFER_SIZE) {
        c->flame_cycle0 -= FLAME_BUFFER_SIZE;
        int _rand = (int)(jrand() * 12.0);
        client_update_flame_buffer(c, c->image_runes[_rand]);
    }

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < 127; x++) {
            int index = x + (y << 7);
            int intensity = c->flame_buffer2[index + 128] - c->flame_buffer0[index + c->flame_cycle0 & FLAME_BUFFER_SIZE - 1] / 5;
            if (intensity < 0) {
                intensity = 0;
            }
            c->flame_buffer3[index] = intensity;
        }
    }

    for (int y = 0; y < height - 1; y++) {
        c->flame_line_offset[y] = c->flame_line_offset[y + 1];
    }

    c->flame_line_offset[height - 1] = (int)(sin((double)_Client.loop_cycle / 14.0) * 16.0 + sin((double)_Client.loop_cycle / 15.0) * 14.0 + sin((double)_Client.loop_cycle / 16.0) * 12.0);

    if (c->flameGradientCycle0 > 0) {
        c->flameGradientCycle0 -= 4;
    }

    if (c->flameGradientCycle1 > 0) {
        c->flameGradientCycle1 -= 4;
    }

    if (c->flameGradientCycle0 == 0 && c->flameGradientCycle1 == 0) {
        int _rand = (int)(jrand() * 2000.0);

        if (_rand == 0) {
            c->flameGradientCycle0 = 1024;
        } else if (_rand == 1) {
            c->flameGradientCycle1 = 1024;
        }
    }
}

static int mix(int src, int alpha, int dst) {
    int invAlpha = 256 - alpha;
    return (((src & 0xff00ff) * invAlpha + (dst & 0xff00ff) * alpha & 0xff00ff00) + ((src & 0xff00) * invAlpha + (dst & 0xff00) * alpha & 0xff0000)) >> 8;
}

static void client_draw_flames(Client *c) {
    int height = 256;

    if (c->flameGradientCycle0 > 0) {
        for (int i = 0; i < 256; i++) {
            if (c->flameGradientCycle0 > 768) {
                c->flame_gradient[i] = mix(c->flame_gradient0[i], 1024 - c->flameGradientCycle0, c->flame_gradient1[i]);
            } else if (c->flameGradientCycle0 > 256) {
                c->flame_gradient[i] = c->flame_gradient1[i];
            } else {
                c->flame_gradient[i] = mix(c->flame_gradient1[i], 256 - c->flameGradientCycle0, c->flame_gradient0[i]);
            }
        }
    } else if (c->flameGradientCycle1 > 0) {
        for (int i = 0; i < 256; i++) {
            if (c->flameGradientCycle1 > 768) {
                c->flame_gradient[i] = mix(c->flame_gradient0[i], 1024 - c->flameGradientCycle1, c->flame_gradient2[i]);
            } else if (c->flameGradientCycle1 > 256) {
                c->flame_gradient[i] = c->flame_gradient2[i];
            } else {
                c->flame_gradient[i] = mix(c->flame_gradient2[i], 256 - c->flameGradientCycle1, c->flame_gradient0[i]);
            }
        }
    } else {
        memcpy(c->flame_gradient, c->flame_gradient0, 256 * sizeof(int));
    }
    memcpy(c->image_title0->pixels, c->image_flames_left->pixels, 33920 * sizeof(int));

    int srcOffset = 0;
    int dstOffset = 1152;

    for (int y = 1; y < height - 1; y++) {
        int offset = c->flame_line_offset[y] * (height - y) / height;
        int step = offset + 22;
        if (step < 0) {
            step = 0;
        }
        srcOffset += step;
        for (int x = step; x < 128; x++) {
            int value = c->flame_buffer3[srcOffset++];
            if (value == 0) {
                dstOffset++;
            } else {
                int alpha = value;
                int invAlpha = 256 - value;
                value = c->flame_gradient[value];
                int background = c->image_title0->pixels[dstOffset];
                c->image_title0->pixels[dstOffset++] = (((value & 0xff00ff) * alpha + (background & 0xff00ff) * invAlpha & 0xff00ff00) + ((value & 0xff00) * alpha + (background & 0xff00) * invAlpha & 0xff0000)) >> 8;
            }
        }
        dstOffset += step;
    }

    pixmap_draw(c->image_title0, 0, 0);

    memcpy(c->image_title1->pixels, c->image_flames_right->pixels, 33920 * sizeof(int));

    srcOffset = 0;
    dstOffset = 1176;
    for (int y = 1; y < height - 1; y++) {
        int offset = c->flame_line_offset[y] * (height - y) / height;
        int step = 103 - offset;
        dstOffset += offset;
        for (int x = 0; x < step; x++) {
            int value = c->flame_buffer3[srcOffset++];
            if (value == 0) {
                dstOffset++;
            } else {
                int alpha = value;
                int invAlpha = 256 - value;
                value = c->flame_gradient[value];
                int background = c->image_title1->pixels[dstOffset];
                c->image_title1->pixels[dstOffset++] = (((value & 0xff00ff) * alpha + (background & 0xff00ff) * invAlpha & 0xff00ff00) + ((value & 0xff00) * alpha + (background & 0xff00) * invAlpha & 0xff0000)) >> 8;
            }
        }
        srcOffset += 128 - step;
        dstOffset += 128 - step - offset;
    }

    pixmap_draw(c->image_title1, 661, 0);
}

void client_run_flames(Client *c) {
    static uint64_t next = 0;
    if (!c->flame_active || next >= rs2_now()) {
        return;
    }
    client_update_flames(c);
    client_update_flames(c);
    client_draw_flames(c);
    next = rs2_now() + 35; // hardcode interval of 35 to avoid inconsistent rate

    /* NOTE: original
    // try {
    uint64_t last = rs2_now();
    int cycle = 0;
    int interval = 20;
    while (c->flame_active) {
        client_update_flames(c);
        client_update_flames(c);
        client_draw_flames(c);

        cycle++;

        if (cycle > 10) {
            uint64_t now = rs2_now();
            int delay = (int)(now - last) / 10 - interval;

            interval = 40 - delay;
            if (interval < 5) {
                interval = 5;
            }

            cycle = 0;
            last = now;
        }

        // try {
        rs2_sleep(interval);
        // } catch (@Pc(52) Exception ignored) {
        // }
    }
    // } catch (@Pc(58) Exception ignored) {
    // }
    */
}

void client_update(Client *c) {
    platform_poll_events(c);
    if (c->error_started || c->error_loading || c->error_host) {
        return;
    }

    _Client.loop_cycle++;
    if (c->ingame) {
        client_update_game(c);
    } else {
        client_update_title(c);
    }
}

void handleChatMouseInput(Client *c, int mouseX, int mouseY) {
    (void)mouseX;
    int line = 0;
    for (int i = 0; i < 100; i++) {
        if (c->message_text[i][0] == '\0') {
            continue;
        }

        int type = c->message_type[i];
        int y = c->chat_scroll_offset + 70 + 4 - line * 14;
        if (y < -20) {
            break;
        }

        if (type == 0) {
            line++;
        }

        if ((type == 1 || type == 2) && (type == 1 || c->public_chat_setting == 0 || (c->public_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
            if (mouseY > y - 14 && mouseY <= y && strcmp(c->message_sender[i], c->local_player->name) != 0) {
                if (c->rights) {
                    sprintf(c->menu_option[c->menu_size], "Report abuse @whi@%s", c->message_sender[i]);
                    c->menu_action[c->menu_size] = 34;
                    c->menu_size++;
                }

                sprintf(c->menu_option[c->menu_size], "Add ignore @whi@%s", c->message_sender[i]);
                c->menu_action[c->menu_size] = 436;
                c->menu_size++;
                sprintf(c->menu_option[c->menu_size], "Add friend @whi@%s", c->message_sender[i]);
                c->menu_action[c->menu_size] = 406;
                c->menu_size++;
            }

            line++;
        }

        if ((type == 3 || type == 7) && c->split_private_chat == 0 && (type == 7 || c->private_chat_setting == 0 || (c->private_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
            if (mouseY > y - 14 && mouseY <= y) {
                if (c->rights) {
                    sprintf(c->menu_option[c->menu_size], "Report abuse @whi@%s", c->message_sender[i]);
                    c->menu_action[c->menu_size] = 34;
                    c->menu_size++;
                }

                sprintf(c->menu_option[c->menu_size], "Add ignore @whi@%s", c->message_sender[i]);
                c->menu_action[c->menu_size] = 436;
                c->menu_size++;
                sprintf(c->menu_option[c->menu_size], "Add friend @whi@%s", c->message_sender[i]);
                c->menu_action[c->menu_size] = 406;
                c->menu_size++;
            }

            line++;
        }

        if (type == 4 && (c->trade_chat_setting == 0 || (c->trade_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
            if (mouseY > y - 14 && mouseY <= y) {
                sprintf(c->menu_option[c->menu_size], "Accept trade @whi@%s", c->message_sender[i]);
                c->menu_action[c->menu_size] = 903;
                c->menu_size++;
            }

            line++;
        }

        if ((type == 5 || type == 6) && c->split_private_chat == 0 && c->private_chat_setting < 2) {
            line++;
        }

        if (type == 8 && (c->trade_chat_setting == 0 || (c->trade_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
            if (mouseY > y - 14 && mouseY <= y) {
                sprintf(c->menu_option[c->menu_size], "Accept duel @whi@%s", c->message_sender[i]);
                c->menu_action[c->menu_size] = 363;
                c->menu_size++;
            }

            line++;
        }
    }
}

void handleInterfaceInput(Client *c, Component *com, int mouseX, int mouseY, int x, int y, int scrollPosition) {
    if (com->type != 0 || !com->childId || com->hide || (mouseX < x || mouseY < y || mouseX > x + com->width || mouseY > y + com->height)) {
        return;
    }

    int children = com->childCount;
    for (int i = 0; i < children; i++) {
        int childX = com->childX[i] + x;
        int childY = com->childY[i] + y - scrollPosition;
        Component *child = _Component.instances[com->childId[i]];

        childX += child->x;
        childY += child->y;

        if ((child->overLayer >= 0 || child->overColour != 0) && mouseX >= childX && mouseY >= childY && mouseX < childX + child->width && mouseY < childY + child->height) {
            if (child->overLayer >= 0) {
                c->lastHoveredInterfaceId = child->overLayer;
            } else {
                c->lastHoveredInterfaceId = child->id;
            }
        }

        if (child->type == 0) {
            handleInterfaceInput(c, child, mouseX, mouseY, childX, childY, child->scrollPosition);

            if (child->scroll > child->height) {
                handleScrollInput(c, mouseX, mouseY, child->scroll, child->height, true, childX + child->width, childY, child);
            }
        } else if (child->type == 2) {
            int slot = 0;

            for (int row = 0; row < child->height; row++) {
                for (int col = 0; col < child->width; col++) {
                    int slotX = childX + col * (child->marginX + 32);
                    int slotY = childY + row * (child->marginY + 32);

                    if (slot < 20) {
                        slotX += child->invSlotOffsetX[slot];
                        slotY += child->invSlotOffsetY[slot];
                    }

                    if (mouseX < slotX || mouseY < slotY || mouseX >= slotX + 32 || mouseY >= slotY + 32) {
                        slot++;
                        continue;
                    }

                    c->hoveredSlot = slot;
                    c->hoveredSlotParentId = child->id;

                    if (child->invSlotObjId[slot] <= 0) {
                        slot++;
                        continue;
                    }

                    ObjType *obj = objtype_get(child->invSlotObjId[slot] - 1);

                    if (c->obj_selected == 1 && child->interactable) {
                        if (child->id != c->objSelectedInterface || slot != c->objSelectedSlot) {
                            sprintf(c->menu_option[c->menu_size], "Use %s with @lre@%s", c->objSelectedName, obj->name);
                            c->menu_action[c->menu_size] = 881;
                            c->menuParamA[c->menu_size] = obj->index;
                            c->menuParamB[c->menu_size] = slot;
                            c->menuParamC[c->menu_size] = child->id;
                            c->menu_size++;
                        }
                    } else if (c->spell_selected == 1 && child->interactable) {
                        if ((c->activeSpellFlags & 0x10) == 16) {
                            sprintf(c->menu_option[c->menu_size], "%s @lre@%s", c->spellCaption, obj->name);
                            c->menu_action[c->menu_size] = 391;
                            c->menuParamA[c->menu_size] = obj->index;
                            c->menuParamB[c->menu_size] = slot;
                            c->menuParamC[c->menu_size] = child->id;
                            c->menu_size++;
                        }
                    } else {
                        if (child->interactable) {
                            for (int op = 4; op >= 3; op--) {
                                if (obj->iop && obj->iop[op]) {
                                    sprintf(c->menu_option[c->menu_size], "%s @lre@%s", obj->iop[op], obj->name);
                                    if (op == 3) {
                                        c->menu_action[c->menu_size] = 478;
                                    } else if (op == 4) {
                                        c->menu_action[c->menu_size] = 347;
                                    }
                                    c->menuParamA[c->menu_size] = obj->index;
                                    c->menuParamB[c->menu_size] = slot;
                                    c->menuParamC[c->menu_size] = child->id;
                                    c->menu_size++;
                                } else if (op == 4) {
                                    sprintf(c->menu_option[c->menu_size], "Drop @lre@%s", obj->name);
                                    c->menu_action[c->menu_size] = 347;
                                    c->menuParamA[c->menu_size] = obj->index;
                                    c->menuParamB[c->menu_size] = slot;
                                    c->menuParamC[c->menu_size] = child->id;
                                    c->menu_size++;
                                }
                            }
                        }

                        if (child->usable) {
                            sprintf(c->menu_option[c->menu_size], "Use @lre@%s", obj->name);
                            c->menu_action[c->menu_size] = 188;
                            c->menuParamA[c->menu_size] = obj->index;
                            c->menuParamB[c->menu_size] = slot;
                            c->menuParamC[c->menu_size] = child->id;
                            c->menu_size++;
                        }

                        if (child->interactable && obj->iop) {
                            for (int op = 2; op >= 0; op--) {
                                if (obj->iop[op]) {
                                    sprintf(c->menu_option[c->menu_size], "%s @lre@%s", obj->iop[op], obj->name);
                                    if (op == 0) {
                                        c->menu_action[c->menu_size] = 405;
                                    } else if (op == 1) {
                                        c->menu_action[c->menu_size] = 38;
                                    } else if (op == 2) {
                                        c->menu_action[c->menu_size] = 422;
                                    }
                                    c->menuParamA[c->menu_size] = obj->index;
                                    c->menuParamB[c->menu_size] = slot;
                                    c->menuParamC[c->menu_size] = child->id;
                                    c->menu_size++;
                                }
                            }
                        }

                        if (child->iops) {
                            for (int op = 4; op >= 0; op--) {
                                if (child->iops[op]) {
                                    sprintf(c->menu_option[c->menu_size], "%s @lre@%s", child->iops[op], obj->name);
                                    if (op == 0) {
                                        c->menu_action[c->menu_size] = 602;
                                    } else if (op == 1) {
                                        c->menu_action[c->menu_size] = 596;
                                    } else if (op == 2) {
                                        c->menu_action[c->menu_size] = 22;
                                    } else if (op == 3) {
                                        c->menu_action[c->menu_size] = 892;
                                    } else if (op == 4) {
                                        c->menu_action[c->menu_size] = 415;
                                    }
                                    c->menuParamA[c->menu_size] = obj->index;
                                    c->menuParamB[c->menu_size] = slot;
                                    c->menuParamC[c->menu_size] = child->id;
                                    c->menu_size++;
                                }
                            }
                        }

                        sprintf(c->menu_option[c->menu_size], "Examine @lre@%s", obj->name);
                        // TODO
                        // if (c->showDebug) {
                        // 	c->menu_option[c->menu_size] += "@whi@ (" + (obj.index) + ")";
                        // }
                        c->menu_action[c->menu_size] = 1773;
                        c->menuParamA[c->menu_size] = obj->index;
                        c->menuParamC[c->menu_size] = child->invSlotObjCount[slot];
                        c->menu_size++;
                    }

                    slot++;
                }
            }
        } else if (mouseX >= childX && mouseY >= childY && mouseX < childX + child->width && mouseY < childY + child->height) {
            if (child->buttonType == BUTTON_OK) {
                bool override = false;
                if (child->clientCode != 0) {
                    override = handleSocialMenuOption(c, child);
                }

                if (!override) {
                    strcpy(c->menu_option[c->menu_size], child->option);
                    c->menu_action[c->menu_size] = 951;
                    c->menuParamC[c->menu_size] = child->id;
                    c->menu_size++;
                }
            } else if (child->buttonType == BUTTON_TARGET && c->spell_selected == 0) {
                char *prefix = child->actionVerb;
                bool _free = false;
                if (indexof(prefix, " ") != -1) {
                    prefix = substring(prefix, 0, indexof(prefix, " "));
                    _free = true;
                }

                sprintf(c->menu_option[c->menu_size], "%s @gre@%s", prefix, child->action);
                if (_free) {
                    free(prefix);
                }
                c->menu_action[c->menu_size] = 930;
                c->menuParamC[c->menu_size] = child->id;
                c->menu_size++;
            } else if (child->buttonType == BUTTON_CLOSE) {
                strcpy(c->menu_option[c->menu_size], "Close");
                c->menu_action[c->menu_size] = 947;
                c->menuParamC[c->menu_size] = child->id;
                c->menu_size++;
            } else if (child->buttonType == BUTTON_TOGGLE) {
                strcpy(c->menu_option[c->menu_size], child->option);
                c->menu_action[c->menu_size] = 465;
                c->menuParamC[c->menu_size] = child->id;
                c->menu_size++;
            } else if (child->buttonType == BUTTON_SELECT) {
                strcpy(c->menu_option[c->menu_size], child->option);
                c->menu_action[c->menu_size] = 960;
                c->menuParamC[c->menu_size] = child->id;
                c->menu_size++;
            } else if (child->buttonType == BUTTON_CONTINUE && !c->pressed_continue_option) {
                strcpy(c->menu_option[c->menu_size], child->option);
                c->menu_action[c->menu_size] = 44;
                c->menuParamC[c->menu_size] = child->id;
                c->menu_size++;
            }
        }
    }
}

bool handleSocialMenuOption(Client *c, Component *component) {
    int type = component->clientCode;
    if (type >= 1 && type <= 200) {
        if (type >= 101) {
            type -= 101;
        } else {
            type--;
        }

        sprintf(c->menu_option[c->menu_size], "Remove @whi@%s", c->friendName[type]);
        c->menu_action[c->menu_size] = 557;
        c->menu_size++;

        sprintf(c->menu_option[c->menu_size], "Message @whi@%s", c->friendName[type]);
        c->menu_action[c->menu_size] = 679;
        c->menu_size++;
        return true;
    } else if (type >= 401 && type <= 500) {
        sprintf(c->menu_option[c->menu_size], "Remove @whi@%s", component->text);
        c->menu_action[c->menu_size] = 556;
        c->menu_size++;
        return true;
    } else {
        return false;
    }
}

void handlePrivateChatInput(Client *c, int mouse_x, int mouse_y) {
    if (c->split_private_chat == 0) {
        return;
    }

    int lineOffset = 0;
    if (c->system_update_timer != 0) {
        lineOffset = 1;
    }

    for (int i = 0; i < 100; i++) {
        if (c->message_text[i][0]) {
            int type = c->message_type[i];
            if ((type == 3 || type == 7) && (type == 7 || c->private_chat_setting == 0 || (c->private_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
                int y = 329 - lineOffset * 13;
                // super.mouseX was used here for no reason when they are both passed to func
                if (mouse_x > 8 && mouse_x < 520 && mouse_y - 11 > y - 10 && mouse_y - 11 <= y + 3) {
                    if (c->rights) {
                        sprintf(c->menu_option[c->menu_size], "Report abuse @whi@%s", c->message_sender[i]);
                        c->menu_action[c->menu_size] = 2034;
                        c->menu_size++;
                    }
                    sprintf(c->menu_option[c->menu_size], "Add ignore @whi@%s", c->message_sender[i]);
                    c->menu_action[c->menu_size] = 2436;
                    c->menu_size++;
                    sprintf(c->menu_option[c->menu_size], "Add friend @whi@%s", c->message_sender[i]);
                    c->menu_action[c->menu_size] = 2406;
                    c->menu_size++;
                }

                lineOffset++;
                if (lineOffset >= 5) {
                    return;
                }
            }

            if ((type == 5 || type == 6) && c->private_chat_setting < 2) {
                lineOffset++;
                if (lineOffset >= 5) {
                    return;
                }
            }
        }
    }
}

static const char *getCombatLevelColorTag(int viewerLevel, int otherLevel) {
    int diff = viewerLevel - otherLevel;
    if (diff < -9) {
        return "@red@";
    } else if (diff < -6) {
        return "@or3@";
    } else if (diff < -3) {
        return "@or2@";
    } else if (diff < 0) {
        return "@or1@";
    } else if (diff > 9) {
        return "@gre@";
    } else if (diff > 6) {
        return "@gr3@";
    } else if (diff > 3) {
        return "@gr2@";
    } else if (diff > 0) {
        return "@gr1@";
    } else {
        return "@yel@";
    }
}

void addNpcOptions(Client *cl, NpcType *npc, int a, int b, int c) {
    if (cl->menu_size >= 400) {
        return;
    }

    char tooltip[MAX_STR];
    if (npc->vislevel != 0) {
        char tmp[HALF_STR];
        strcpy(tmp, npc->name);
        sprintf(tooltip, "%s%s (level-%d)", tmp, getCombatLevelColorTag(cl->local_player->combatLevel, npc->vislevel), npc->vislevel);
    } else {
        strcpy(tooltip, npc->name);
    }

    if (cl->obj_selected == 1) {
        sprintf(cl->menu_option[cl->menu_size], "Use %s with @yel@%s", cl->objSelectedName, tooltip);
        cl->menu_action[cl->menu_size] = 900;
        cl->menuParamA[cl->menu_size] = a;
        cl->menuParamB[cl->menu_size] = b;
        cl->menuParamC[cl->menu_size] = c;
        cl->menu_size++;
    } else if (cl->spell_selected != 1) {
        int type;
        if (npc->op) {
            for (type = 4; type >= 0; type--) {
                if (npc->op[type] && platform_strcasecmp(npc->op[type], "attack") != 0) {
                    sprintf(cl->menu_option[cl->menu_size], "%s @yel@%s", npc->op[type], tooltip);

                    if (type == 0) {
                        cl->menu_action[cl->menu_size] = 728;
                    } else if (type == 1) {
                        cl->menu_action[cl->menu_size] = 542;
                    } else if (type == 2) {
                        cl->menu_action[cl->menu_size] = 6;
                    } else if (type == 3) {
                        cl->menu_action[cl->menu_size] = 963;
                    } else if (type == 4) {
                        cl->menu_action[cl->menu_size] = 245;
                    }

                    cl->menuParamA[cl->menu_size] = a;
                    cl->menuParamB[cl->menu_size] = b;
                    cl->menuParamC[cl->menu_size] = c;
                    cl->menu_size++;
                }
            }
        }

        if (npc->op) {
            for (type = 4; type >= 0; type--) {
                if (npc->op[type] && platform_strcasecmp(npc->op[type], "attack") == 0) {
                    int action = 0;
                    if (npc->vislevel > cl->local_player->combatLevel) {
                        action = 2000;
                    }

                    sprintf(cl->menu_option[cl->menu_size], "%s @yel@%s", npc->op[type], tooltip);

                    if (type == 0) {
                        cl->menu_action[cl->menu_size] = action + 728;
                    } else if (type == 1) {
                        cl->menu_action[cl->menu_size] = action + 542;
                    } else if (type == 2) {
                        cl->menu_action[cl->menu_size] = action + 6;
                    } else if (type == 3) {
                        cl->menu_action[cl->menu_size] = action + 963;
                    } else if (type == 4) {
                        cl->menu_action[cl->menu_size] = action + 245;
                    }

                    cl->menuParamA[cl->menu_size] = a;
                    cl->menuParamB[cl->menu_size] = b;
                    cl->menuParamC[cl->menu_size] = c;
                    cl->menu_size++;
                }
            }
        }

        sprintf(cl->menu_option[cl->menu_size], "Examine @yel@%s", tooltip);
        // TODO:
        // if (cl->showDebug) {
        // 	cl->menu_option[cl->menu_size] += "@whi@ (" + (npc.index) + ")";
        // }
        cl->menu_action[cl->menu_size] = 1607;
        cl->menuParamA[cl->menu_size] = a;
        cl->menuParamB[cl->menu_size] = b;
        cl->menuParamC[cl->menu_size] = c;
        cl->menu_size++;
    } else if ((cl->activeSpellFlags & 0x2) == 2) {
        sprintf(cl->menu_option[cl->menu_size], "%s @yel@%s", cl->spellCaption, tooltip);
        cl->menu_action[cl->menu_size] = 265;
        cl->menuParamA[cl->menu_size] = a;
        cl->menuParamB[cl->menu_size] = b;
        cl->menuParamC[cl->menu_size] = c;
        cl->menu_size++;
    }
}

void addPlayerOptions(Client *cl, PlayerEntity *player, int a, int b, int c) {
    if (player == cl->local_player || cl->menu_size >= 400) {
        return;
    }

    char tooltip[MAX_STR];
    sprintf(tooltip, "%s%s (level-%d)", player->name, getCombatLevelColorTag(cl->local_player->combatLevel, player->combatLevel), player->combatLevel);
    if (cl->obj_selected == 1) {
        sprintf(cl->menu_option[cl->menu_size], "Use %s with @whi@%s", cl->objSelectedName, tooltip);
        cl->menu_action[cl->menu_size] = 367;
        cl->menuParamA[cl->menu_size] = a;
        cl->menuParamB[cl->menu_size] = b;
        cl->menuParamC[cl->menu_size] = c;
        cl->menu_size++;
    } else if (cl->spell_selected != 1) {
        sprintf(cl->menu_option[cl->menu_size], "Follow @whi@%s", tooltip);
        cl->menu_action[cl->menu_size] = 1544;
        cl->menuParamA[cl->menu_size] = a;
        cl->menuParamB[cl->menu_size] = b;
        cl->menuParamC[cl->menu_size] = c;
        cl->menu_size++;

        if (cl->overrideChat == 0) {
            sprintf(cl->menu_option[cl->menu_size], "Trade with @whi@%s", tooltip);
            cl->menu_action[cl->menu_size] = 1373;
            cl->menuParamA[cl->menu_size] = a;
            cl->menuParamB[cl->menu_size] = b;
            cl->menuParamC[cl->menu_size] = c;
            cl->menu_size++;
        }

        if (cl->wildernessLevel > 0) {
            sprintf(cl->menu_option[cl->menu_size], "Attack @whi@%s", tooltip);
            if (cl->local_player->combatLevel >= player->combatLevel) {
                cl->menu_action[cl->menu_size] = 151;
            } else {
                cl->menu_action[cl->menu_size] = 2151;
            }
            cl->menuParamA[cl->menu_size] = a;
            cl->menuParamB[cl->menu_size] = b;
            cl->menuParamC[cl->menu_size] = c;
            cl->menu_size++;
        }

        if (cl->worldLocationState == 1) {
            sprintf(cl->menu_option[cl->menu_size], "Fight @whi@%s", tooltip);
            cl->menu_action[cl->menu_size] = 151;
            cl->menuParamA[cl->menu_size] = a;
            cl->menuParamB[cl->menu_size] = b;
            cl->menuParamC[cl->menu_size] = c;
            cl->menu_size++;
        }

        if (cl->worldLocationState == 2) {
            sprintf(cl->menu_option[cl->menu_size], "Duel-with @whi@%s", tooltip);
            cl->menu_action[cl->menu_size] = 1101;
            cl->menuParamA[cl->menu_size] = a;
            cl->menuParamB[cl->menu_size] = b;
            cl->menuParamC[cl->menu_size] = c;
            cl->menu_size++;
        }
    } else if ((cl->activeSpellFlags & 0x8) == 8) {
        sprintf(cl->menu_option[cl->menu_size], "%s @whi@%s", cl->spellCaption, tooltip);
        cl->menu_action[cl->menu_size] = 651;
        cl->menuParamA[cl->menu_size] = a;
        cl->menuParamB[cl->menu_size] = b;
        cl->menuParamC[cl->menu_size] = c;
        cl->menu_size++;
    }

    for (int i = 0; i < cl->menu_size; i++) {
        if (cl->menu_action[i] == 660) {
            sprintf(cl->menu_option[i], "Walk here @whi@%s", tooltip);
            return;
        }
    }
}

void handleViewportOptions(Client *c) {
    if (c->obj_selected == 0 && c->spell_selected == 0) {
        strcpy(c->menu_option[c->menu_size], "Walk here");
        c->menu_action[c->menu_size] = 660;
        c->menuParamB[c->menu_size] = c->shell->mouse_x;
        c->menuParamC[c->menu_size] = c->shell->mouse_y;
        c->menu_size++;
    }

    int lastBitset = -1;
    for (int picked = 0; picked < _Model.picked_count; picked++) {
        int bitset = _Model.picked_bitsets[picked];
        int x = bitset & 0x7f;
        int z = bitset >> 7 & 0x7f;
        int entityType = bitset >> 29 & 0x3;
        int typeId = bitset >> 14 & 0x7fff;

        if (bitset == lastBitset) {
            continue;
        }

        lastBitset = bitset;

        if (entityType == 2 && world3d_get_info(c->scene, c->currentLevel, x, z, bitset) >= 0) {
            LocType *loc = loctype_get(typeId);
            if (c->obj_selected == 1) {
                sprintf(c->menu_option[c->menu_size], "Use %s with @cya@%s", c->objSelectedName, loc->name);
                c->menu_action[c->menu_size] = 450;
                c->menuParamA[c->menu_size] = bitset;
                c->menuParamB[c->menu_size] = x;
                c->menuParamC[c->menu_size] = z;
                c->menu_size++;
            } else if (c->spell_selected != 1) {
                if (loc->op) {
                    for (int op = 4; op >= 0; op--) {
                        if (loc->op[op]) {
                            sprintf(c->menu_option[c->menu_size], "%s @cya@%s", loc->op[op], loc->name);
                            if (op == 0) {
                                c->menu_action[c->menu_size] = 285;
                            }

                            if (op == 1) {
                                c->menu_action[c->menu_size] = 504;
                            }

                            if (op == 2) {
                                c->menu_action[c->menu_size] = 364;
                            }

                            if (op == 3) {
                                c->menu_action[c->menu_size] = 581;
                            }

                            if (op == 4) {
                                c->menu_action[c->menu_size] = 1501;
                            }

                            c->menuParamA[c->menu_size] = bitset;
                            c->menuParamB[c->menu_size] = x;
                            c->menuParamC[c->menu_size] = z;
                            c->menu_size++;
                        }
                    }
                }

                sprintf(c->menu_option[c->menu_size], "Examine @cya@%s", loc->name);
                // TODO
                // if (c->showDebug) {
                // 	c->menu_option[c->menu_size] += "@whi@ (" + (loc.index) + ")";
                // }
                c->menu_action[c->menu_size] = 1175;
                c->menuParamA[c->menu_size] = bitset;
                c->menuParamB[c->menu_size] = x;
                c->menuParamC[c->menu_size] = z;
                c->menu_size++;
            } else if ((c->activeSpellFlags & 0x4) == 4) {
                sprintf(c->menu_option[c->menu_size], "%s @cya@%s", c->spellCaption, loc->name);
                c->menu_action[c->menu_size] = 55;
                c->menuParamA[c->menu_size] = bitset;
                c->menuParamB[c->menu_size] = x;
                c->menuParamC[c->menu_size] = z;
                c->menu_size++;
            }
        }

        if (entityType == 1) {
            NpcEntity *npc = c->npcs[typeId];
            if (npc->type->size == 1 && (npc->pathing_entity.x & 0x7f) == 64 && (npc->pathing_entity.z & 0x7f) == 64) {
                for (int i = 0; i < c->npc_count; i++) {
                    NpcEntity *other = c->npcs[c->npc_ids[i]];

                    if (other && other != npc && other->type->size == 1 && other->pathing_entity.x == npc->pathing_entity.x && other->pathing_entity.z == npc->pathing_entity.z) {
                        addNpcOptions(c, other->type, c->npc_ids[i], x, z);
                    }
                }
            }

            addNpcOptions(c, npc->type, typeId, x, z);
        }

        if (entityType == 0) {
            PlayerEntity *player = c->players[typeId];
            if ((player->pathing_entity.x & 0x7f) == 64 && (player->pathing_entity.z & 0x7f) == 64) {
                for (int i = 0; i < c->npc_count; i++) {
                    NpcEntity *other = c->npcs[c->npc_ids[i]];

                    if (other && other->type->size == 1 && other->pathing_entity.x == player->pathing_entity.x && other->pathing_entity.z == player->pathing_entity.z) {
                        addNpcOptions(c, other->type, c->npc_ids[i], x, z);
                    }
                }

                for (int i = 0; i < c->player_count; i++) {
                    PlayerEntity *other = c->players[c->player_ids[i]];

                    if (other && other != player && other->pathing_entity.x == player->pathing_entity.x && other->pathing_entity.z == player->pathing_entity.z) {
                        addPlayerOptions(c, other, c->player_ids[i], x, z);
                    }
                }
            }

            addPlayerOptions(c, player, typeId, x, z);
        }

        if (entityType == 3) {
            LinkList *objs = c->level_obj_stacks[c->currentLevel][x][z];
            if (!objs) {
                continue;
            }

            for (ObjStackEntity *obj = (ObjStackEntity *)linklist_tail(objs); obj; obj = (ObjStackEntity *)linklist_prev(objs)) {
                ObjType *type = objtype_get(obj->index);
                if (c->obj_selected == 1) {
                    sprintf(c->menu_option[c->menu_size], "Use %s with @lre@%s", c->objSelectedName, type->name);
                    c->menu_action[c->menu_size] = 217;
                    c->menuParamA[c->menu_size] = obj->index;
                    c->menuParamB[c->menu_size] = x;
                    c->menuParamC[c->menu_size] = z;
                    c->menu_size++;
                } else if (c->spell_selected != 1) {
                    for (int op = 4; op >= 0; op--) {
                        if (type->op && type->op[op]) {
                            sprintf(c->menu_option[c->menu_size], "%s @lre@%s", type->op[op], type->name);
                            if (op == 0) {
                                c->menu_action[c->menu_size] = 224;
                            }

                            if (op == 1) {
                                c->menu_action[c->menu_size] = 993;
                            }

                            if (op == 2) {
                                c->menu_action[c->menu_size] = 99;
                            }

                            if (op == 3) {
                                c->menu_action[c->menu_size] = 746;
                            }

                            if (op == 4) {
                                c->menu_action[c->menu_size] = 877;
                            }

                            c->menuParamA[c->menu_size] = obj->index;
                            c->menuParamB[c->menu_size] = x;
                            c->menuParamC[c->menu_size] = z;
                            c->menu_size++;
                        } else if (op == 2) {
                            sprintf(c->menu_option[c->menu_size], "Take @lre@%s", type->name);
                            c->menu_action[c->menu_size] = 99;
                            c->menuParamA[c->menu_size] = obj->index;
                            c->menuParamB[c->menu_size] = x;
                            c->menuParamC[c->menu_size] = z;
                            c->menu_size++;
                        }
                    }

                    sprintf(c->menu_option[c->menu_size], "Examine @lre@%s", type->name);
                    // TODO
                    // if (c->showDebug) {
                    // 	c->menu_option[c->menu_size] += "@whi@ (" + (obj.index) + ")";
                    // }
                    c->menu_action[c->menu_size] = 1102;
                    c->menuParamA[c->menu_size] = obj->index;
                    c->menuParamB[c->menu_size] = x;
                    c->menuParamC[c->menu_size] = z;
                    c->menu_size++;
                } else if ((c->activeSpellFlags & 0x1) == 1) {
                    sprintf(c->menu_option[c->menu_size], "%s @lre@%s", c->spellCaption, type->name);
                    c->menu_action[c->menu_size] = 965;
                    c->menuParamA[c->menu_size] = obj->index;
                    c->menuParamB[c->menu_size] = x;
                    c->menuParamC[c->menu_size] = z;
                    c->menu_size++;
                }
            }
        }
    }
}

void client_handle_input(Client *c) {
    if (c->obj_drag_area != 0) {
        return;
    }

    strcpy(c->menu_option[0], "Cancel");
    c->menu_action[0] = 1252;
    c->menu_size = 1;
    handlePrivateChatInput(c, c->shell->mouse_x, c->shell->mouse_y);
    c->lastHoveredInterfaceId = 0;

    if (c->shell->mouse_x > 8 && c->shell->mouse_y > 11 && c->shell->mouse_x < 520 && c->shell->mouse_y < 345) {
        if (c->viewport_interface_id == -1) {
            handleViewportOptions(c);
        } else {
            handleInterfaceInput(c, _Component.instances[c->viewport_interface_id], c->shell->mouse_x, c->shell->mouse_y, 8, 11, 0);
        }
    }

    if (c->lastHoveredInterfaceId != c->viewportHoveredInterfaceIndex) {
        c->viewportHoveredInterfaceIndex = c->lastHoveredInterfaceId;
    }

    c->lastHoveredInterfaceId = 0;

    if (c->shell->mouse_x > 562 && c->shell->mouse_y > 231 && c->shell->mouse_x < 752 && c->shell->mouse_y < 492) {
        if (c->sidebar_interface_id != -1) {
            handleInterfaceInput(c, _Component.instances[c->sidebar_interface_id], c->shell->mouse_x, c->shell->mouse_y, 562, 231, 0);
        } else if (c->tab_interface_id[c->selected_tab] != -1) {
            handleInterfaceInput(c, _Component.instances[c->tab_interface_id[c->selected_tab]], c->shell->mouse_x, c->shell->mouse_y, 562, 231, 0);
        }
    }

    if (c->lastHoveredInterfaceId != c->sidebarHoveredInterfaceIndex) {
        c->redraw_sidebar = true;
        c->sidebarHoveredInterfaceIndex = c->lastHoveredInterfaceId;
    }

    c->lastHoveredInterfaceId = 0;

    if (c->shell->mouse_x > 22 && c->shell->mouse_y > 375 && c->shell->mouse_x < 431 && c->shell->mouse_y < 471) {
        if (c->chat_interface_id == -1) {
            handleChatMouseInput(c, c->shell->mouse_x - 22, c->shell->mouse_y - 375);
        } else {
            handleInterfaceInput(c, _Component.instances[c->chat_interface_id], c->shell->mouse_x, c->shell->mouse_y, 22, 375, 0);
        }
    }

    if (c->chat_interface_id != -1 && c->lastHoveredInterfaceId != c->chatHoveredInterfaceIndex) {
        c->redraw_chatback = true;
        c->chatHoveredInterfaceIndex = c->lastHoveredInterfaceId;
    }

    bool done = false;
    while (!done) {
        done = true;

        for (int i = 0; i < c->menu_size - 1; i++) {
            if (c->menu_action[i] < 1000 && c->menu_action[i + 1] > 1000) {
                char tmp0[MAX_STR];
                strcpy(tmp0, c->menu_option[i]);
                strcpy(c->menu_option[i], c->menu_option[i + 1]);
                strcpy(c->menu_option[i + 1], tmp0);

                int tmp1 = c->menu_action[i];
                c->menu_action[i] = c->menu_action[i + 1];
                c->menu_action[i + 1] = tmp1;

                int tmp2 = c->menuParamB[i];
                c->menuParamB[i] = c->menuParamB[i + 1];
                c->menuParamB[i + 1] = tmp2;

                int tmp3 = c->menuParamC[i];
                c->menuParamC[i] = c->menuParamC[i + 1];
                c->menuParamC[i + 1] = tmp3;

                int tmp4 = c->menuParamA[i];
                c->menuParamA[i] = c->menuParamA[i + 1];
                c->menuParamA[i + 1] = tmp4;

                done = false;
            }
        }
    }
}

bool handleInterfaceAction(Client *c, Component *com) {
    int clientCode = com->clientCode;
    if (clientCode == 201) {
        c->redraw_chatback = true;
        c->chatback_input_open = false;
        c->show_social_input = true;
        c->social_input[0] = '\0';
        c->social_action = 1;
        strcpy(c->social_message, "Enter name of friend to add to list");
    }

    if (clientCode == 202) {
        c->redraw_chatback = true;
        c->chatback_input_open = false;
        c->show_social_input = true;
        c->social_input[0] = '\0';
        c->social_action = 2;
        strcpy(c->social_message, "Enter name of friend to delete from list");
    }

    if (clientCode == 205) {
        c->idle_timeout = 250;
        return true;
    }

    if (clientCode == 501) {
        c->redraw_chatback = true;
        c->chatback_input_open = false;
        c->show_social_input = true;
        c->social_input[0] = '\0';
        c->social_action = 4;
        strcpy(c->social_message, "Enter name of player to add to list");
    }

    if (clientCode == 502) {
        c->redraw_chatback = true;
        c->chatback_input_open = false;
        c->show_social_input = true;
        c->social_input[0] = '\0';
        c->social_action = 5;
        strcpy(c->social_message, "Enter name of player to delete from list");
    }

    if (clientCode >= 300 && clientCode <= 313) {
        int part = (clientCode - 300) / 2;
        int direction = clientCode & 0x1;
        int kit = c->designIdentikits[part];

        if (kit != -1) {
            while (true) {
                if (direction == 0) {
                    kit--;
                    if (kit < 0) {
                        kit = _IdkType.count - 1;
                    }
                }

                if (direction == 1) {
                    kit++;
                    if (kit >= _IdkType.count) {
                        kit = 0;
                    }
                }

                if (!_IdkType.instances[kit]->disable && _IdkType.instances[kit]->type == part + (c->design_gender_male ? 0 : 7)) {
                    c->designIdentikits[part] = kit;
                    c->update_design_model = true;
                    break;
                }
            }
        }
    }

    if (clientCode >= 314 && clientCode <= 323) {
        int part = (clientCode - 314) / 2;
        int direction = clientCode & 0x1;
        int color = c->design_colors[part];

        if (direction == 0) {
            color--;
            if (color < 0) {
                color = DESIGN_BODY_COLOR_LENGTH[part] - 1;
            }
        }

        if (direction == 1) {
            color++;
            if (color >= DESIGN_BODY_COLOR_LENGTH[part]) {
                color = 0;
            }
        }

        c->design_colors[part] = color;
        c->update_design_model = true;
    }

    if (clientCode == 324 && !c->design_gender_male) {
        c->design_gender_male = true;
        client_validate_character_design(c);
    }

    if (clientCode == 325 && c->design_gender_male) {
        c->design_gender_male = false;
        client_validate_character_design(c);
    }

    if (clientCode == 326) {
        // IF_PLAYERDESIGN
        p1isaac(c->out, 52);
        p1(c->out, c->design_gender_male ? 0 : 1);
        for (int i = 0; i < 7; i++) {
            p1(c->out, c->designIdentikits[i]);
        }
        for (int i = 0; i < 5; i++) {
            p1(c->out, c->design_colors[i]);
        }
        return true;
    }

    if (clientCode == 613) {
        c->reportAbuseMuteOption = !c->reportAbuseMuteOption;
    }

    if (clientCode >= 601 && clientCode <= 612) {
        closeInterfaces(c);

        if (strlen(c->reportAbuseInput) > 0) {
            // BUG_REPORT
            p1isaac(c->out, 190);
            p8(c->out, jstring_to_base37(c->reportAbuseInput));
            p1(c->out, clientCode - 601);
            p1(c->out, c->reportAbuseMuteOption ? 1 : 0);
        }
    }

    return false;
}

void handleScrollInput(Client *c, int mouseX, int mouseY, int scrollableHeight, int height, bool redraw, int left, int top, Component *component) {
    if (c->scrollGrabbed) {
        c->scrollInputPadding = 32;
    } else {
        c->scrollInputPadding = 0;
    }

    c->scrollGrabbed = false;

    if (mouseX >= left && mouseX < left + 16 && mouseY >= top && mouseY < top + 16) {
        component->scrollPosition -= c->drag_cycles * 4;
        if (redraw) {
            c->redraw_sidebar = true;
        }
    } else if (mouseX >= left && mouseX < left + 16 && mouseY >= top + height - 16 && mouseY < top + height) {
        component->scrollPosition += c->drag_cycles * 4;
        if (redraw) {
            c->redraw_sidebar = true;
        }
    } else if (mouseX >= left - c->scrollInputPadding && mouseX < left + c->scrollInputPadding + 16 && mouseY >= top + 16 && mouseY < top + height - 16 && c->drag_cycles > 0) {
        int gripSize = (height - 32) * height / scrollableHeight;
        if (gripSize < 8) {
            gripSize = 8;
        }
        int gripY = mouseY - top - gripSize / 2 - 16;
        int maxY = height - gripSize - 32;
        component->scrollPosition = (scrollableHeight - height) * gripY / maxY;
        if (redraw) {
            c->redraw_sidebar = true;
        }
        c->scrollGrabbed = true;
    }
}

void showContextMenu(Client *c) {
    int width = stringWidth(c->font_bold12, "Choose Option");
    int maxWidth;
    for (int i = 0; i < c->menu_size; i++) {
        maxWidth = stringWidth(c->font_bold12, c->menu_option[i]);
        if (maxWidth > width) {
            width = maxWidth;
        }
    }
    width += 8;

    int height = c->menu_size * 15 + 21;

    int x;
    int y;
    if (c->shell->mouse_click_x > 8 && c->shell->mouse_click_y > 11 && c->shell->mouse_click_x < 520 && c->shell->mouse_click_y < 345) {
        x = c->shell->mouse_click_x - width / 2 - 8;
        if (x + width > 512) {
            x = 512 - width;
        } else if (x < 0) {
            x = 0;
        }

        y = c->shell->mouse_click_y - 11;
        if (y + height > 334) {
            y = 334 - height;
        } else if (y < 0) {
            y = 0;
        }

        c->menu_visible = true;
        c->menu_area = 0;
        c->menu_x = x;
        c->menu_y = y;
        c->menu_width = width;
        c->menu_height = c->menu_size * 15 + 22;
    }
    if (c->shell->mouse_click_x > 562 && c->shell->mouse_click_y > 231 && c->shell->mouse_click_x < 752 && c->shell->mouse_click_y < 492) {
        x = c->shell->mouse_click_x - width / 2 - 562;
        if (x < 0) {
            x = 0;
        } else if (x + width > 190) {
            x = 190 - width;
        }

        y = c->shell->mouse_click_y - 231;
        if (y < 0) {
            y = 0;
        } else if (y + height > 261) {
            y = 261 - height;
        }

        c->menu_visible = true;
        c->menu_area = 1;
        c->menu_x = x;
        c->menu_y = y;
        c->menu_width = width;
        c->menu_height = c->menu_size * 15 + 22;
    }
    if (c->shell->mouse_click_x > 22 && c->shell->mouse_click_y > 375 && c->shell->mouse_click_x < 501 && c->shell->mouse_click_y < 471) {
        x = c->shell->mouse_click_x - width / 2 - 22;
        if (x < 0) {
            x = 0;
        } else if (x + width > 479) {
            x = 479 - width;
        }

        y = c->shell->mouse_click_y - 375;
        if (y < 0) {
            y = 0;
        } else if (y + height > 96) {
            y = 96 - height;
        }

        c->menu_visible = true;
        c->menu_area = 2;
        c->menu_x = x;
        c->menu_y = y;
        c->menu_width = width;
        c->menu_height = c->menu_size * 15 + 22;
    }
}

bool isAddFriendOption(Client *c, int option) {
    if (option < 0) {
        return false;
    }

    int action = c->menu_action[option];
    if (action >= 2000) {
        action -= 2000;
    }
    return action == 406;
}

void updateMergeLocs(Client *c) {
    if (c->scene_state == 2) {
        for (LocMergeEntity *loc = (LocMergeEntity *)linklist_head(c->merged_locations); loc; loc = (LocMergeEntity *)linklist_next(c->merged_locations)) {
            if (_Client.loop_cycle >= loc->lastCycle) {
                addLoc(c, loc->plane, loc->x, loc->z, loc->locIndex, loc->angle, loc->shape, loc->layer);
                linkable_unlink(&loc->link);
                free(loc);
            }
        }

        _Client.cyclelogic5++;
        if (_Client.cyclelogic5 > 85) {
            _Client.cyclelogic5 = 0;
            // ANTICHEAT_CYCLELOGIC5
            p1isaac(c->out, 85);
        }
    }
}

void updateEntityChats(Client *c) {
    for (int i = -1; i < c->player_count; i++) {
        int index;
        if (i == -1) {
            index = LOCAL_PLAYER_INDEX;
        } else {
            index = c->player_ids[i];
        }

        PlayerEntity *player = c->players[index];
        if (player && player->pathing_entity.chatTimer > 0) {
            player->pathing_entity.chatTimer--;

            if (player->pathing_entity.chatTimer == 0) {
                player->pathing_entity.chat[0] = '\0';
            }
        }
    }

    for (int i = 0; i < c->npc_count; i++) {
        int index = c->npc_ids[i];
        NpcEntity *npc = c->npcs[index];

        if (npc && npc->pathing_entity.chatTimer > 0) {
            npc->pathing_entity.chatTimer--;

            if (npc->pathing_entity.chatTimer == 0) {
                npc->pathing_entity.chat[0] = '\0';
            }
        }
    }
}

static void updateForceMovement(PathingEntity *entity) {
    int delta = entity->forceMoveEndCycle - _Client.loop_cycle;
    int dstX = entity->forceMoveStartSceneTileX * 128 + entity->size * 64;
    int dstZ = entity->forceMoveStartSceneTileZ * 128 + entity->size * 64;

    entity->x += (dstX - entity->x) / delta;
    entity->z += (dstZ - entity->z) / delta;

    entity->seqTrigger = 0;

    if (entity->forceMoveFaceDirection == 0) {
        entity->dstYaw = 1024;
    }

    if (entity->forceMoveFaceDirection == 1) {
        entity->dstYaw = 1536;
    }

    if (entity->forceMoveFaceDirection == 2) {
        entity->dstYaw = 0;
    }

    if (entity->forceMoveFaceDirection == 3) {
        entity->dstYaw = 512;
    }
}

static void startForceMovement(PathingEntity *entity) {
    if (entity->forceMoveStartCycle == _Client.loop_cycle || entity->primarySeqId == -1 || entity->primarySeqDelay != 0 || entity->primarySeqCycle + 1 > _SeqType.instances[entity->primarySeqId]->delay[entity->primarySeqFrame]) {
        int duration = entity->forceMoveStartCycle - entity->forceMoveEndCycle;
        int delta = _Client.loop_cycle - entity->forceMoveEndCycle;
        int dx0 = entity->forceMoveStartSceneTileX * 128 + entity->size * 64;
        int dz0 = entity->forceMoveStartSceneTileZ * 128 + entity->size * 64;
        int dx1 = entity->forceMoveEndSceneTileX * 128 + entity->size * 64;
        int dz1 = entity->forceMoveEndSceneTileZ * 128 + entity->size * 64;
        entity->x = (dx0 * (duration - delta) + dx1 * delta) / duration;
        entity->z = (dz0 * (duration - delta) + dz1 * delta) / duration;
    }

    entity->seqTrigger = 0;

    if (entity->forceMoveFaceDirection == 0) {
        entity->dstYaw = 1024;
    }

    if (entity->forceMoveFaceDirection == 1) {
        entity->dstYaw = 1536;
    }

    if (entity->forceMoveFaceDirection == 2) {
        entity->dstYaw = 0;
    }

    if (entity->forceMoveFaceDirection == 3) {
        entity->dstYaw = 512;
    }

    entity->yaw = entity->dstYaw;
}

static void updateMovement(PathingEntity *entity) {
    entity->secondarySeqId = entity->seqStandId;

    if (entity->pathLength == 0) {
        entity->seqTrigger = 0;
        return;
    }

    if (entity->primarySeqId != -1 && entity->primarySeqDelay == 0) {
        SeqType *seq = _SeqType.instances[entity->primarySeqId];
        if (!seq->walkmerge) {
            entity->seqTrigger++;
            return;
        }
    }

    int x = entity->x;
    int z = entity->z;
    int dstX = entity->pathTileX[entity->pathLength - 1] * 128 + entity->size * 64;
    int dstZ = entity->pathTileZ[entity->pathLength - 1] * 128 + entity->size * 64;

    if (dstX - x <= 256 && dstX - x >= -256 && dstZ - z <= 256 && dstZ - z >= -256) {
        if (x < dstX) {
            if (z < dstZ) {
                entity->dstYaw = 1280;
            } else if (z > dstZ) {
                entity->dstYaw = 1792;
            } else {
                entity->dstYaw = 1536;
            }
        } else if (x > dstX) {
            if (z < dstZ) {
                entity->dstYaw = 768;
            } else if (z > dstZ) {
                entity->dstYaw = 256;
            } else {
                entity->dstYaw = 512;
            }
        } else if (z < dstZ) {
            entity->dstYaw = 1024;
        } else {
            entity->dstYaw = 0;
        }

        int deltaYaw = entity->dstYaw - entity->yaw & 0x7ff;
        if (deltaYaw > 1024) {
            deltaYaw -= 2048;
        }

        int seqId = entity->seqTurnAroundId;
        if (deltaYaw >= -256 && deltaYaw <= 256) {
            seqId = entity->seqWalkId;
        } else if (deltaYaw >= 256 && deltaYaw < 768) {
            seqId = entity->seqTurnRightId;
        } else if (deltaYaw >= -768 && deltaYaw <= -256) {
            seqId = entity->seqTurnLeftId;
        }

        if (seqId == -1) {
            seqId = entity->seqWalkId;
        }

        entity->secondarySeqId = seqId;
        int moveSpeed = 4;
        if (entity->yaw != entity->dstYaw && entity->targetId == -1) {
            moveSpeed = 2;
        }

        if (entity->pathLength > 2) {
            moveSpeed = 6;
        }

        if (entity->pathLength > 3) {
            moveSpeed = 8;
        }

        if (entity->seqTrigger > 0 && entity->pathLength > 1) {
            moveSpeed = 8;
            entity->seqTrigger--;
        }

        if (entity->pathRunning[entity->pathLength - 1]) {
            moveSpeed <<= 0x1;
        }

        if (moveSpeed >= 8 && entity->secondarySeqId == entity->seqWalkId && entity->seqRunId != -1) {
            entity->secondarySeqId = entity->seqRunId;
        }

        if (x < dstX) {
            entity->x += moveSpeed;
            if (entity->x > dstX) {
                entity->x = dstX;
            }
        } else if (x > dstX) {
            entity->x -= moveSpeed;
            if (entity->x < dstX) {
                entity->x = dstX;
            }
        }
        if (z < dstZ) {
            entity->z += moveSpeed;
            if (entity->z > dstZ) {
                entity->z = dstZ;
            }
        } else if (z > dstZ) {
            entity->z -= moveSpeed;
            if (entity->z < dstZ) {
                entity->z = dstZ;
            }
        }

        if (entity->x == dstX && entity->z == dstZ) {
            entity->pathLength--;
        }
    } else {
        entity->x = dstX;
        entity->z = dstZ;
    }
}

static void updateFacingDirection(Client *c, PathingEntity *e) {
    if (e->targetId != -1 && e->targetId < 32768) {
        NpcEntity *npc = c->npcs[e->targetId];
        if (npc) {
            int dstX = e->x - npc->pathing_entity.x;
            int dstZ = e->z - npc->pathing_entity.z;

            if (dstX != 0 || dstZ != 0) {
                e->dstYaw = (int)(atan2(dstX, dstZ) * 325.949) & 0x7ff;
            }
        }
    }

    if (e->targetId >= 32768) {
        int index = e->targetId - 32768;
        if (index == c->local_pid) {
            index = LOCAL_PLAYER_INDEX;
        }

        PlayerEntity *player = c->players[index];
        if (player) {
            int dstX = e->x - player->pathing_entity.x;
            int dstZ = e->z - player->pathing_entity.z;

            if (dstX != 0 || dstZ != 0) {
                e->dstYaw = (int)(atan2(dstX, dstZ) * 325.949) & 0x7ff;
            }
        }
    }

    if ((e->targetTileX != 0 || e->targetTileZ != 0) && (e->pathLength == 0 || e->seqTrigger > 0)) {
        int dstX = e->x - (e->targetTileX - c->sceneBaseTileX - c->sceneBaseTileX) * 64;
        int dstZ = e->z - (e->targetTileZ - c->sceneBaseTileZ - c->sceneBaseTileZ) * 64;

        if (dstX != 0 || dstZ != 0) {
            e->dstYaw = (int)(atan2(dstX, dstZ) * 325.949) & 0x7ff;
        }

        e->targetTileX = 0;
        e->targetTileZ = 0;
    }

    int remainingYaw = e->dstYaw - e->yaw & 0x7ff;

    if (remainingYaw != 0) {
        if (remainingYaw < 32 || remainingYaw > 2016) {
            e->yaw = e->dstYaw;
        } else if (remainingYaw > 1024) {
            e->yaw -= 32;
        } else {
            e->yaw += 32;
        }

        e->yaw &= 0x7ff;

        if (e->secondarySeqId == e->seqStandId && e->yaw != e->dstYaw) {
            if (e->seqTurnId != -1) {
                e->secondarySeqId = e->seqTurnId;
                return;
            }

            e->secondarySeqId = e->seqWalkId;
        }
    }
}

static void updateSequences(PathingEntity *e) {
    e->seqStretches = false;

    SeqType *seq;
    if (e->secondarySeqId != -1) {
        seq = _SeqType.instances[e->secondarySeqId];
        e->secondarySeqCycle++;
        if (e->secondarySeqFrame < seq->frameCount && e->secondarySeqCycle > seq->delay[e->secondarySeqFrame]) {
            e->secondarySeqCycle = 0;
            e->secondarySeqFrame++;
        }
        if (e->secondarySeqFrame >= seq->frameCount) {
            e->secondarySeqCycle = 0;
            e->secondarySeqFrame = 0;
        }
    }

    if (e->primarySeqId != -1 && e->primarySeqDelay == 0) {
        seq = _SeqType.instances[e->primarySeqId];
        e->primarySeqCycle++;
        while (e->primarySeqFrame < seq->frameCount && e->primarySeqCycle > seq->delay[e->primarySeqFrame]) {
            e->primarySeqCycle -= seq->delay[e->primarySeqFrame];
            e->primarySeqFrame++;
        }

        if (e->primarySeqFrame >= seq->frameCount) {
            e->primarySeqFrame -= seq->replayoff;
            e->primarySeqLoop++;
            if (e->primarySeqLoop >= seq->replaycount) {
                e->primarySeqId = -1;
            }
            if (e->primarySeqFrame < 0 || e->primarySeqFrame >= seq->frameCount) {
                e->primarySeqId = -1;
            }
        }

        e->seqStretches = seq->stretches;
    }

    if (e->primarySeqDelay > 0) {
        e->primarySeqDelay--;
    }

    if (e->spotanimId != -1 && _Client.loop_cycle >= e->spotanimLastCycle) {
        if (e->spotanimFrame < 0) {
            e->spotanimFrame = 0;
        }

        seq = _SpotAnimType.instances[e->spotanimId]->seq;
        e->spotanimCycle++;
        while (e->spotanimFrame < seq->frameCount && e->spotanimCycle > seq->delay[e->spotanimFrame]) {
            e->spotanimCycle -= seq->delay[e->spotanimFrame];
            e->spotanimFrame++;
        }

        if (e->spotanimFrame >= seq->frameCount) {
            if (e->spotanimFrame < 0 || e->spotanimFrame >= seq->frameCount) {
                e->spotanimId = -1;
            }
        }
    }
}

static void updateEntity(Client *c, PathingEntity *entity, int size) {
    (void)size;
    if (entity->x < 128 || entity->z < 128 || entity->x >= 13184 || entity->z >= 13184) {
        entity->primarySeqId = -1;
        entity->spotanimId = -1;
        entity->forceMoveEndCycle = 0;
        entity->forceMoveStartCycle = 0;
        entity->x = entity->pathTileX[0] * 128 + entity->size * 64;
        entity->z = entity->pathTileZ[0] * 128 + entity->size * 64;
        entity->pathLength = 0;
    }

    if (entity == &c->local_player->pathing_entity && (entity->x < 1536 || entity->z < 1536 || entity->x >= 11776 || entity->z >= 11776)) {
        entity->primarySeqId = -1;
        entity->spotanimId = -1;
        entity->forceMoveEndCycle = 0;
        entity->forceMoveStartCycle = 0;
        entity->x = entity->pathTileX[0] * 128 + entity->size * 64;
        entity->z = entity->pathTileZ[0] * 128 + entity->size * 64;
        entity->pathLength = 0;
    }

    if (entity->forceMoveEndCycle > _Client.loop_cycle) {
        updateForceMovement(entity);
    } else if (entity->forceMoveStartCycle >= _Client.loop_cycle) {
        startForceMovement(entity);
    } else {
        updateMovement(entity);
    }

    updateFacingDirection(c, entity);
    updateSequences(entity);
}

void updatePlayers(Client *c) {
    for (int i = -1; i < c->player_count; i++) {
        int index;
        if (i == -1) {
            index = LOCAL_PLAYER_INDEX;
        } else {
            index = c->player_ids[i];
        }

        PlayerEntity *player = c->players[index];
        if (player) {
            updateEntity(c, &player->pathing_entity, 1);
        }
    }

    _Client.cyclelogic6++;
    if (_Client.cyclelogic6 > 1406) {
        _Client.cyclelogic6 = 0;
        // ANTICHEAT_c->CYCLELOGIC6
        p1isaac(c->out, 219);
        p1(c->out, 0);
        int start = c->out->pos;
        p1(c->out, 162);
        p1(c->out, 22);
        if ((int)(jrand() * 2.0) == 0) {
            p1(c->out, 84);
        }
        p2(c->out, 31824);
        p2(c->out, 13490);
        if ((int)(jrand() * 2.0) == 0) {
            p1(c->out, 123);
        }
        if ((int)(jrand() * 2.0) == 0) {
            p1(c->out, 134);
        }
        p1(c->out, 100);
        p1(c->out, 94);
        p2(c->out, 35521);
        psize1(c->out, c->out->pos - start);
    }
}

static void updateNpcs(Client *c) {
    for (int i = 0; i < c->npc_count; i++) {
        int id = c->npc_ids[i];
        NpcEntity *npc = c->npcs[id];
        if (npc) {
            updateEntity(c, &npc->pathing_entity, npc->type->size);
        }
    }
}

static void client_update_orbit_camera(Client *c) {
    int orbitX = c->local_player->pathing_entity.x + c->camera_anticheat_offset_x;
    int orbitZ = c->local_player->pathing_entity.z + c->camera_anticheat_offset_z;
    if (c->orbitCameraX - orbitX < -500 || c->orbitCameraX - orbitX > 500 || c->orbitCameraZ - orbitZ < -500 || c->orbitCameraZ - orbitZ > 500) {
        c->orbitCameraX = orbitX;
        c->orbitCameraZ = orbitZ;
    }
    if (c->orbitCameraX != orbitX) {
        c->orbitCameraX += (orbitX - c->orbitCameraX) / 16;
    }
    if (c->orbitCameraZ != orbitZ) {
        c->orbitCameraZ += (orbitZ - c->orbitCameraZ) / 16;
    }
    if (c->shell->action_key[1] == 1) {
        c->orbitCameraYawVelocity += (-c->orbitCameraYawVelocity - 24) / 2;
    } else if (c->shell->action_key[2] == 1) {
        c->orbitCameraYawVelocity += (24 - c->orbitCameraYawVelocity) / 2;
    } else {
        c->orbitCameraYawVelocity /= 2;
    }
    if (c->shell->action_key[3] == 1) {
        c->orbitCameraPitchVelocity += (12 - c->orbitCameraPitchVelocity) / 2;
    } else if (c->shell->action_key[4] == 1) {
        c->orbitCameraPitchVelocity += (-c->orbitCameraPitchVelocity - 12) / 2;
    } else {
        c->orbitCameraPitchVelocity /= 2;
    }
    c->orbit_camera_yaw = c->orbit_camera_yaw + c->orbitCameraYawVelocity / 2 & 0x7ff;
    c->orbit_camera_pitch += c->orbitCameraPitchVelocity / 2;
    if (c->orbit_camera_pitch < 128) {
        c->orbit_camera_pitch = 128;
    }
    if (c->orbit_camera_pitch > 383) {
        c->orbit_camera_pitch = 383;
    }

    int orbitTileX = c->orbitCameraX >> 7;
    int orbitTileZ = c->orbitCameraZ >> 7;
    int orbitY = getHeightmapY(c, c->currentLevel, c->orbitCameraX, c->orbitCameraZ);
    int maxY = 0;

    if (orbitTileX > 3 && orbitTileZ > 3 && orbitTileX < 100 && orbitTileZ < 100) {
        for (int x = orbitTileX - 4; x <= orbitTileX + 4; x++) {
            for (int z = orbitTileZ - 4; z <= orbitTileZ + 4; z++) {
                int level = c->currentLevel;
                if (level < 3 && (c->levelTileFlags[1][x][z] & 0x2) == 2) {
                    level++;
                }

                int y = orbitY - c->levelHeightmap[level][x][z];
                if (y > maxY) {
                    maxY = y;
                }
            }
        }
    }

    int clamp = maxY * 192;
    if (clamp > 98048) {
        clamp = 98048;
    }

    if (clamp < 32768) {
        clamp = 32768;
    }

    if (clamp > c->cameraPitchClamp) {
        c->cameraPitchClamp += (clamp - c->cameraPitchClamp) / 24;
    } else if (clamp < c->cameraPitchClamp) {
        c->cameraPitchClamp += (clamp - c->cameraPitchClamp) / 80;
    }
}

static bool client_try_move(Client *c, int srcX, int srcZ, int dx, int dz, int type, int locWidth, int locLength, int locRotation, int locShape, int forceapproach, bool tryNearest) {
    int8_t sceneWidth = 104;
    int8_t sceneLength = 104;
    for (int x = 0; x < sceneWidth; x++) {
        for (int z = 0; z < sceneLength; z++) {
            c->bfsDirection[x][z] = 0;
            c->bfsCost[x][z] = 99999999;
        }
    }

    int x = srcX;
    int z = srcZ;

    c->bfsDirection[srcX][srcZ] = 99;
    c->bfsCost[srcX][srcZ] = 0;

    int steps = 0;
    int length = 0;

    c->bfsStepX[steps] = srcX;
    c->bfsStepZ[steps++] = srcZ;

    bool arrived = false;
    int bufferSize = BFS_STEP_SIZE;
    int **flags = c->levelCollisionMap[c->currentLevel]->flags;

    while (length != steps) {
        x = c->bfsStepX[length];
        z = c->bfsStepZ[length];
        length = (length + 1) % bufferSize;

        if (x == dx && z == dz) {
            arrived = true;
            break;
        }

        if (locShape != 0) {
            int shape = locShape - 1;

            if ((shape <= WALL_SQUARECORNER || shape == WALL_DIAGONAL) && collisionmap_test_wall(c->levelCollisionMap[c->currentLevel], x, z, dx, dz, shape, locRotation)) {
                arrived = true;
                break;
            }

            if (shape <= WALLDECOR_DIAGONAL_BOTH && collisionmap_test_wdecor(c->levelCollisionMap[c->currentLevel], x, z, dx, dz, shape, locRotation)) {
                arrived = true;
                break;
            }
        }

        if (locWidth != 0 && locLength != 0 && collisionmap_test_loc(c->levelCollisionMap[c->currentLevel], x, z, dx, dz, locWidth, locLength, forceapproach)) {
            arrived = true;
            break;
        }

        int nextCost = c->bfsCost[x][z] + 1;
        if (x > 0 && c->bfsDirection[x - 1][z] == 0 && (flags[x - 1][z] & 0x280108) == 0) {
            c->bfsStepX[steps] = x - 1;
            c->bfsStepZ[steps] = z;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x - 1][z] = 2;
            c->bfsCost[x - 1][z] = nextCost;
        }

        if (x < sceneWidth - 1 && c->bfsDirection[x + 1][z] == 0 && (flags[x + 1][z] & 0x280180) == 0) {
            c->bfsStepX[steps] = x + 1;
            c->bfsStepZ[steps] = z;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x + 1][z] = 8;
            c->bfsCost[x + 1][z] = nextCost;
        }

        if (z > 0 && c->bfsDirection[x][z - 1] == 0 && (flags[x][z - 1] & 0x280102) == 0) {
            c->bfsStepX[steps] = x;
            c->bfsStepZ[steps] = z - 1;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x][z - 1] = 1;
            c->bfsCost[x][z - 1] = nextCost;
        }

        if (z < sceneLength - 1 && c->bfsDirection[x][z + 1] == 0 && (flags[x][z + 1] & 0x280120) == 0) {
            c->bfsStepX[steps] = x;
            c->bfsStepZ[steps] = z + 1;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x][z + 1] = 4;
            c->bfsCost[x][z + 1] = nextCost;
        }

        if (x > 0 && z > 0 && c->bfsDirection[x - 1][z - 1] == 0 && (flags[x - 1][z - 1] & 0x28010E) == 0 && (flags[x - 1][z] & 0x280108) == 0 && (flags[x][z - 1] & 0x280102) == 0) {
            c->bfsStepX[steps] = x - 1;
            c->bfsStepZ[steps] = z - 1;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x - 1][z - 1] = 3;
            c->bfsCost[x - 1][z - 1] = nextCost;
        }

        if (x < sceneWidth - 1 && z > 0 && c->bfsDirection[x + 1][z - 1] == 0 && (flags[x + 1][z - 1] & 0x280183) == 0 && (flags[x + 1][z] & 0x280180) == 0 && (flags[x][z - 1] & 0x280102) == 0) {
            c->bfsStepX[steps] = x + 1;
            c->bfsStepZ[steps] = z - 1;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x + 1][z - 1] = 9;
            c->bfsCost[x + 1][z - 1] = nextCost;
        }

        if (x > 0 && z < sceneLength - 1 && c->bfsDirection[x - 1][z + 1] == 0 && (flags[x - 1][z + 1] & 0x280138) == 0 && (flags[x - 1][z] & 0x280108) == 0 && (flags[x][z + 1] & 0x280120) == 0) {
            c->bfsStepX[steps] = x - 1;
            c->bfsStepZ[steps] = z + 1;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x - 1][z + 1] = 6;
            c->bfsCost[x - 1][z + 1] = nextCost;
        }

        if (x < sceneWidth - 1 && z < sceneLength - 1 && c->bfsDirection[x + 1][z + 1] == 0 && (flags[x + 1][z + 1] & 0x2801E0) == 0 && (flags[x + 1][z] & 0x280180) == 0 && (flags[x][z + 1] & 0x280120) == 0) {
            c->bfsStepX[steps] = x + 1;
            c->bfsStepZ[steps] = z + 1;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x + 1][z + 1] = 12;
            c->bfsCost[x + 1][z + 1] = nextCost;
        }
    }

    c->tryMoveNearest = 0;

    if (!arrived) {
        if (tryNearest) {
            int min = 100;
            for (int padding = 1; padding < 2; padding++) {
                for (int px = dx - padding; px <= dx + padding; px++) {
                    for (int pz = dz - padding; pz <= dz + padding; pz++) {
                        if (px >= 0 && pz >= 0 && px < 104 && pz < 104 && c->bfsCost[px][pz] < min) {
                            min = c->bfsCost[px][pz];
                            x = px;
                            z = pz;
                            c->tryMoveNearest = 1;
                            arrived = true;
                        }
                    }
                }

                if (arrived) {
                    break;
                }
            }
        }

        if (!arrived) {
            return false;
        }
    }

    length = 0;
    c->bfsStepX[length] = x;
    c->bfsStepZ[length++] = z;

    int dir = c->bfsDirection[x][z];
    int next = dir;
    while (x != srcX || z != srcZ) {
        if (next != dir) {
            dir = next;
            c->bfsStepX[length] = x;
            c->bfsStepZ[length++] = z;
        }

        if ((next & 0x2) != 0) {
            x++;
        } else if ((next & 0x8) != 0) {
            x--;
        }

        if ((next & 0x1) != 0) {
            z++;
        } else if ((next & 0x4) != 0) {
            z--;
        }

        next = c->bfsDirection[x][z];
    }

    if (length > 0) {
        bufferSize = length;

        if (length > 25) {
            bufferSize = 25;
        }

        length--;

        int startX = c->bfsStepX[length];
        int startZ = c->bfsStepZ[length];

        // TODO showdebug
        // if (c->showDebug && super.actionKey[6] == 1 && super.actionKey[7] == 1) {
        // 	// check if tile is already added, if so remove it
        // 	for (int i = 0; i < c->userTileMarkers.length; i++) {
        // 		if (c->userTileMarkers[i] != null && c->userTileMarkers[i].x == World3D.clickTileX && c->userTileMarkers[i].z == World3D.clickTileZ) {
        // 			c->userTileMarkers[i] = null;
        // 			return false;
        // 		}
        // 	}

        // 	// add new
        // 	c->userTileMarkers[c->userTileMarkerIndex] = new Ground(c->currentLevel, World3D.clickTileX, World3D.clickTileZ);
        // 	c->userTileMarkerIndex = c->userTileMarkerIndex + 1 & (c->userTileMarkers.length - 1);
        // 	return false;
        // }

        if (type == 0) {
            // MOVE_GAMECLICK
            p1isaac(c->out, 181);
            p1(c->out, bufferSize + bufferSize + 3);
        } else if (type == 1) {
            // MOVE_MINIMAPCLICK
            p1isaac(c->out, 165);
            p1(c->out, bufferSize + bufferSize + 3 + 14);
        } else if (type == 2) {
            // MOVE_OPCLICK
            p1isaac(c->out, 93);
            p1(c->out, bufferSize + bufferSize + 3);
        }

        if (c->shell->action_key[5] == 1) {
            p1(c->out, 1);
        } else {
            p1(c->out, 0);
        }

        p2(c->out, startX + c->sceneBaseTileX);
        p2(c->out, startZ + c->sceneBaseTileZ);
        c->flagSceneTileX = c->bfsStepX[0];
        c->flagSceneTileZ = c->bfsStepZ[0];

        for (int i = 1; i < bufferSize; i++) {
            length--;
            p1(c->out, c->bfsStepX[length] - startX);
            p1(c->out, c->bfsStepZ[length] - startZ);
        }

        return true;
    }

    return type != 1;
}

static bool interactWithLoc(Client *c, int opcode, int x, int z, int bitset) {
    int locId = bitset >> 14 & 0x7fff;
    int info = world3d_get_info(c->scene, c->currentLevel, x, z, bitset);
    if (info == -1) {
        return false;
    }

    int type = info & 0x1f;
    int angle = info >> 6 & 0x3;
    if (type == 10 || type == 11 || type == 22) {
        LocType *loc = loctype_get(locId);
        int width;
        int height;

        if (angle == 0 || angle == 2) {
            width = loc->width;
            height = loc->length;
        } else {
            width = loc->length;
            height = loc->width;
        }

        int forceapproach = loc->forceapproach;
        if (angle != 0) {
            forceapproach = (forceapproach << angle & 0xf) + (forceapproach >> (4 - angle));
        }

        client_try_move(c, c->local_player->pathing_entity.pathTileX[0], c->local_player->pathing_entity.pathTileZ[0], x, z, 2, width, height, 0, 0, forceapproach, false);
    } else {
        client_try_move(c, c->local_player->pathing_entity.pathTileX[0], c->local_player->pathing_entity.pathTileZ[0], x, z, 2, 0, 0, angle, type + 1, 0, false);
    }

    c->crossX = c->shell->mouse_click_x;
    c->crossY = c->shell->mouse_click_y;
    c->cross_mode = 2;
    c->cross_cycle = 0;

    p1isaac(c->out, opcode);
    p2(c->out, x + c->sceneBaseTileX);
    p2(c->out, z + c->sceneBaseTileZ);
    p2(c->out, locId);
    return true;
}

static void addFriend(Client *c, int64_t username) {
    if (username == 0L) {
        return;
    }

    if (c->friend_count >= 100) {
        client_add_message(c, 0, "Your friends list is full. Max of 100 hit", "");
        return;
    }

    char *displayName = jstring_format_name(jstring_from_base37(username));
    for (int i = 0; i < c->friend_count; i++) {
        if (c->friendName37[i] == username) {
            char buf[MAX_STR];
            sprintf(buf, "%s is already on your friend list", displayName);
            client_add_message(c, 0, buf, "");
            return;
        }
    }

    for (int i = 0; i < c->ignoreCount; i++) {
        if (c->ignoreName37[i] == username) {
            char buf[MAX_STR];
            sprintf(buf, "Please remove %s from your ignore list first", displayName);
            client_add_message(c, 0, buf, "");
            return;
        }
    }

    if (strcmp(displayName, c->local_player->name) != 0) {
        c->friendName[c->friend_count] = displayName;
        c->friendName37[c->friend_count] = username;
        c->friendWorld[c->friend_count] = 0;
        c->friend_count++;
        c->redraw_sidebar = true;

        // FRIENDLIST_ADD
        p1isaac(c->out, 118);
        p8(c->out, username);
    }
}

static void addIgnore(Client *c, int64_t username) {
    if (username == 0L) {
        return;
    }

    if (c->ignoreCount >= 100) {
        client_add_message(c, 0, "Your ignore list is full. Max of 100 hit", "");
        return;
    }

    char *displayName = jstring_format_name(jstring_from_base37(username));
    for (int i = 0; i < c->ignoreCount; i++) {
        if (c->ignoreName37[i] == username) {
            char buf[MAX_STR];
            sprintf(buf, "%s is already on your ignore list", displayName);
            client_add_message(c, 0, buf, "");
            return;
        }
    }

    for (int i = 0; i < c->friend_count; i++) {
        if (c->friendName37[i] == username) {
            char buf[MAX_STR];
            sprintf(buf, "Please remove %s from your friend list first", displayName);
            client_add_message(c, 0, buf, "");
            return;
        }
    }

    c->ignoreName37[c->ignoreCount++] = username;
    c->redraw_sidebar = true;
    // IGNORELIST_ADD
    p1isaac(c->out, 79);
    p8(c->out, username);
}

static void removeFriend(Client *c, int64_t username) {
    if (username == 0L) {
        return;
    }

    for (int i = 0; i < c->friend_count; i++) {
        if (c->friendName37[i] == username) {
            c->friend_count--;
            c->redraw_sidebar = true;
            for (int j = i; j < c->friend_count; j++) {
                c->friendName[j] = c->friendName[j + 1];
                c->friendWorld[j] = c->friendWorld[j + 1];
                c->friendName37[j] = c->friendName37[j + 1];
            }
            // FRIENDLIST_DEL
            p1isaac(c->out, 11);
            p8(c->out, username);
            return;
        }
    }
}

static void removeIgnore(Client *c, int64_t username) {
    if (username == 0L) {
        return;
    }

    for (int i = 0; i < c->ignoreCount; i++) {
        if (c->ignoreName37[i] == username) {
            c->ignoreCount--;
            c->redraw_sidebar = true;
            for (int j = i; j < c->ignoreCount; j++) {
                c->ignoreName37[j] = c->ignoreName37[j + 1];
            }
            // IGNORELIST_DEL
            p1isaac(c->out, 171);
            p8(c->out, username);
            return;
        }
    }
}

static void useMenuOption(Client *cl, int optionId) {
    if (optionId < 0) {
        return;
    }

    if (cl->chatback_input_open) {
        cl->chatback_input_open = false;
        cl->redraw_chatback = true;
    }

    int action = cl->menu_action[optionId];
    int a = cl->menuParamA[optionId];
    int b = cl->menuParamB[optionId];
    int c = cl->menuParamC[optionId];

    if (action >= 2000) {
        action -= 2000;
    }

    if (action == 903 || action == 363) {
        char *option = cl->menu_option[optionId];
        int tag = indexof(option, "@whi@");

        if (tag != -1) {
            option = substring(option, tag + 5, strlen(option));
            strtrim(option);
            char *name = jstring_format_name(jstring_from_base37(jstring_to_base37(option)));
            bool found = false;

            for (int i = 0; i < cl->player_count; i++) {
                PlayerEntity *player = cl->players[cl->player_ids[i]];

                if (player && player->name[0] && platform_strcasecmp(player->name, name) == 0) {
                    client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], player->pathing_entity.pathTileX[0], player->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);

                    if (action == 903) {
                        // OPPLAYER4
                        p1isaac(cl->out, 206);
                    } else if (action == 363) {
                        // OPPLAYER1
                        p1isaac(cl->out, 164);
                    }

                    p2(cl->out, cl->player_ids[i]);
                    found = true;
                    break;
                }
            }

            if (!found) {
                char buf[MAX_STR];
                sprintf(buf, "Unable to find %s", name);
                client_add_message(cl, 0, buf, "");
            }
        }
    } else if (action == 450) {
        // OPLOCU
        if (interactWithLoc(cl, 75, b, c, a)) {
            p2(cl->out, cl->objInterface);
            p2(cl->out, cl->objSelectedSlot);
            p2(cl->out, cl->objSelectedInterface);
        }
    } else if (action == 405 || action == 38 || action == 422 || action == 478 || action == 347) {
        if (action == 478) {
            if ((b & 0x3) == 0) {
                _Client.oplogic5++;
            }

            if (_Client.oplogic5 >= 90) {
                // ANTICHEAT_OPLOGIC5
                p1isaac(cl->out, 220);
            }

            // OPHELD4
            p1isaac(cl->out, 157);
        } else if (action == 347) {
            // OPHELD5
            p1isaac(cl->out, 211);
        } else if (action == 422) {
            // OPHELD3
            p1isaac(cl->out, 133);
        } else if (action == 405) {
            _Client.oplogic3 += a;
            if (_Client.oplogic3 >= 97) {
                // ANTICHEAT_OPLOGIC3
                p1isaac(cl->out, 30);
                p3(cl->out, 14953816);
            }

            // OPHELD1
            p1isaac(cl->out, 195);
        } else if (action == 38) {
            // OPHELD2
            p1isaac(cl->out, 71);
        }

        p2(cl->out, a);
        p2(cl->out, b);
        p2(cl->out, c);
        cl->selected_cycle = 0;
        cl->selectedInterface = c;
        cl->selectedItem = b;
        cl->selected_area = 2;

        if (_Component.instances[c]->layer == cl->viewport_interface_id) {
            cl->selected_area = 1;
        }

        if (_Component.instances[c]->layer == cl->chat_interface_id) {
            cl->selected_area = 3;
        }
    } else if (action == 728 || action == 542 || action == 6 || action == 963 || action == 245) {
        NpcEntity *npc = cl->npcs[a];
        if (npc) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], npc->pathing_entity.pathTileX[0], npc->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);

            cl->crossX = cl->shell->mouse_click_x;
            cl->crossY = cl->shell->mouse_click_y;
            cl->cross_mode = 2;
            cl->cross_cycle = 0;

            if (action == 542) {
                // OPNPC2
                p1isaac(cl->out, 8);
            } else if (action == 6) {
                if ((a & 0x3) == 0) {
                    _Client.oplogic2++;
                }

                if (_Client.oplogic2 >= 124) {
                    // ANTICHEAT_OPLOGIC2
                    p1isaac(cl->out, 88);
                    p4(cl->out, 0);
                }

                // OPNPC3
                p1isaac(cl->out, 27);
            } else if (action == 963) {
                // OPNPC4
                p1isaac(cl->out, 113);
            } else if (action == 728) {
                // OPNPC1
                p1isaac(cl->out, 194);
            } else if (action == 245) {
                if ((a & 0x3) == 0) {
                    _Client.oplogic4++;
                }

                if (_Client.oplogic4 >= 85) {
                    // ANTICHEAT_OPLOGIC4
                    p1isaac(cl->out, 176);
                    p2(cl->out, 39596);
                }

                // OPNPC5
                p1isaac(cl->out, 100);
            }

            p2(cl->out, a);
        }
    } else if (action == 217) {
        bool success = client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], b, c, 2, 0, 0, 0, 0, 0, false);
        if (!success) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], b, c, 2, 1, 1, 0, 0, 0, false);
        }

        cl->crossX = cl->shell->mouse_click_x;
        cl->crossY = cl->shell->mouse_click_y;
        cl->cross_mode = 2;
        cl->cross_cycle = 0;

        // OPOBJU
        p1isaac(cl->out, 239);
        p2(cl->out, b + cl->sceneBaseTileX);
        p2(cl->out, c + cl->sceneBaseTileZ);
        p2(cl->out, a);
        p2(cl->out, cl->objInterface);
        p2(cl->out, cl->objSelectedSlot);
        p2(cl->out, cl->objSelectedInterface);
    } else if (action == 1175) {
        int locId = a >> 14 & 0x7fff;
        LocType *loc = loctype_get(locId);

        char examine[MAX_STR];
        if (!loc->desc) {
            sprintf(examine, "It's a %s.", loc->name);
        } else {
            strcpy(examine, loc->desc);
        }

        client_add_message(cl, 0, examine, "");
    } else if (action == 285) {
        // OPLOC1
        interactWithLoc(cl, 245, b, c, a);
    } else if (action == 881) {
        // OPHELDU
        p1isaac(cl->out, 130);
        p2(cl->out, a);
        p2(cl->out, b);
        p2(cl->out, c);
        p2(cl->out, cl->objInterface);
        p2(cl->out, cl->objSelectedSlot);
        p2(cl->out, cl->objSelectedInterface);

        cl->selected_cycle = 0;
        cl->selectedInterface = c;
        cl->selectedItem = b;
        cl->selected_area = 2;

        if (_Component.instances[c]->layer == cl->viewport_interface_id) {
            cl->selected_area = 1;
        }

        if (_Component.instances[c]->layer == cl->chat_interface_id) {
            cl->selected_area = 3;
        }
    } else if (action == 391) {
        // OPHELDT
        p1isaac(cl->out, 48);
        p2(cl->out, a);
        p2(cl->out, b);
        p2(cl->out, c);
        p2(cl->out, cl->activeSpellId);

        cl->selected_cycle = 0;
        cl->selectedInterface = c;
        cl->selectedItem = b;
        cl->selected_area = 2;

        if (_Component.instances[c]->layer == cl->viewport_interface_id) {
            cl->selected_area = 1;
        }

        if (_Component.instances[c]->layer == cl->chat_interface_id) {
            cl->selected_area = 3;
        }
    } else if (action == 660) {
        if (cl->menu_visible) {
            world3d_click(b - 8, c - 11);
        } else {
            world3d_click(cl->shell->mouse_click_x - 8, cl->shell->mouse_click_y - 11);
        }
    } else if (action == 188) {
        cl->obj_selected = 1;
        cl->objSelectedSlot = b;
        cl->objSelectedInterface = c;
        cl->objInterface = a;
        cl->objSelectedName = objtype_get(a)->name;
        cl->spell_selected = 0;
        if (_Custom.item_outlines) {
            cl->redraw_sidebar = true;
        }
        return;
    } else if (action == 44) {
        if (!cl->pressed_continue_option) {
            // RESUME_PAUSEBUTTON
            p1isaac(cl->out, 235);
            p2(cl->out, c);
            cl->pressed_continue_option = true;
        }
    } else if (action == 1773) {
        ObjType *obj = objtype_get(a);
        char examine[MAX_STR];

        if (c >= 100000) {
            sprintf(examine, "%d x %s", c, obj->name);
        } else if (obj->desc[0] == '\0') {
            sprintf(examine, "It's a %s.", obj->name);
        } else {
            strcpy(examine, obj->desc);
        }

        client_add_message(cl, 0, examine, "");
    } else if (action == 900) {
        NpcEntity *npc = cl->npcs[a];

        if (npc) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], npc->pathing_entity.pathTileX[0], npc->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);
            cl->crossX = cl->shell->mouse_click_x;
            cl->crossY = cl->shell->mouse_click_y;
            cl->cross_mode = 2;
            cl->cross_cycle = 0;
            // OPNPCU
            p1isaac(cl->out, 202);
            p2(cl->out, a);
            p2(cl->out, cl->objInterface);
            p2(cl->out, cl->objSelectedSlot);
            p2(cl->out, cl->objSelectedInterface);
        }
    } else if (action == 1373 || action == 1544 || action == 151 || action == 1101) {
        PlayerEntity *player = cl->players[a];
        if (player) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], player->pathing_entity.pathTileX[0], player->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);

            cl->crossX = cl->shell->mouse_click_x;
            cl->crossY = cl->shell->mouse_click_y;
            cl->cross_mode = 2;
            cl->cross_cycle = 0;

            if (action == 1101) {
                // OPPLAYER1
                p1isaac(cl->out, 164);
            } else if (action == 151) {
                _Client.oplogic8++;
                if (_Client.oplogic8 >= 90) {
                    // ANTICHEAT_OPLOGIC8
                    p1isaac(cl->out, 2);
                    p2(cl->out, 31114);
                }

                // OPPLAYER2
                p1isaac(cl->out, 53);
            } else if (action == 1373) {
                // OPPLAYER4
                p1isaac(cl->out, 206);
            } else if (action == 1544) {
                // OPPLAYER3
                p1isaac(cl->out, 185);
            }

            p2(cl->out, a);
        }
    } else if (action == 265) {
        NpcEntity *npc = cl->npcs[a];
        if (npc) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], npc->pathing_entity.pathTileX[0], npc->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);

            cl->crossX = cl->shell->mouse_click_x;
            cl->crossY = cl->shell->mouse_click_y;
            cl->cross_mode = 2;
            cl->cross_cycle = 0;

            // OPNPCT
            p1isaac(cl->out, 134);
            p2(cl->out, a);
            p2(cl->out, cl->activeSpellId);
        }
    } else if (action == 679) {
        const char *option = cl->menu_option[optionId];
        int tag = indexof(option, "@whi@");

        if (tag != -1) {
            char *name = substring(option, tag + 5, strlen(option));
            strtrim(name);
            int64_t name37 = jstring_to_base37(name);
            int friend = -1;
            for (int i = 0; i < cl->friend_count; i++) {
                if (cl->friendName37[i] == name37) {
                    friend = i;
                    break;
                }
            }

            if (friend != -1 && cl->friendWorld[friend] > 0) {
                cl->redraw_chatback = true;
                cl->chatback_input_open = false;
                cl->show_social_input = true;
                cl->social_input[0] = '\0';
                cl->social_action = 3;
                cl->social_name37 = cl->friendName37[friend];
                sprintf(cl->social_message, "Enter message to send to %s", cl->friendName[friend]);
            }
        }
    } else if (action == 55) {
        // OPLOCT
        if (interactWithLoc(cl, 9, b, c, a)) {
            p2(cl->out, cl->activeSpellId);
        }
    } else if (action == 224 || action == 993 || action == 99 || action == 746 || action == 877) {
        bool success = client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], b, c, 2, 0, 0, 0, 0, 0, false);
        if (!success) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], b, c, 2, 1, 1, 0, 0, 0, false);
        }

        cl->crossX = cl->shell->mouse_click_x;
        cl->crossY = cl->shell->mouse_click_y;
        cl->cross_mode = 2;
        cl->cross_cycle = 0;

        if (action == 224) {
            // OPOBJ1
            p1isaac(cl->out, 140);
        } else if (action == 746) {
            // OPOBJ4
            p1isaac(cl->out, 178);
        } else if (action == 877) {
            // OPOBJ5
            p1isaac(cl->out, 247);
        } else if (action == 99) {
            // OPOBJ3
            p1isaac(cl->out, 200);
        } else if (action == 993) {
            // OPOBJ2
            p1isaac(cl->out, 40);
        }

        p2(cl->out, b + cl->sceneBaseTileX);
        p2(cl->out, c + cl->sceneBaseTileZ);
        p2(cl->out, a);
    } else if (action == 1607) {
        NpcEntity *npc = cl->npcs[a];
        if (npc) {
            char examine[MAX_STR];

            if (!npc->type->desc) {
                sprintf(examine, "It's a %s.", npc->type->name);
            } else {
                strcpy(examine, npc->type->desc);
            }

            client_add_message(cl, 0, examine, "");
        }
    } else if (action == 504) {
        // OPLOC2
        interactWithLoc(cl, 172, b, c, a);
    } else if (action == 930) {
        Component *com = _Component.instances[c];
        cl->spell_selected = 1;
        cl->activeSpellId = c;
        cl->activeSpellFlags = com->actionTarget;
        cl->obj_selected = 0;
        if (_Custom.item_outlines) {
            cl->redraw_sidebar = true;
        }

        char *prefix = com->actionVerb;
        bool free_prefix = false;
        if (indexof(prefix, " ") != -1) {
            prefix = substring(prefix, 0, indexof(prefix, " "));
            free_prefix = true;
        }

        char *suffix = com->actionVerb;
        bool free_suffix = false;
        if (indexof(suffix, " ") != -1) {
            suffix = substring(suffix, indexof(suffix, " ") + 1, strlen(suffix));
            free_suffix = true;
        }

        sprintf(cl->spellCaption, "%s %s %s", prefix, com->action, suffix);
        if (free_prefix) {
            free(prefix);
        }
        if (free_suffix) {
            free(suffix);
        }

        if (cl->activeSpellFlags == 16) {
            cl->redraw_sidebar = true;
            cl->selected_tab = 3;
            cl->redraw_sideicons = true;
        }

        return;
    } else if (action == 951) {
        Component *com = _Component.instances[c];
        bool notify = true;

        if (com->clientCode > 0) {
            notify = handleInterfaceAction(cl, com);
        }

        if (notify) {
            // IF_BUTTON
            p1isaac(cl->out, 155);
            p2(cl->out, c);
        }
    } else if (action == 602 || action == 596 || action == 22 || action == 892 || action == 415) {
        if (action == 22) {
            // INV_BUTTON3
            p1isaac(cl->out, 212);
        } else if (action == 415) {
            if ((c & 0x3) == 0) {
                _Client.oplogic7++;
            }

            if (_Client.oplogic7 >= 55) {
                // ANTICHEAT_OPLOGIC7
                p1isaac(cl->out, 17);
                p4(cl->out, 0);
            }

            // INV_BUTTON5
            p1isaac(cl->out, 6);
        } else if (action == 602) {
            // INV_BUTTON1
            p1isaac(cl->out, 31);
        } else if (action == 892) {
            if ((b & 0x3) == 0) {
                _Client.oplogic9++;
            }

            if (_Client.oplogic9 >= 130) {
                // ANTICHEAT_OPLOGIC9
                p1isaac(cl->out, 238);
                p1(cl->out, 177);
            }

            // INV_BUTTON4
            p1isaac(cl->out, 38);
        } else if (action == 596) {
            // INV_BUTTON2
            p1isaac(cl->out, 59);
        }

        p2(cl->out, a);
        p2(cl->out, b);
        p2(cl->out, c);

        cl->selected_cycle = 0;
        cl->selectedInterface = c;
        cl->selectedItem = b;
        cl->selected_area = 2;

        if (_Component.instances[c]->layer == cl->viewport_interface_id) {
            cl->selected_area = 1;
        }

        if (_Component.instances[c]->layer == cl->chat_interface_id) {
            cl->selected_area = 3;
        }
    } else if (action == 581) {
        if ((a & 0x3) == 0) {
            _Client.oplogic1++;
        }

        if (_Client.oplogic1 >= 99) {
            // ANTICHEAT_OPLOGIC1
            p1isaac(cl->out, 7);
            p4(cl->out, 0);
        }

        // OPLOC4
        interactWithLoc(cl, 97, b, c, a);
    } else if (action == 965) {
        bool success = client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], b, c, 2, 0, 0, 0, 0, 0, false);
        if (!success) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], b, c, 2, 1, 1, 0, 0, 0, false);
        }

        cl->crossX = cl->shell->mouse_click_x;
        cl->crossY = cl->shell->mouse_click_y;
        cl->cross_mode = 2;
        cl->cross_cycle = 0;

        // OPOBJT
        p1isaac(cl->out, 138);
        p2(cl->out, b + cl->sceneBaseTileX);
        p2(cl->out, c + cl->sceneBaseTileZ);
        p2(cl->out, a);
        p2(cl->out, cl->activeSpellId);
    } else if (action == 1501) {
        _Client.oplogic6 += cl->sceneBaseTileZ;
        if (_Client.oplogic6 >= 92) {
            // ANTICHEAT_OPLOGIC6
            p1isaac(cl->out, 66);
            p4(cl->out, 0);
        }

        // OPLOC5
        interactWithLoc(cl, 116, b, c, a);
    } else if (action == 364) {
        // OPLOC3
        interactWithLoc(cl, 96, b, c, a);
    } else if (action == 1102) {
        ObjType *obj = objtype_get(a);
        char examine[MAX_STR];

        if (obj->desc[0] == '\0') {
            sprintf(examine, "It's a %s.", obj->name);
        } else {
            strcpy(examine, obj->desc);
        }
        client_add_message(cl, 0, examine, "");
    } else if (action == 960) {
        // IF_BUTTON
        p1isaac(cl->out, 155);
        p2(cl->out, c);

        Component *com = _Component.instances[c];
        if (com->scripts && com->scripts[0][0] == 5) {
            int varp = com->scripts[0][1];
            if (cl->varps[varp] != com->scriptOperand[0]) {
                cl->varps[varp] = com->scriptOperand[0];
                updateVarp(cl, varp);
                cl->redraw_sidebar = true;
            }
        }
    } else if (action == 34) {
        const char *option = cl->menu_option[optionId];
        int tag = indexof(option, "@whi@");

        if (tag != -1) {
            closeInterfaces(cl);

            strcpy(cl->reportAbuseInput, substring(option, tag + 5, strlen(option)));
            strtrim(cl->reportAbuseInput);
            cl->reportAbuseMuteOption = false;

            for (int i = 0; i < _Component.count; i++) {
                if (_Component.instances[i] && _Component.instances[i]->clientCode == 600) {
                    cl->reportAbuseInterfaceID = cl->viewport_interface_id = _Component.instances[i]->layer;
                    break;
                }
            }
        }
    } else if (action == 947) {
        closeInterfaces(cl);
    } else if (action == 367) {
        PlayerEntity *player = cl->players[a];
        if (player) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], player->pathing_entity.pathTileX[0], player->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);

            cl->crossX = cl->shell->mouse_click_x;
            cl->crossY = cl->shell->mouse_click_y;
            cl->cross_mode = 2;
            cl->cross_cycle = 0;

            // OPPLAYERU
            p1isaac(cl->out, 248);
            p2(cl->out, a);
            p2(cl->out, cl->objInterface);
            p2(cl->out, cl->objSelectedSlot);
            p2(cl->out, cl->objSelectedInterface);
        }
    } else if (action == 465) {
        // IF_BUTTON
        p1isaac(cl->out, 155);
        p2(cl->out, c);

        Component *com = _Component.instances[c];
        if (com->scripts && com->scripts[0][0] == 5) {
            int varp = com->scripts[0][1];
            cl->varps[varp] = 1 - cl->varps[varp];
            updateVarp(cl, varp);
            cl->redraw_sidebar = true;
        }
    } else if (action == 406 || action == 436 || action == 557 || action == 556) {
        const char *option = cl->menu_option[optionId];
        int tag = indexof(option, "@whi@");

        if (tag != -1) {
            char *username = substring(option, tag + 5, strlen(option));
            strtrim(username);
            int64_t username37 = jstring_to_base37(username);
            if (action == 406) {
                addFriend(cl, username37);
            } else if (action == 436) {
                addIgnore(cl, username37);
            } else if (action == 557) {
                removeFriend(cl, username37);
            } else if (action == 556) {
                removeIgnore(cl, username37);
            }
        }
    } else if (action == 651) {
        PlayerEntity *player = cl->players[a];

        if (player) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], player->pathing_entity.pathTileX[0], player->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);

            cl->crossX = cl->shell->mouse_click_x;
            cl->crossY = cl->shell->mouse_click_y;
            cl->cross_mode = 2;
            cl->cross_cycle = 0;

            // OPPLAYERT
            p1isaac(cl->out, 177);
            p2(cl->out, a);
            p2(cl->out, cl->activeSpellId);
        }
    }

    cl->obj_selected = 0;
    cl->spell_selected = 0;
    if (_Custom.item_outlines) {
        cl->redraw_sidebar = true;
    }
}

static void applyCutscene(Client *c) {
    int x = c->cutsceneSrcLocalTileX * 128 + 64;
    int z = c->cutsceneSrcLocalTileZ * 128 + 64;
    int y = getHeightmapY(c, c->currentLevel, c->cutsceneSrcLocalTileX, c->cutsceneSrcLocalTileZ) - c->cutsceneSrcHeight;

    if (c->cameraX < x) {
        c->cameraX += c->cutsceneMoveSpeed + (x - c->cameraX) * c->cutsceneMoveAcceleration / 1000;
        if (c->cameraX > x) {
            c->cameraX = x;
        }
    }

    if (c->cameraX > x) {
        c->cameraX -= c->cutsceneMoveSpeed + (c->cameraX - x) * c->cutsceneMoveAcceleration / 1000;
        if (c->cameraX < x) {
            c->cameraX = x;
        }
    }

    if (c->cameraY < y) {
        c->cameraY += c->cutsceneMoveSpeed + (y - c->cameraY) * c->cutsceneMoveAcceleration / 1000;
        if (c->cameraY > y) {
            c->cameraY = y;
        }
    }

    if (c->cameraY > y) {
        c->cameraY -= c->cutsceneMoveSpeed + (c->cameraY - y) * c->cutsceneMoveAcceleration / 1000;
        if (c->cameraY < y) {
            c->cameraY = y;
        }
    }

    if (c->cameraZ < z) {
        c->cameraZ += c->cutsceneMoveSpeed + (z - c->cameraZ) * c->cutsceneMoveAcceleration / 1000;
        if (c->cameraZ > z) {
            c->cameraZ = z;
        }
    }

    if (c->cameraZ > z) {
        c->cameraZ -= c->cutsceneMoveSpeed + (c->cameraZ - z) * c->cutsceneMoveAcceleration / 1000;
        if (c->cameraZ < z) {
            c->cameraZ = z;
        }
    }

    x = c->cutsceneDstLocalTileX * 128 + 64;
    z = c->cutsceneDstLocalTileZ * 128 + 64;
    y = getHeightmapY(c, c->currentLevel, c->cutsceneDstLocalTileX, c->cutsceneDstLocalTileZ) - c->cutsceneDstHeight;

    int deltaX = x - c->cameraX;
    int deltaY = y - c->cameraY;
    int deltaZ = z - c->cameraZ;

    int distance = (int)sqrt(deltaX * deltaX + deltaZ * deltaZ);
    int pitch = (int)(atan2(deltaY, distance) * 325.949) & 0x7ff;
    int yaw = (int)(atan2(deltaX, deltaZ) * -325.949) & 0x7ff;

    if (pitch < 128) {
        pitch = 128;
    }

    if (pitch > 383) {
        pitch = 383;
    }

    if (c->cameraPitch < pitch) {
        c->cameraPitch += c->cutsceneRotateSpeed + (pitch - c->cameraPitch) * c->cutsceneRotateAcceleration / 1000;
        if (c->cameraPitch > pitch) {
            c->cameraPitch = pitch;
        }
    }

    if (c->cameraPitch > pitch) {
        c->cameraPitch -= c->cutsceneRotateSpeed + (c->cameraPitch - pitch) * c->cutsceneRotateAcceleration / 1000;
        if (c->cameraPitch < pitch) {
            c->cameraPitch = pitch;
        }
    }

    int deltaYaw = yaw - c->cameraYaw;
    if (deltaYaw > 1024) {
        deltaYaw -= 2048;
    }

    if (deltaYaw < -1024) {
        deltaYaw += 2048;
    }

    if (deltaYaw > 0) {
        c->cameraYaw += c->cutsceneRotateSpeed + deltaYaw * c->cutsceneRotateAcceleration / 1000;
        c->cameraYaw &= 0x7ff;
    }

    if (deltaYaw < 0) {
        c->cameraYaw -= c->cutsceneRotateSpeed + -deltaYaw * c->cutsceneRotateAcceleration / 1000;
        c->cameraYaw &= 0x7ff;
    }

    int tmp = yaw - c->cameraYaw;
    if (tmp > 1024) {
        tmp -= 2048;
    }

    if (tmp < -1024) {
        tmp += 2048;
    }

    if ((tmp < 0 && deltaYaw > 0) || (tmp > 0 && deltaYaw < 0)) {
        c->cameraYaw = yaw;
    }
}

static void handleInputKey(Client *c) {
    while (true) {
        int key;
        do {
            while (true) {
                key = poll_key(c->shell);
                if (key == -1) {
                    return;
                }

                if (c->viewport_interface_id != -1 && c->viewport_interface_id == c->reportAbuseInterfaceID) {
                    size_t len = strlen(c->reportAbuseInput);
                    if (key == 8 && len > 0) {
                        c->reportAbuseInput[len - 1] = '\0';
                    }
                    break;
                }

                if (c->show_social_input) {
                    size_t len = strlen(c->social_input);
                    if (key >= 32 && key <= 122 && len < CHAT_LENGTH) {
                        c->social_input[len] = (char)key;
                        c->social_input[len + 1] = '\0';
                        c->redraw_chatback = true;
                    }

                    if (key == 8 && len > 0) {
                        c->social_input[len - 1] = '\0';
                        c->redraw_chatback = true;
                    }

                    if (key == 13 || key == 10) {
                        c->show_social_input = false;
                        c->redraw_chatback = true;

                        int64_t username;
                        if (c->social_action == 1) {
                            username = jstring_to_base37(c->social_input);
                            addFriend(c, username);
                        }

                        if (c->social_action == 2 && c->friend_count > 0) {
                            username = jstring_to_base37(c->social_input);
                            removeFriend(c, username);
                        }

                        if (c->social_action == 3 && len > 0) {
                            // MESSAGE_PRIVATE
                            p1isaac(c->out, 148);
                            p1(c->out, 0);
                            int start = c->out->pos;
                            p8(c->out, c->social_name37);
                            wordpack_pack(c->out, c->social_input);
                            psize1(c->out, c->out->pos - start);
                            jstring_to_sentence_case(c->social_input);
                            // TODO
                            // c->social_input = wordfilter_filter(c->social_input);
                            client_add_message(c, 6, c->social_input, jstring_format_name(jstring_from_base37(c->social_name37)));
                            if (c->private_chat_setting == 2) {
                                c->private_chat_setting = 1;
                                c->redraw_privacy_settings = true;
                                // CHAT_SETMODE
                                p1isaac(c->out, 244);
                                p1(c->out, c->public_chat_setting);
                                p1(c->out, c->private_chat_setting);
                                p1(c->out, c->trade_chat_setting);
                            }
                        }

                        if (c->social_action == 4 && c->ignoreCount < 100) {
                            username = jstring_to_base37(c->social_input);
                            addIgnore(c, username);
                        }

                        if (c->social_action == 5 && c->ignoreCount > 0) {
                            username = jstring_to_base37(c->social_input);
                            removeIgnore(c, username);
                        }
                    }
                } else if (c->chatback_input_open) {
                    size_t len = strlen(c->chatback_input);
                    if (key >= 48 && key <= 57 && len < CHATBACK_LENGTH) {
                        c->chatback_input[len] = (char)key;
                        c->chatback_input[len + 1] = '\0';
                        c->redraw_chatback = true;
                    }

                    if (key == 8 && len > 0) {
                        c->chatback_input[len - 1] = '\0';
                        c->redraw_chatback = true;
                    }

                    if (key == 13 || key == 10) {
                        if (len > 0) {
                            int value = 0;
                            // try {
                            value = atoi(c->chatback_input);
                            // } catch (Exception ignored) {
                            // }
                            // RESUME_P_COUNTDIALOG
                            p1isaac(c->out, 237);
                            p4(c->out, value);
                        }
                        c->chatback_input_open = false;
                        c->redraw_chatback = true;
                    }
                } else if (c->chat_interface_id == -1) {
                    size_t len = strlen(c->chat_typed);
                    if (_Custom.allow_debugprocs) {
                        if (key >= 32 && key <= 126 && len < CHAT_LENGTH) {
                            c->chat_typed[len] = (char)key;
                            c->chat_typed[len + 1] = '\0';
                            c->redraw_chatback = true;
                        }
                    } else {
                        if (key >= 32 && key <= 122 && len < CHAT_LENGTH) {
                            c->chat_typed[len] = (char)key;
                            c->chat_typed[len + 1] = '\0';
                            c->redraw_chatback = true;
                        }
                    }

                    if (key == 8 && len > 0) {
                        c->chat_typed[len - 1] = '\0';
                        c->redraw_chatback = true;
                    }

                    if ((key == 13 || key == 10) && strlen(c->chat_typed) > 0) {
                        // custom, originally only with frame or local servers
                        if (c->rights || _Custom.allow_commands) {
                            if (strcmp(c->chat_typed, "::clientdrop") == 0 /* && c->shell->window */) {
                                client_try_reconnect(c);
                            } else if (strcmp(c->chat_typed, "::noclip") == 0) {
                                for (int level = 0; level < 4; level++) {
                                    for (int x = 1; x < 104 - 1; x++) {
                                        for (int z = 1; z < 104 - 1; z++) {
                                            c->levelCollisionMap[level]->flags[x][z] = 0;
                                        }
                                    }
                                }
                            } else if (strcmp(c->chat_typed, "::debug") == 0) {
                                _Custom.showDebug = !_Custom.showDebug;
                            } else if (strcmp(c->chat_typed, "::perf") == 0) {
                                _Custom.showPerformance = !_Custom.showPerformance;
                            } else if (strcmp(c->chat_typed, "::camera") == 0) {
                                // _Custom.cameraEditor = !_Custom.cameraEditor;
                                // c->cutscene = _Custom.cameraEditor;
                                // c->cutsceneDstLocalTileX = 52;
                                // c->cutsceneDstLocalTileZ = 52;
                                // c->cutsceneSrcLocalTileX = 52;
                                // c->cutsceneSrcLocalTileZ = 52;
                                // c->cutsceneSrcHeight = 1000;
                                // c->cutsceneDstHeight = 1000;
                                // TODO
                                // } else if (c->chat_typed.startsWith("::camsrc ")) {
                                //     const char** args = c->chat_typed.split(" ");
                                //     if (args.length == 3) {
                                //         c->cutsceneSrcLocalTileX = atoi(args[1]);
                                //         c->cutsceneSrcLocalTileZ = atoi(args[2]);
                                //     } else if (args.length == 4) {
                                //         c->cutsceneSrcLocalTileX = atoi(args[1]);
                                //         c->cutsceneSrcLocalTileZ = atoi(args[2]);
                                //         c->cutsceneSrcHeight = atoi(args[3]);
                                //     }
                                // } else if (c->chat_typed.startsWith("::camdst ")) {
                                //     const char** args = c->chat_typed.split(" ");
                                //     if (args.length == 3) {
                                //         c->cutsceneDstLocalTileX = atoi(args[1]);
                                //         c->cutsceneDstLocalTileZ = atoi(args[2]);
                                //     } else if (args.length == 4) {
                                //         c->cutsceneDstLocalTileX = atoi(args[1]);
                                //         c->cutsceneDstLocalTileZ = atoi(args[2]);
                                //         c->cutsceneDstHeight = atoi(args[3]);
                                //     }
                                // }
                            }
                        }

                        if (strstartswith(c->chat_typed, "::")) {
                            // CLIENT_CHEAT
                            p1isaac(c->out, 4);
                            p1(c->out, (int)strlen(c->chat_typed) - 1);
                            char *sub = substring(c->chat_typed, 2, strlen(c->chat_typed));
                            pjstr(c->out, sub);
                            free(sub);
                        } else {
                            int8_t color = 0;
                            if (strstartswith(c->chat_typed, "yellow:")) {
                                color = 0;
                                strcpy(c->chat_typed, substring(c->chat_typed, 7, strlen(c->chat_typed)));
                            } else if (strstartswith(c->chat_typed, "red:")) {
                                color = 1;
                                strcpy(c->chat_typed, substring(c->chat_typed, 4, strlen(c->chat_typed)));
                            } else if (strstartswith(c->chat_typed, "green:")) {
                                color = 2;
                                strcpy(c->chat_typed, substring(c->chat_typed, 6, strlen(c->chat_typed)));
                            } else if (strstartswith(c->chat_typed, "cyan:")) {
                                color = 3;
                                strcpy(c->chat_typed, substring(c->chat_typed, 5, strlen(c->chat_typed)));
                            } else if (strstartswith(c->chat_typed, "purple:")) {
                                color = 4;
                                strcpy(c->chat_typed, substring(c->chat_typed, 7, strlen(c->chat_typed)));
                            } else if (strstartswith(c->chat_typed, "white:")) {
                                color = 5;
                                strcpy(c->chat_typed, substring(c->chat_typed, 6, strlen(c->chat_typed)));
                            } else if (strstartswith(c->chat_typed, "flash1:")) {
                                color = 6;
                                strcpy(c->chat_typed, substring(c->chat_typed, 7, strlen(c->chat_typed)));
                            } else if (strstartswith(c->chat_typed, "flash2:")) {
                                color = 7;
                                strcpy(c->chat_typed, substring(c->chat_typed, 7, strlen(c->chat_typed)));
                            } else if (strstartswith(c->chat_typed, "flash3:")) {
                                color = 8;
                                strcpy(c->chat_typed, substring(c->chat_typed, 7, strlen(c->chat_typed)));
                            } else if (strstartswith(c->chat_typed, "glow1:")) {
                                color = 9;
                                strcpy(c->chat_typed, substring(c->chat_typed, 6, strlen(c->chat_typed)));
                            } else if (strstartswith(c->chat_typed, "glow2:")) {
                                color = 10;
                                strcpy(c->chat_typed, substring(c->chat_typed, 6, strlen(c->chat_typed)));
                            } else if (strstartswith(c->chat_typed, "glow3:")) {
                                color = 11;
                                strcpy(c->chat_typed, substring(c->chat_typed, 6, strlen(c->chat_typed)));
                            }

                            int8_t effect = 0;
                            if (strstartswith(c->chat_typed, "wave:")) {
                                effect = 1;
                                strcpy(c->chat_typed, substring(c->chat_typed, 5, strlen(c->chat_typed)));
                            }
                            if (strstartswith(c->chat_typed, "scroll:")) {
                                effect = 2;
                                strcpy(c->chat_typed, substring(c->chat_typed, 7, strlen(c->chat_typed)));
                            }

                            // MESSAGE_PUBLIC
                            p1isaac(c->out, 158);
                            p1(c->out, 0);
                            int start = c->out->pos;
                            p1(c->out, color);
                            p1(c->out, effect);
                            wordpack_pack(c->out, c->chat_typed);
                            psize1(c->out, c->out->pos - start);

                            jstring_to_sentence_case(c->chat_typed);
                            // TODO
                            // c->chat_typed = wordfilter_filter(c->chat_typed);
                            strcpy(c->local_player->pathing_entity.chat, c->chat_typed);
                            c->local_player->pathing_entity.chatColor = color;
                            c->local_player->pathing_entity.chatStyle = effect;
                            c->local_player->pathing_entity.chatTimer = 150;
                            client_add_message(c, 2, c->local_player->pathing_entity.chat, c->local_player->name);

                            if (c->public_chat_setting == 2) {
                                c->public_chat_setting = 3;
                                c->redraw_privacy_settings = true;
                                // CHAT_SETMODE
                                p1isaac(c->out, 244);
                                p1(c->out, c->public_chat_setting);
                                p1(c->out, c->private_chat_setting);
                                p1(c->out, c->trade_chat_setting);
                            }
                        }

                        c->chat_typed[0] = '\0';
                        c->redraw_chatback = true;
                    }
                }
            }
        } while ((key < 97 || key > 122) && (key < 65 || key > 90) && (key < 48 || key > 57) && key != 32);

        size_t len = strlen(c->reportAbuseInput);
        if (len < REPORT_ABUSE_LENGTH) {
            c->reportAbuseInput[len] = (char)key;
            c->reportAbuseInput[len + 1] = '\0';
        }
    }
}

static void handleMouseInput(Client *c) {
    if (c->obj_drag_area != 0) {
        return;
    }

    int button = c->shell->mouse_click_button;
    if (c->spell_selected == 1 && c->shell->mouse_click_x >= 520 && c->shell->mouse_click_y >= 165 && c->shell->mouse_click_x <= 788 && c->shell->mouse_click_y <= 230) {
        button = 0;
    }

    if (c->menu_visible) {
        if (button != 1) {
            int x = c->shell->mouse_x;
            int y = c->shell->mouse_y;

            if (c->menu_area == 0) {
                x -= 8;
                y -= 11;
            } else if (c->menu_area == 1) {
                x -= 562;
                y -= 231;
            } else if (c->menu_area == 2) {
                x -= 22;
                y -= 375;
            }

            if (x < c->menu_x - 10 || x > c->menu_x + c->menu_width + 10 || y < c->menu_y - 10 || y > c->menu_y + c->menu_height + 10) {
                c->menu_visible = false;
                if (c->menu_area == 1) {
                    c->redraw_sidebar = true;
                }
                if (c->menu_area == 2) {
                    c->redraw_chatback = true;
                }
            }
        }

        if (button == 1) {
            int menuX = c->menu_x;
            int menuY = c->menu_y;
            int menuWidth = c->menu_width;

            int clickX = c->shell->mouse_click_x;
            int clickY = c->shell->mouse_click_y;

            if (c->menu_area == 0) {
                clickX -= 8;
                clickY -= 11;
            } else if (c->menu_area == 1) {
                clickX -= 562;
                clickY -= 231;
            } else if (c->menu_area == 2) {
                clickX -= 22;
                clickY -= 375;
            }

            int option = -1;
            for (int i = 0; i < c->menu_size; i++) {
                int optionY = menuY + (c->menu_size - 1 - i) * 15 + 31;
                if (clickX > menuX && clickX < menuX + menuWidth && clickY > optionY - 13 && clickY < optionY + 3) {
                    option = i;
                }
            }

            if (option != -1) {
                useMenuOption(c, option);
            }

            c->menu_visible = false;
            if (c->menu_area == 1) {
                c->redraw_sidebar = true;
            } else if (c->menu_area == 2) {
                c->redraw_chatback = true;
            }
        }
    } else {
        if (button == 1 && c->menu_size > 0) {
            int action = c->menu_action[c->menu_size - 1];

            if (action == 602 || action == 596 || action == 22 || action == 892 || action == 415 || action == 405 || action == 38 || action == 422 || action == 478 || action == 347 || action == 188) {
                int slot = c->menuParamB[c->menu_size - 1];
                int comId = c->menuParamC[c->menu_size - 1];
                Component *com = _Component.instances[comId];

                if (com->draggable) {
                    c->objGrabThreshold = false;
                    c->obj_drag_cycles = 0;
                    c->objDragInterfaceId = comId;
                    c->objDragSlot = slot;
                    c->obj_drag_area = 2;
                    c->objGrabX = c->shell->mouse_click_x;
                    c->objGrabY = c->shell->mouse_click_y;

                    if (_Component.instances[comId]->layer == c->viewport_interface_id) {
                        c->obj_drag_area = 1;
                    }

                    if (_Component.instances[comId]->layer == c->chat_interface_id) {
                        c->obj_drag_area = 3;
                    }

                    return;
                }
            }
        }

        if (button == 1 && (c->mouseButtonsOption == 1 || isAddFriendOption(c, c->menu_size - 1)) && c->menu_size > 2) {
            button = 2;
        }

        if (button == 1 && c->menu_size > 0) {
            useMenuOption(c, c->menu_size - 1);
        }

        if (button != 2 || c->menu_size <= 0) {
            return;
        }

        showContextMenu(c);
    }
}

static void handleMinimapInput(Client *c) {
    if (c->shell->mouse_click_button == 1) {
        int x = c->shell->mouse_click_x - 21 - 561;
        int y = c->shell->mouse_click_y - 9 - 5;

        if (x >= 0 && y >= 0 && x < 146 && y < 151) {
            x -= 73;
            y -= 75;

            int yaw = c->orbit_camera_yaw + c->minimap_anticheat_angle & 0x7ff;
            int sinYaw = _Pix3D.sin_table[yaw];
            int cosYaw = _Pix3D.cos_table[yaw];

            sinYaw = (sinYaw * (c->minimap_zoom + 256)) >> 8;
            cosYaw = (cosYaw * (c->minimap_zoom + 256)) >> 8;

            int relX = (y * sinYaw + x * cosYaw) >> 11;
            int relY = (y * cosYaw - x * sinYaw) >> 11;

            int tileX = (c->local_player->pathing_entity.x + relX) >> 7;
            int tileZ = (c->local_player->pathing_entity.z - relY) >> 7;

            bool success = client_try_move(c, c->local_player->pathing_entity.pathTileX[0], c->local_player->pathing_entity.pathTileZ[0], tileX, tileZ, 1, 0, 0, 0, 0, 0, true);
            if (success) {
                // the additional 14-bytes in MOVE_MINIMAPCLICK
                p1(c->out, x);
                p1(c->out, y);
                p2(c->out, c->orbit_camera_yaw);
                p1(c->out, 57);
                p1(c->out, c->minimap_anticheat_angle);
                p1(c->out, c->minimap_zoom);
                p1(c->out, 89);
                p2(c->out, c->local_player->pathing_entity.x);
                p2(c->out, c->local_player->pathing_entity.z);
                p1(c->out, c->tryMoveNearest);
                p1(c->out, 63);
            }
        }
    }
}

static void handleTabInput(Client *c) {
    if (c->shell->mouse_click_button != 1) {
        return;
    }

    if (c->shell->mouse_click_x >= 549 && c->shell->mouse_click_x <= 583 && c->shell->mouse_click_y >= 195 && c->shell->mouse_click_y < 231 && c->tab_interface_id[0] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 0;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 579 && c->shell->mouse_click_x <= 609 && c->shell->mouse_click_y >= 194 && c->shell->mouse_click_y < 231 && c->tab_interface_id[1] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 1;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 607 && c->shell->mouse_click_x <= 637 && c->shell->mouse_click_y >= 194 && c->shell->mouse_click_y < 231 && c->tab_interface_id[2] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 2;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 635 && c->shell->mouse_click_x <= 679 && c->shell->mouse_click_y >= 194 && c->shell->mouse_click_y < 229 && c->tab_interface_id[3] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 3;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 676 && c->shell->mouse_click_x <= 706 && c->shell->mouse_click_y >= 194 && c->shell->mouse_click_y < 231 && c->tab_interface_id[4] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 4;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 704 && c->shell->mouse_click_x <= 734 && c->shell->mouse_click_y >= 194 && c->shell->mouse_click_y < 231 && c->tab_interface_id[5] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 5;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 732 && c->shell->mouse_click_x <= 766 && c->shell->mouse_click_y >= 195 && c->shell->mouse_click_y < 231 && c->tab_interface_id[6] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 6;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 550 && c->shell->mouse_click_x <= 584 && c->shell->mouse_click_y >= 492 && c->shell->mouse_click_y < 528 && c->tab_interface_id[7] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 7;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 582 && c->shell->mouse_click_x <= 612 && c->shell->mouse_click_y >= 492 && c->shell->mouse_click_y < 529 && c->tab_interface_id[8] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 8;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 609 && c->shell->mouse_click_x <= 639 && c->shell->mouse_click_y >= 492 && c->shell->mouse_click_y < 529 && c->tab_interface_id[9] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 9;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 637 && c->shell->mouse_click_x <= 681 && c->shell->mouse_click_y >= 493 && c->shell->mouse_click_y < 528 && c->tab_interface_id[10] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 10;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 679 && c->shell->mouse_click_x <= 709 && c->shell->mouse_click_y >= 492 && c->shell->mouse_click_y < 529 && c->tab_interface_id[11] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 11;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 706 && c->shell->mouse_click_x <= 736 && c->shell->mouse_click_y >= 492 && c->shell->mouse_click_y < 529 && c->tab_interface_id[12] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 12;
        c->redraw_sideicons = true;
    } else if (c->shell->mouse_click_x >= 734 && c->shell->mouse_click_x <= 768 && c->shell->mouse_click_y >= 492 && c->shell->mouse_click_y < 528 && c->tab_interface_id[13] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 13;
        c->redraw_sideicons = true;
    }

    _Client.cyclelogic1++;
    if (_Client.cyclelogic1 > 150) {
        _Client.cyclelogic1 = 0;
        // ANTICHEAT_CYCLELOGIC1
        p1isaac(c->out, 233);
        p1(c->out, 43);
    }
}

static void handleChatSettingsInput(Client *c) {
    if (c->shell->mouse_click_button != 1) {
        return;
    }

    if (c->shell->mouse_click_x >= 8 && c->shell->mouse_click_x <= 108 && c->shell->mouse_click_y >= 490 && c->shell->mouse_click_y <= 522) {
        c->public_chat_setting = (c->public_chat_setting + 1) % 4;
        c->redraw_privacy_settings = true;
        c->redraw_chatback = true;

        // CHAT_SETMODE
        p1isaac(c->out, 244);
        p1(c->out, c->public_chat_setting);
        p1(c->out, c->private_chat_setting);
        p1(c->out, c->trade_chat_setting);
    } else if (c->shell->mouse_click_x >= 137 && c->shell->mouse_click_x <= 237 && c->shell->mouse_click_y >= 490 && c->shell->mouse_click_y <= 522) {
        c->private_chat_setting = (c->private_chat_setting + 1) % 3;
        c->redraw_privacy_settings = true;
        c->redraw_chatback = true;

        // CHAT_SETMODE
        p1isaac(c->out, 244);
        p1(c->out, c->public_chat_setting);
        p1(c->out, c->private_chat_setting);
        p1(c->out, c->trade_chat_setting);
    } else if (c->shell->mouse_click_x >= 275 && c->shell->mouse_click_x <= 375 && c->shell->mouse_click_y >= 490 && c->shell->mouse_click_y <= 522) {
        c->trade_chat_setting = (c->trade_chat_setting + 1) % 3;
        c->redraw_privacy_settings = true;
        c->redraw_chatback = true;

        // CHAT_SETMODE
        p1isaac(c->out, 244);
        p1(c->out, c->public_chat_setting);
        p1(c->out, c->private_chat_setting);
        p1(c->out, c->trade_chat_setting);
    } else if (c->shell->mouse_click_x >= 416 && c->shell->mouse_click_x <= 516 && c->shell->mouse_click_y >= 490 && c->shell->mouse_click_y <= 522) {
        closeInterfaces(c);

        c->reportAbuseInput[0] = '\0';
        c->reportAbuseMuteOption = false;

        for (int i = 0; i < _Component.count; i++) {
            if (_Component.instances[i] && _Component.instances[i]->clientCode == 600) {
                c->reportAbuseInterfaceID = c->viewport_interface_id = _Component.instances[i]->layer;
                return;
            }
        }
    }
}

static void client_scenemap_free(Client *c) {
    for (int i = 0; i < c->sceneMapIndexLength; i++) {
        free(c->sceneMapLandData[i]);
        free(c->sceneMapLocData[i]);
    }
    free(c->sceneMapLandData);
    free(c->sceneMapLocData);
    free(c->sceneMapIndex);
    free(c->sceneMapLandDataIndexLength);
    free(c->sceneMapLocDataIndexLength);
}

void client_update_game(Client *c) {
    if (c->system_update_timer > 1) {
        c->system_update_timer--;
    }

    if (c->idle_timeout > 0) {
        c->idle_timeout--;
    }

    for (int i = 0; i < 5 && client_read(c); i++) {
    }

    if (c->ingame) {
        for (int wave = 0; wave < c->wave_count; wave++) {
            if (c->wave_delay[wave] <= 0) {
                // deprecated code unused to save wav for the browser to play it
                // bool failed = false;
                // try {
                // if (c->wave_ids[wave] != c->last_wave_id || c->wave_loops[wave] != c->last_wave_loops) {
                Packet *buf = wave_generate(c->wave_ids[wave], c->wave_loops[wave]);

                if (rs2_now() + (uint64_t)(buf->pos / 22) > c->last_wave_start_time + (uint64_t)(c->last_wave_length / 22)) {
                    c->last_wave_length = buf->pos;
                    c->last_wave_start_time = rs2_now();
                    // if (c->saveWave(buf->data, buf->pos)) {
                    c->last_wave_id = c->wave_ids[wave];
                    c->last_wave_loops = c->wave_loops[wave];
                    platform_play_wave(buf->data, buf->pos);
                    // } else {
                    // 	failed = true;
                    // }
                }
                // } else if (!c->replayWave()) {
                // 	failed = true;
                // }
                // } catch (Exception ignored) {
                // }

                // if (failed && c->wave_delay[wave] != -5) {
                // 	c->wave_delay[wave] = -5;
                // } else {
                c->wave_count--;
                for (int i = wave; i < c->wave_count; i++) {
                    c->wave_ids[i] = c->wave_ids[i + 1];
                    c->wave_loops[i] = c->wave_loops[i + 1];
                    c->wave_delay[i] = c->wave_delay[i + 1];
                }
                wave--;
                // }
            } else {
                c->wave_delay[wave]--;
            }
        }

        if (c->nextMusicDelay > 0) {
            c->nextMusicDelay -= 20;
            if (c->nextMusicDelay < 0) {
                c->nextMusicDelay = 0;
            }
            if (c->nextMusicDelay == 0 && c->midiActive && !_Client.lowmem) {
                platform_set_midi(c->currentMidi, c->midiCrc, c->midiSize);
            }
        }

        Packet *tracking = inputtracking_flush(&_InputTracking);
        if (tracking) {
            // EVENT_TRACKING
            p1isaac(c->out, 81);
            p2(c->out, tracking->pos);
            pdata(c->out, tracking->data, tracking->pos, 0);
            packet_release(tracking);
        }

        c->idle_net_cycles++;
        if (c->idle_net_cycles > 750) {
            client_try_reconnect(c);
        }

        updatePlayers(c);
        updateNpcs(c);
        updateEntityChats(c);
        updateMergeLocs(c);

        if ((c->shell->action_key[1] == 1 || c->shell->action_key[2] == 1 || c->shell->action_key[3] == 1 || c->shell->action_key[4] == 1) && c->camera_moved_write++ > 5) {
            c->camera_moved_write = 0;
            // EVENT_CAMERA_POSITION
            p1isaac(c->out, 189);
            p2(c->out, c->orbit_camera_pitch);
            p2(c->out, c->orbit_camera_yaw);
            p1(c->out, c->minimap_anticheat_angle);
            p1(c->out, c->minimap_zoom);
        }

        c->scene_delta++;
        if (c->cross_mode != 0) {
            c->cross_cycle += 20;
            if (c->cross_cycle >= 400) {
                c->cross_mode = 0;
            }
        }

        if (c->selected_area != 0) {
            c->selected_cycle++;
            if (c->selected_cycle >= 15) {
                if (c->selected_area == 2) {
                    c->redraw_sidebar = true;
                }
                if (c->selected_area == 3) {
                    c->redraw_chatback = true;
                }
                c->selected_area = 0;
            }
        }

        if (c->obj_drag_area != 0) {
            c->obj_drag_cycles++;
            if (c->shell->mouse_x > c->objGrabX + 5 || c->shell->mouse_x < c->objGrabX - 5 || c->shell->mouse_y > c->objGrabY + 5 || c->shell->mouse_y < c->objGrabY - 5) {
                c->objGrabThreshold = true;
            }

            if (c->shell->mouse_button == 0) {
                if (c->obj_drag_area == 2) {
                    c->redraw_sidebar = true;
                }
                if (c->obj_drag_area == 3) {
                    c->redraw_chatback = true;
                }

                c->obj_drag_area = 0;
                if (c->objGrabThreshold && c->obj_drag_cycles >= 5) {
                    c->hoveredSlotParentId = -1;
                    client_handle_input(c);
                    if (c->hoveredSlotParentId == c->objDragInterfaceId && c->hoveredSlot != c->objDragSlot) {
                        Component *com = _Component.instances[c->objDragInterfaceId];
                        int obj = com->invSlotObjId[c->hoveredSlot];
                        com->invSlotObjId[c->hoveredSlot] = com->invSlotObjId[c->objDragSlot];
                        com->invSlotObjId[c->objDragSlot] = obj;

                        int count = com->invSlotObjCount[c->hoveredSlot];
                        com->invSlotObjCount[c->hoveredSlot] = com->invSlotObjCount[c->objDragSlot];
                        com->invSlotObjCount[c->objDragSlot] = count;

                        // INV_BUTTOND
                        p1isaac(c->out, 159);
                        p2(c->out, c->objDragInterfaceId);
                        p2(c->out, c->objDragSlot);
                        p2(c->out, c->hoveredSlot);
                    }
                } else if ((c->mouseButtonsOption == 1 || isAddFriendOption(c, c->menu_size - 1)) && c->menu_size > 2) {
                    showContextMenu(c);
                } else if (c->menu_size > 0) {
                    useMenuOption(c, c->menu_size - 1);
                }

                c->selected_cycle = 10;
                c->shell->mouse_click_button = 0;
            }
        }

        _Client.cyclelogic3++;
        if (_Client.cyclelogic3 > 127) {
            _Client.cyclelogic3 = 0;
            // ANTICHEAT_CYCLELOGIC3
            p1isaac(c->out, 215);
            p3(c->out, 4991788);
        }

        if (_World3D.clickTileX != -1) {
            int x = _World3D.clickTileX;
            int z = _World3D.clickTileZ;
            bool success = client_try_move(c, c->local_player->pathing_entity.pathTileX[0], c->local_player->pathing_entity.pathTileZ[0], x, z, 0, 0, 0, 0, 0, 0, true);
            _World3D.clickTileX = -1;

            if (success) {
                c->crossX = c->shell->mouse_click_x;
                c->crossY = c->shell->mouse_click_y;
                c->cross_mode = 1;
                c->cross_cycle = 0;
            }
        }

        if (c->shell->mouse_click_button == 1 && c->modal_message[0]) {
            c->modal_message[0] = '\0';
            c->redraw_chatback = true;
            c->shell->mouse_click_button = 0;
        }

        handleMouseInput(c);
        handleMinimapInput(c);
        handleTabInput(c);
        handleChatSettingsInput(c);

        if (c->shell->mouse_button == 1 || c->shell->mouse_click_button == 1) {
            c->drag_cycles++;
        }

        if (c->scene_state == 2) {
            // NOTE unused
            // if (_Custom.cameraEditor) {
            //     update_camera_editor(c);
            // } else {
            client_update_orbit_camera(c);
            // }
        }
        if (c->scene_state == 2 && c->cutscene) {
            applyCutscene(c);
        }

        for (int i = 0; i < 5; i++) {
            c->cameraModifierCycle[i]++;
        }

        handleInputKey(c);
        c->shell->idle_cycles++;
        if (c->shell->idle_cycles > 4500) {
            c->idle_timeout = 250;
            c->shell->idle_cycles -= 500;
            // IDLE_TIMER
            p1isaac(c->out, 70);
        }

        c->cameraOffsetCycle++;
        if (c->cameraOffsetCycle > 500) {
            c->cameraOffsetCycle = 0;
            int _rand = (int)(jrand() * 8.0);
            if ((_rand & 0x1) == 1) {
                c->camera_anticheat_offset_x += c->cameraOffsetXModifier;
            }
            if ((_rand & 0x2) == 2) {
                c->camera_anticheat_offset_z += c->cameraOffsetZModifier;
            }
            if ((_rand & 0x4) == 4) {
                c->camera_anticheat_angle += c->cameraOffsetYawModifier;
            }
        }

        if (c->camera_anticheat_offset_x < -50) {
            c->cameraOffsetXModifier = 2;
        }
        if (c->camera_anticheat_offset_x > 50) {
            c->cameraOffsetXModifier = -2;
        }
        if (c->camera_anticheat_offset_z < -55) {
            c->cameraOffsetZModifier = 2;
        }
        if (c->camera_anticheat_offset_z > 55) {
            c->cameraOffsetZModifier = -2;
        }
        if (c->camera_anticheat_angle < -40) {
            c->cameraOffsetYawModifier = 1;
        }
        if (c->camera_anticheat_angle > 40) {
            c->cameraOffsetYawModifier = -1;
        }

        c->cameraOffsetCycle++;
        if (c->cameraOffsetCycle > 500) {
            c->cameraOffsetCycle = 0;
            int random = (int)(jrand() * 8.0);
            if ((random & 0x1) == 1) {
                c->minimap_anticheat_angle += c->minimapAngleModifier;
            }
            if ((random & 0x2) == 2) {
                c->minimap_zoom += c->minimapZoomModifier;
            }
        }

        if (c->minimap_anticheat_angle < -60) {
            c->minimapAngleModifier = 2;
        }
        if (c->minimap_anticheat_angle > 60) {
            c->minimapAngleModifier = -2;
        }

        if (c->minimap_zoom < -20) {
            c->minimapZoomModifier = 1;
        }
        if (c->minimap_zoom > 10) {
            c->minimapZoomModifier = -1;
        }

        _Client.cyclelogic4++;
        if (_Client.cyclelogic4 > 110) {
            _Client.cyclelogic4 = 0;
            // ANTICHEAT_CYCLELOGIC4
            p1isaac(c->out, 236);
            p4(c->out, 0);
        }

        c->heartbeatTimer++;
        if (c->heartbeatTimer > 50) {
            // NO_TIMEOUT
            p1isaac(c->out, 108);
        }

        // try {
        if (c->stream && c->out->pos > 0) {
            clientstream_write(c->stream, c->out->data, c->out->pos, 0);
            c->out->pos = 0;
            c->heartbeatTimer = 0;
        }
        // NOTE: no catch for logout or reconn
        // } catch (IOException ignored) {
        // client_try_reconnect(c);
        // } catch (Exception ignored) {
        // client_logout(c);
        // }
    }
}

bool client_read(Client *c) {
    if (!c->stream) {
        return false;
    }

    // try {
    if (!clientstream_available(c->stream, 1)) {
        return false;
    }

    if (c->packet_type == -1) {
        clientstream_read_bytes(c->stream, c->in->data, 0, 1);
        c->packet_type = c->in->data[0] & 0xff;
        // if (c->random_in) {
        c->packet_type = (c->packet_type - isaac_next(&c->random_in)) & 0xff;
        // }
        c->packet_size = _Protocol.SERVERPROT_SIZES[c->packet_type];
    }

    if (c->packet_size == -1) {
        if (!clientstream_available(c->stream, 1)) {
            return false;
        }

        clientstream_read_bytes(c->stream, c->in->data, 0, 1);
        c->packet_size = c->in->data[0] & 0xff;
    }

    if (c->packet_size == -2) {
        if (!clientstream_available(c->stream, 2)) {
            return false;
        }

        clientstream_read_bytes(c->stream, c->in->data, 0, 2);
        c->in->pos = 0;
        c->packet_size = g2(c->in);
    }

    if (!clientstream_available(c->stream, c->packet_size)) {
        return false;
    }
    c->in->pos = 0;
    clientstream_read_bytes(c->stream, c->in->data, 0, c->packet_size);
    c->idle_net_cycles = 0;
    c->last_packet_type2 = c->last_packet_type1;
    c->last_packet_type1 = c->last_packet_type0;
    c->last_packet_type0 = c->packet_type;

    if (c->packet_type == 150) {
        // VARP_SMALL
        int varp = g2(c->in);
        int8_t value = g1b(c->in);
        c->varCache[varp] = value;
        if (c->varps[varp] != value) {
            c->varps[varp] = value;
            updateVarp(c, varp);
            c->redraw_sidebar = true;
            if (c->sticky_chat_interface_id != -1) {
                c->redraw_chatback = true;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 152) {
        // UPDATE_FRIENDLIST
        int64_t username = g8(c->in);
        int world = g1(c->in);
        char *display_name = jstring_format_name(jstring_from_base37(username));
        for (int i = 0; i < c->friend_count; i++) {
            if (username == c->friendName37[i]) {
                if (c->friendWorld[i] != world) {
                    c->friendWorld[i] = world;
                    c->redraw_sidebar = true;
                    char buf[MAX_STR];
                    if (world > 0) {
                        sprintf(buf, "%s has logged in.", display_name);
                        client_add_message(c, 5, buf, "");
                    }
                    if (world == 0) {
                        sprintf(buf, "%s has logged out.", display_name);
                        client_add_message(c, 5, buf, "");
                    }
                }
                display_name = NULL;
                break;
            }
        }
        if (display_name && c->friend_count < 100) {
            c->friendName37[c->friend_count] = username;
            c->friendName[c->friend_count] = display_name;
            c->friendWorld[c->friend_count] = world;
            c->friend_count++;
            c->redraw_sidebar = true;
        }
        bool sorted = false;
        while (!sorted) {
            sorted = true;
            for (int i = 0; i < c->friend_count - 1; i++) {
                if ((c->friendWorld[i] != _Client.nodeid && c->friendWorld[i + 1] == _Client.nodeid) || (c->friendWorld[i] == 0 && c->friendWorld[i + 1] != 0)) {
                    int oldWorld = c->friendWorld[i];
                    c->friendWorld[i] = c->friendWorld[i + 1];
                    c->friendWorld[i + 1] = oldWorld;

                    char *oldName = c->friendName[i];
                    c->friendName[i] = c->friendName[i + 1];
                    c->friendName[i + 1] = oldName;

                    int64_t oldName37 = c->friendName37[i];
                    c->friendName37[i] = c->friendName37[i + 1];
                    c->friendName37[i + 1] = oldName37;
                    c->redraw_sidebar = true;
                    sorted = false;
                }
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 43) {
        // UPDATE_REBOOT_TIMER
        c->system_update_timer = g2(c->in) * 30;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 80) {
        // DATA_LAND_DONE
        int x = g1(c->in);
        int z = g1(c->in);
        int index = -1;
        for (int i = 0; i < c->sceneMapIndexLength; i++) {
            if (c->sceneMapIndex[i] == (x << 8) + z) {
                index = i;
            }
        }
        if (index != -1) {
#if defined(__EMSCRIPTEN__) && (!defined(SDL) || SDL == 0)
            // TODO use indexeddb instead of emscripten memfs
            char filename[PATH_MAX];
            sprintf(filename, "m%d_%d", x, z);
            FILE *file = fopen(filename, "wb");
            fwrite(c->sceneMapLandData[index], 1, c->sceneMapLandDataIndexLength[index], file);
            fclose(file);
#endif
            // signlink.cachesave("m" + x + "_" + z, this.sceneMapLandData[index]);
            c->scene_state = 1;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 1) {
        // NPC_INFO
        getNpcPos(c, c->in, c->packet_size);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 237) {
        // REBUILD_NORMAL
        int zoneX = g2(c->in);
        int zoneZ = g2(c->in);
        if (c->sceneCenterZoneX == zoneX && c->sceneCenterZoneZ == zoneZ && c->scene_state != 0) {
            c->packet_type = -1;
            return true;
        }
        c->sceneCenterZoneX = zoneX;
        c->sceneCenterZoneZ = zoneZ;
        c->sceneBaseTileX = (c->sceneCenterZoneX - 6) * 8;
        c->sceneBaseTileZ = (c->sceneCenterZoneZ - 6) * 8;
        c->scene_state = 1;
        pixmap_bind(c->area_viewport);
        drawStringCenter(c->font_plain12, 257, 151, "Loading - please wait.", BLACK);
        drawStringCenter(c->font_plain12, 256, 150, "Loading - please wait.", WHITE);
        pixmap_draw(c->area_viewport, 8, 11);
        // signlink.looprate(5);
        int regions = (c->packet_size - 2) / 10;

        client_scenemap_free(c);
        c->sceneMapLandData = calloc(regions, sizeof(int8_t *));
        c->sceneMapLocData = calloc(regions, sizeof(int8_t *));
        c->sceneMapIndex = calloc(regions, sizeof(int));
        c->sceneMapLandDataIndexLength = calloc(regions, sizeof(int));
        c->sceneMapLocDataIndexLength = calloc(regions, sizeof(int));
        c->sceneMapIndexLength = regions;
        // REBUILD_GETMAPS
        p1isaac(c->out, 150);
        p1(c->out, 0);
        int mapCount = 0;
        for (int i = 0; i < regions; i++) {
            int mapsquareX = g1(c->in);
            int mapsquareZ = g1(c->in);
            int landCrc = g4(c->in);
            int locCrc = g4(c->in);
            c->sceneMapIndex[i] = (mapsquareX << 8) + mapsquareZ;
            int8_t *data = NULL;
            size_t size = 0;
            if (landCrc != 0) {
                // data = signlink.cacheload("m" + mapsquareX + "_" + mapsquareZ);
                // custom NOTE move these
                char filename[PATH_MAX];
#ifdef _arch_dreamcast
                snprintf(filename, sizeof(filename), "cache/client/maps/m%d_%d.", mapsquareX, mapsquareZ);
#elif defined(NXDK)
                snprintf(filename, sizeof(filename), "D:\\cache\\client\\maps\\m%d_%d", mapsquareX, mapsquareZ);
#elif defined(__EMSCRIPTEN__) && (!defined(SDL) || SDL == 0)
                snprintf(filename, sizeof(filename), "m%d_%d", mapsquareX, mapsquareZ);
#else
                snprintf(filename, sizeof(filename), "cache/client/maps/m%d_%d", mapsquareX, mapsquareZ);
#endif

#if ANDROID
                SDL_RWops *file = SDL_RWFromFile(filename, "rb");
#else
                FILE *file = fopen(filename, "rb");
#endif
                if (!file) {
                    // rs2_error("%s: %s\n", filename, strerror(errno));
                } else {
#ifdef ANDROID
                    size_t size = SDL_RWseek(file, 0, RW_SEEK_END);
                    SDL_RWseek(file, 0, RW_SEEK_SET);
#else
                    fseek(file, 0, SEEK_END);
                    size = ftell(file);
                    fseek(file, 0, SEEK_SET);
#endif

                    data = malloc(size);
#ifdef ANDROID
                    if (SDL_RWread(file, data, 1, size) != size) {
#else
                    if (fread(data, 1, size, file) != size) {
#endif
                        rs2_error("Failed to read file: %s\n", strerror(errno));
                    }
#ifdef ANDROID
                    SDL_RWclose(file);
#else
                    fclose(file);
#endif
                }

                if (data) {
                    if (rs_crc32(data, size) != landCrc) {
                        // NOTE: requires manual update since it doesn't dl/save
                        // rs2_error("mapdata CRC check failed\n");
                        free(data);
                        data = NULL;
                    }
                }
                if (!data) {
                    c->scene_state = 0;
                    p1(c->out, 0);
                    p1(c->out, mapsquareX);
                    p1(c->out, mapsquareZ);
                    mapCount += 3;
                } else {
                    c->sceneMapLandDataIndexLength[i] = (int)size;
                    c->sceneMapLandData[i] = data;
                }
            }
            if (locCrc != 0) {
                // data = signlink.cacheload("l" + mapsquareX + "_" + mapsquareZ);
                // custom NOTE move this
                char filename[PATH_MAX];
#ifdef _arch_dreamcast
                snprintf(filename, sizeof(filename), "cache/client/maps/l%d_%d.", mapsquareX, mapsquareZ);
#elif defined(NXDK)
                snprintf(filename, sizeof(filename), "D:\\cache\\client\\maps\\l%d_%d", mapsquareX, mapsquareZ);
#elif defined(__EMSCRIPTEN__) && (!defined(SDL) || SDL == 0)
            snprintf(filename, sizeof(filename), "l%d_%d", mapsquareX, mapsquareZ);
#else
            snprintf(filename, sizeof(filename), "cache/client/maps/l%d_%d", mapsquareX, mapsquareZ);
#endif

#if ANDROID
                SDL_RWops *file = SDL_RWFromFile(filename, "rb");
#else
                FILE *file = fopen(filename, "rb");
#endif
                if (!file) {
                    // rs2_error("%s: %s\n", filename, strerror(errno));
                } else {
#ifdef ANDROID
                    size_t size = SDL_RWseek(file, 0, RW_SEEK_END);
                    SDL_RWseek(file, 0, RW_SEEK_SET);
#else
                    fseek(file, 0, SEEK_END);
                    size = ftell(file);
                    fseek(file, 0, SEEK_SET);
#endif

                    data = malloc(size);
#ifdef ANDROID
                    if (SDL_RWread(file, data, 1, size) != size) {
#else
                    if (fread(data, 1, size, file) != size) {
#endif
                        rs2_error("Failed to read file: %s\n", strerror(errno));
                    }
#ifdef ANDROID
                    SDL_RWclose(file);
#else
                    fclose(file);
#endif
                }

                if (data) {
                    if (rs_crc32(data, size) != locCrc) {
                        // NOTE: requires manual update since it doesn't dl/save
                        // rs2_error("mapdata CRC check failed\n");
                        free(data);
                        data = NULL;
                    }
                }
                if (!data) {
                    c->scene_state = 0;
                    p1(c->out, 1);
                    p1(c->out, mapsquareX);
                    p1(c->out, mapsquareZ);
                    mapCount += 3;
                } else {
                    c->sceneMapLocDataIndexLength[i] = (int)size;
                    c->sceneMapLocData[i] = data;
                }
            }
        }
        psize1(c->out, mapCount);
        // signlink.looprate(50);
        pixmap_bind(c->area_viewport);
        if (c->scene_state == 0) {
            drawStringCenter(c->font_plain12, 257, 166, "Map area updated since last visit, so load will take longer this time only", BLACK);
            drawStringCenter(c->font_plain12, 256, 165, "Map area updated since last visit, so load will take longer this time only", WHITE);
        }
        pixmap_draw(c->area_viewport, 8, 11);
        int dx = c->sceneBaseTileX - c->mapLastBaseX;
        int dz = c->sceneBaseTileZ - c->mapLastBaseZ;
        c->mapLastBaseX = c->sceneBaseTileX;
        c->mapLastBaseZ = c->sceneBaseTileZ;
        for (int i = 0; i < MAX_NPC_COUNT; i++) {
            NpcEntity *npc = c->npcs[i];
            if (npc) {
                for (int j = 0; j < 10; j++) {
                    npc->pathing_entity.pathTileX[j] -= dx;
                    npc->pathing_entity.pathTileZ[j] -= dz;
                }
                npc->pathing_entity.x -= dx * 128;
                npc->pathing_entity.z -= dz * 128;
            }
        }
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            PlayerEntity *player = c->players[i];
            if (player) {
                for (int j = 0; j < 10; j++) {
                    player->pathing_entity.pathTileX[j] -= dx;
                    player->pathing_entity.pathTileZ[j] -= dz;
                }
                player->pathing_entity.x -= dx * 128;
                player->pathing_entity.z -= dz * 128;
            }
        }
        int8_t startTileX = 0;
        int8_t endTileX = 104;
        int8_t dirX = 1;
        if (dx < 0) {
            startTileX = 104 - 1;
            endTileX = -1;
            dirX = -1;
        }
        int8_t startTileZ = 0;
        int8_t endTileZ = 104;
        int8_t dirZ = 1;
        if (dz < 0) {
            startTileZ = 104 - 1;
            endTileZ = -1;
            dirZ = -1;
        }
        for (int x = startTileX; x != endTileX; x += dirX) {
            for (int z = startTileZ; z != endTileZ; z += dirZ) {
                int lastX = x + dx;
                int lastZ = z + dz;
                for (int level = 0; level < 4; level++) {
                    if (lastX >= 0 && lastZ >= 0 && lastX < 104 && lastZ < 104) {
                        c->level_obj_stacks[level][x][z] = c->level_obj_stacks[level][lastX][lastZ];
                    } else {
                        c->level_obj_stacks[level][x][z] = NULL;
                    }
                }
            }
        }
        for (LocAddEntity *loc = (LocAddEntity *)linklist_head(c->spawned_locations); loc; loc = (LocAddEntity *)linklist_next(c->spawned_locations)) {
            loc->x -= dx;
            loc->z -= dz;
            if (loc->x < 0 || loc->z < 0 || loc->x >= 104 || loc->z >= 104) {
                linkable_unlink(&loc->link);
                free(loc);
            }
        }
        if (c->flagSceneTileX != 0) {
            c->flagSceneTileX -= dx;
            c->flagSceneTileZ -= dz;
        }
        c->cutscene = false;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 197) {
        // IF_SETPLAYERHEAD
        int com = g2(c->in);
        _Component.instances[com]->model = playerentity_get_headmodel(c->local_player);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 25) {
        // HINT_ARROW
        c->hint_type = g1(c->in);
        if (c->hint_type == 1) {
            c->hint_npc = g2(c->in);
        }
        if (c->hint_type >= 2 && c->hint_type <= 6) {
            if (c->hint_type == 2) {
                c->hint_offset_x = 64;
                c->hint_offset_z = 64;
            }
            if (c->hint_type == 3) {
                c->hint_offset_x = 0;
                c->hint_offset_z = 64;
            }
            if (c->hint_type == 4) {
                c->hint_offset_x = 128;
                c->hint_offset_z = 64;
            }
            if (c->hint_type == 5) {
                c->hint_offset_x = 64;
                c->hint_offset_z = 0;
            }
            if (c->hint_type == 6) {
                c->hint_offset_x = 64;
                c->hint_offset_z = 128;
            }
            c->hint_type = 2;
            c->hint_tile_x = g2(c->in);
            c->hint_tile_z = g2(c->in);
            c->hint_height = g1(c->in);
        }
        if (c->hint_type == 10) {
            c->hint_player = g2(c->in);
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 54) {
        // MIDI_SONG
        char *name = gjstr(c->in);
        int crc = g4(c->in);
        int length = g4(c->in);
        if (strcmp(name, c->currentMidi) != 0 && c->midiActive && !_Client.lowmem) {
            platform_set_midi(name, crc, length);
        }
        strcpy(c->currentMidi, name);
        free(name);
        c->midiCrc = crc;
        c->midiSize = length;
        c->nextMusicDelay = 0;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 142) {
        // LOGOUT
        client_logout(c);
        c->packet_type = -1;
        return false;
    }
    if (c->packet_type == 20) {
        // DATA_LOC_DONE
        int x = g1(c->in);
        int z = g1(c->in);
        int index = -1;
        for (int i = 0; i < c->sceneMapIndexLength; i++) {
            if (c->sceneMapIndex[i] == (x << 8) + z) {
                index = i;
            }
        }
        if (index != -1) {
#if defined(__EMSCRIPTEN__) && (!defined(SDL) || SDL == 0)
            // TODO use indexeddb instead of emscripten memfs
            char filename[PATH_MAX];
            sprintf(filename, "l%d_%d", x, z);
            FILE *file = fopen(filename, "wb");
            fwrite(c->sceneMapLocData[index], 1, c->sceneMapLocDataIndexLength[index], file);
            fclose(file);
#endif
            // signlink.cachesave("l" + x + "_" + z, c->sceneMapLocData[index]);
            c->scene_state = 1;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 19) {
        // UNSET_MAP_FLAG
        c->flagSceneTileX = 0;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 139) {
        // UPDATE_UID192
        c->local_pid = g2(c->in);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 151 || c->packet_type == 23 || c->packet_type == 50 || c->packet_type == 191 || c->packet_type == 69 || c->packet_type == 49 || c->packet_type == 223 || c->packet_type == 42 || c->packet_type == 76 || c->packet_type == 59) {
        // Zone Protocol
        readZonePacket(c, c->in, c->packet_type);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 28) {
        // IF_OPENMAINSIDEMODAL
        int main = g2(c->in);
        int side = g2(c->in);
        if (c->chat_interface_id != -1) {
            c->chat_interface_id = -1;
            c->redraw_chatback = true;
        }
        if (c->chatback_input_open) {
            c->chatback_input_open = false;
            c->redraw_chatback = true;
        }
        c->viewport_interface_id = main;
        c->sidebar_interface_id = side;
        c->redraw_sidebar = true;
        c->redraw_sideicons = true;
        c->pressed_continue_option = false;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 175) {
        // VARP_LARGE
        int varp = g2(c->in);
        int value = g4(c->in);
        c->varCache[varp] = value;
        if (c->varps[varp] != value) {
            c->varps[varp] = value;
            updateVarp(c, varp);
            c->redraw_sidebar = true;
            if (c->sticky_chat_interface_id != -1) {
                c->redraw_chatback = true;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 146) {
        // IF_SETANIM
        int com = g2(c->in);
        int seqId = g2(c->in);
        _Component.instances[com]->anim = seqId;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 167) {
        // IF_OPENSIDEOVERLAY
        int com = g2(c->in);
        int tab = g1(c->in);
        if (com == 65535) {
            com = -1;
        }
        c->tab_interface_id[tab] = com;
        c->redraw_sidebar = true;
        c->redraw_sideicons = true;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 220) {
        // DATA_LOC
        int x = g1(c->in);
        int z = g1(c->in);
        int offset = g2(c->in);
        int length = g2(c->in);
        int index = -1;
        for (int i = 0; i < c->sceneMapIndexLength; i++) {
            if (c->sceneMapIndex[i] == (x << 8) + z) {
                index = i;
            }
        }
        if (index != -1) {
            if (!c->sceneMapLocData[index] || c->sceneMapLocDataIndexLength[index] != length) {
                c->sceneMapLocData[index] = calloc(length, sizeof(int8_t));
            }
            gdata(c->in, c->packet_size - 6, offset, c->sceneMapLocData[index]);
            c->sceneMapLocDataIndexLength[index] = length;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 133) {
        // FINISH_TRACKING
        Packet *tracking = inputtracking_stop(&_InputTracking);
        if (tracking) {
            // EVENT_TRACKING
            p1isaac(c->out, 81);
            p2(c->out, tracking->pos);
            pdata(c->out, tracking->data, tracking->pos, 0);
            packet_release(tracking);
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 98) {
        // UPDATE_INV_FULL
        c->redraw_sidebar = true;
        int com = g2(c->in);
        Component *inv = _Component.instances[com];
        int size = g1(c->in);
        for (int i = 0; i < size; i++) {
            inv->invSlotObjId[i] = g2(c->in);
            int count = g1(c->in);
            if (count == 255) {
                count = g4(c->in);
            }
            inv->invSlotObjCount[i] = count;
        }
        for (int i = size; i < inv->width * inv->height; i++) {
            inv->invSlotObjId[i] = 0;
            inv->invSlotObjCount[i] = 0;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 226) {
        // ENABLE_TRACKING
        inputtracking_set_enabled(&_InputTracking);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 243) {
        // P_COUNTDIALOG
        c->show_social_input = false;
        c->chatback_input_open = true;
        c->chatback_input[0] = '\0';
        c->redraw_chatback = true;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 15) {
        // UPDATE_INV_STOP_TRANSMIT
        int com = g2(c->in);
        Component *inv = _Component.instances[com];
        for (int i = 0; i < inv->width * inv->height; i++) {
            inv->invSlotObjId[i] = -1;
            inv->invSlotObjId[i] = 0;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 140) {
        // LAST_LOGIN_INFO
        c->lastAddress = g4(c->in);
        c->daysSinceLastLogin = g2(c->in);
        c->daysSinceRecoveriesChanged = g1(c->in);
        c->unreadMessages = g2(c->in);
        if (c->lastAddress != 0 && c->viewport_interface_id == -1) {
            if (_Custom.hide_dns) {
                _Client.dns = "unknown";
            } else {
                _Client.dns = dnslookup(jstring_format_ipv4(c->lastAddress));
            }
            closeInterfaces(c);
            int clientCode = 650;
            if (c->daysSinceRecoveriesChanged != 201) {
                clientCode = 655;
            }
            c->reportAbuseInput[0] = '\0';
            c->reportAbuseMuteOption = false;
            for (int i = 0; i < _Component.count; i++) {
                if (_Component.instances[i] && _Component.instances[i]->clientCode == clientCode) {
                    c->viewport_interface_id = _Component.instances[i]->layer;
                    break;
                }
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 126) {
        // TUTORIAL_FLASHSIDE
        c->flashing_tab = g1(c->in);
        if (c->flashing_tab == c->selected_tab) {
            if (c->flashing_tab == 3) {
                c->selected_tab = 1;
            } else {
                c->selected_tab = 3;
            }
            c->redraw_sidebar = true;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 212) {
        // MIDI_JINGLE
        if (c->midiActive && !_Client.lowmem) {
            int delay = g2(c->in);
            int length = g4(c->in);
            int remaining = c->packet_size - 6;
            int8_t *src = calloc(length, sizeof(int8_t));
            bzip_decompress(src, c->in->data, remaining, c->in->pos);
            platform_set_jingle(src, length);
            c->nextMusicDelay = delay;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 254) {
        // SET_MULTIWAY
        c->in_multizone = g1(c->in);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 12) {
        // SYNTH_SOUND
        int id = g2(c->in);
        int loop = g1(c->in);
        int delay = g2(c->in);
        if (c->wave_enabled && !_Client.lowmem && c->wave_count < 50) {
            c->wave_ids[c->wave_count] = id;
            c->wave_loops[c->wave_count] = loop;
            c->wave_delay[c->wave_count] = delay + _Wave.delays[id];
            c->wave_count++;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 204) {
        // IF_SETNPCHEAD
        int com = g2(c->in);
        int npcId = g2(c->in);
        NpcType *npc = npctype_get(npcId);
        _Component.instances[com]->model = npctype_get_headmodel(npc);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 7) {
        // UPDATE_ZONE_PARTIAL_FOLLOWS
        c->baseX = g1(c->in);
        c->baseZ = g1(c->in);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 103) {
        // IF_SETRECOL
        int com = g2(c->in);
        int src = g2(c->in);
        int dst = g2(c->in);
        Component *inter = _Component.instances[com];
        Model *model = inter->model;
        if (model) {
            model_recolor(model, src, dst);
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 32) {
        // CHAT_FILTER_SETTINGS
        c->public_chat_setting = g1(c->in);
        c->private_chat_setting = g1(c->in);
        c->trade_chat_setting = g1(c->in);
        c->redraw_privacy_settings = true;
        c->redraw_chatback = true;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 195) {
        // IF_OPENSIDEMODAL
        int com = g2(c->in);
        reset_interface_animation(com);
        if (c->chat_interface_id != -1) {
            c->chat_interface_id = -1;
            c->redraw_chatback = true;
        }
        if (c->chatback_input_open) {
            c->chatback_input_open = false;
            c->redraw_chatback = true;
        }
        c->sidebar_interface_id = com;
        c->redraw_sidebar = true;
        c->redraw_sideicons = true;
        c->viewport_interface_id = -1;
        c->pressed_continue_option = false;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 14) {
        // IF_OPENCHATMODAL
        int com = g2(c->in);
        reset_interface_animation(com);
        if (c->sidebar_interface_id != -1) {
            c->sidebar_interface_id = -1;
            c->redraw_sidebar = true;
            c->redraw_sideicons = true;
        }
        c->chat_interface_id = com;
        c->redraw_chatback = true;
        c->viewport_interface_id = -1;
        c->pressed_continue_option = false;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 209) {
        // IF_SETPOSITION
        int com = g2(c->in);
        int x = g2b(c->in);
        int z = g2b(c->in);
        Component *inter = _Component.instances[com];
        inter->x = x;
        inter->y = z;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 3) {
        // CAM_MOVETO
        c->cutscene = true;
        c->cutsceneSrcLocalTileX = g1(c->in);
        c->cutsceneSrcLocalTileZ = g1(c->in);
        c->cutsceneSrcHeight = g2(c->in);
        c->cutsceneMoveSpeed = g1(c->in);
        c->cutsceneMoveAcceleration = g1(c->in);
        if (c->cutsceneMoveAcceleration >= 100) {
            c->cameraX = c->cutsceneSrcLocalTileX * 128 + 64;
            c->cameraZ = c->cutsceneSrcLocalTileZ * 128 + 64;
            c->cameraY = getHeightmapY(c, c->currentLevel, c->cutsceneSrcLocalTileX, c->cutsceneSrcLocalTileZ) - c->cutsceneSrcHeight;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 135) {
        // UPDATE_ZONE_FULL_FOLLOWS
        c->baseX = g1(c->in);
        c->baseZ = g1(c->in);
        for (int x = c->baseX; x < c->baseX + 8; x++) {
            for (int z = c->baseZ; z < c->baseZ + 8; z++) {
                if (c->level_obj_stacks[c->currentLevel][x][z]) {
                    linklist_free(c->level_obj_stacks[c->currentLevel][x][z]);
                    c->level_obj_stacks[c->currentLevel][x][z] = NULL;
                }
                sortObjStacks(c, x, z);
            }
        }
        for (LocAddEntity *loc = (LocAddEntity *)linklist_head(c->spawned_locations); loc; loc = (LocAddEntity *)linklist_next(c->spawned_locations)) {
            if (loc->x >= c->baseX && loc->x < c->baseX + 8 && loc->z >= c->baseZ && loc->z < c->baseZ + 8 && loc->plane == c->currentLevel) {
                addLoc(c, loc->plane, loc->x, loc->z, loc->lastLocIndex, loc->lastAngle, loc->lastShape, loc->layer);
                linkable_unlink(&loc->link);
                free(loc);
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 132) {
        // DATA_LAND
        int x = g1(c->in);
        int z = g1(c->in);
        int offset = g2(c->in);
        int length = g2(c->in);
        int index = -1;
        for (int i = 0; i < c->sceneMapIndexLength; i++) {
            if (c->sceneMapIndex[i] == (x << 8) + z) {
                index = i;
            }
        }
        if (index != -1) {
            if (!c->sceneMapLandData[index] || c->sceneMapLandDataIndexLength[index] != length) {
                c->sceneMapLandData[index] = calloc(length, sizeof(int8_t));
            }
            gdata(c->in, c->packet_size - 6, offset, c->sceneMapLandData[index]);
            c->sceneMapLandDataIndexLength[index] = length;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 41) {
        // MESSAGE_PRIVATE
        int64_t from = g8(c->in);
        int messageId = g4(c->in);
        int staffModLevel = g1(c->in);
        bool ignored = false;
        for (int i = 0; i < 100; i++) {
            if (c->messageIds[i] == messageId) {
                ignored = true;
                break;
            }
        }
        if (staffModLevel <= 1) {
            for (int i = 0; i < c->ignoreCount; i++) {
                if (c->ignoreName37[i] == from) {
                    ignored = true;
                    break;
                }
            }
        }
        if (!ignored && c->overrideChat == 0) {
            // try {
            c->messageIds[c->privateMessageCount] = messageId;
            c->privateMessageCount = (c->privateMessageCount + 1) % 100;
            char *uncompressed = wordpack_unpack(c->in, c->packet_size - 13);
            char *filtered = uncompressed; // TODO remove
            // TODO wordfilter
            // char* filtered = wordfilter_filter(uncompressed);
            if (staffModLevel > 1) {
                client_add_message(c, 7, filtered, jstring_format_name(jstring_from_base37(from)));
            } else {
                client_add_message(c, 3, filtered, jstring_format_name(jstring_from_base37(from)));
            }
            // } catch (@Pc(2752) Exception ex) {
            // 	signlink.reporterror("cde1");
            // }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 193) {
        // RESET_CLIENT_VARCACHE
        for (int i = 0; i < VARPS_COUNT; i++) {
            if (c->varps[i] != c->varCache[i]) {
                c->varps[i] = c->varCache[i];
                updateVarp(c, i);
                c->redraw_sidebar = true;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 87) {
        // IF_SETMODEL
        int com = g2(c->in);
        int model = g2(c->in);
        _Component.instances[com]->model = model_from_id(model, false);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 185) {
        // TUTORIAL_OPENCHAT
        int com = g2b(c->in);
        c->sticky_chat_interface_id = com;
        c->redraw_chatback = true;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 68) {
        // UPDATE_RUNENERGY
        if (c->selected_tab == 12) {
            c->redraw_sidebar = true;
        }
        c->energy = g1(c->in);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 74) {
        // CAM_LOOKAT
        c->cutscene = true;
        c->cutsceneDstLocalTileX = g1(c->in);
        c->cutsceneDstLocalTileZ = g1(c->in);
        c->cutsceneDstHeight = g2(c->in);
        c->cutsceneRotateSpeed = g1(c->in);
        c->cutsceneRotateAcceleration = g1(c->in);
        if (c->cutsceneRotateAcceleration >= 100) {
            int sceneX = c->cutsceneDstLocalTileX * 128 + 64;
            int sceneZ = c->cutsceneDstLocalTileZ * 128 + 64;
            int sceneY = getHeightmapY(c, c->currentLevel, c->cutsceneDstLocalTileX, c->cutsceneDstLocalTileZ) - c->cutsceneDstHeight;
            int deltaX = sceneX - c->cameraX;
            int deltaY = sceneY - c->cameraY;
            int deltaZ = sceneZ - c->cameraZ;
            int distance = (int)sqrt(deltaX * deltaX + deltaZ * deltaZ);
            c->cameraPitch = (int)(atan2(deltaY, distance) * 325.949) & 0x7ff;
            c->cameraYaw = (int)(atan2(deltaX, deltaZ) * -325.949) & 0x7ff;
            if (c->cameraPitch < 128) {
                c->cameraPitch = 128;
            }
            if (c->cameraPitch > 383) {
                c->cameraPitch = 383;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 84) {
        // IF_SHOWSIDE
        c->selected_tab = g1(c->in);
        c->redraw_sidebar = true;
        c->redraw_sideicons = true;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 4) {
        // MESSAGE_GAME
        char *message = gjstr(c->in);
        int64_t username;
        if (strendswith(message, ":tradereq:")) {
            char *player = substring(message, 0, indexof(message, ":"));
            username = jstring_to_base37(player);
            bool ignored = false;
            for (int i = 0; i < c->ignoreCount; i++) {
                if (c->ignoreName37[i] == username) {
                    ignored = true;
                    break;
                }
            }
            if (!ignored && c->overrideChat == 0) {
                client_add_message(c, 4, "wishes to trade with you.", player);
            }
        } else if (strendswith(message, ":duelreq:")) {
            char *player = substring(message, 0, indexof(message, ":"));
            username = jstring_to_base37(player);
            bool ignored = false;
            for (int i = 0; i < c->ignoreCount; i++) {
                if (c->ignoreName37[i] == username) {
                    ignored = true;
                    break;
                }
            }
            if (!ignored && c->overrideChat == 0) {
                client_add_message(c, 8, "wishes to duel with you.", player);
            }
        } else {
            client_add_message(c, 0, message, "");
        }
        free(message);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 46) {
        // IF_SETOBJECT
        int com = g2(c->in);
        int objId = g2(c->in);
        int zoom = g2(c->in);
        ObjType *obj = objtype_get(objId);
        _Component.instances[com]->model = objtype_get_interfacemodel(obj, 50, false);
        _Component.instances[com]->xan = obj->xan2d;
        _Component.instances[com]->yan = obj->yan2d;
        _Component.instances[com]->zoom = obj->zoom2d * 100 / zoom;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 168) {
        // IF_OPENMAINMODAL
        int com = g2(c->in);
        reset_interface_animation(com);
        if (c->sidebar_interface_id != -1) {
            c->sidebar_interface_id = -1;
            c->redraw_sidebar = true;
            c->redraw_sideicons = true;
        }
        if (c->chat_interface_id != -1) {
            c->chat_interface_id = -1;
            c->redraw_chatback = true;
        }
        if (c->chatback_input_open) {
            c->chatback_input_open = false;
            c->redraw_chatback = true;
        }
        c->viewport_interface_id = com;
        c->pressed_continue_option = false;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 2) {
        // IF_SETCOLOUR
        int com = g2(c->in);
        int color = g2(c->in);
        int r = color >> 10 & 0x1f;
        int g = color >> 5 & 0x1f;
        int b = color & 0x1f;
        _Component.instances[com]->colour = (r << 19) + (g << 11) + (b << 3);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 136) {
        // RESET_ANIMS
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            if (c->players[i]) {
                c->players[i]->pathing_entity.primarySeqId = -1;
            }
        }
        for (int i = 0; i < MAX_NPC_COUNT; i++) {
            if (c->npcs[i]) {
                c->npcs[i]->pathing_entity.primarySeqId = -1;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 26) {
        // IF_SETHIDE
        int com = g2(c->in);
        bool hide = g1(c->in) == 1;
        _Component.instances[com]->hide = hide;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 21) {
        // UPDATE_IGNORELIST
        c->ignoreCount = c->packet_size / 8;
        for (int i = 0; i < c->ignoreCount; i++) {
            c->ignoreName37[i] = g8(c->in);
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 239) {
        // CAM_RESET
        c->cutscene = false;
        for (int i = 0; i < 5; i++) {
            c->cameraModifierEnabled[i] = false;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 129) {
        // IF_CLOSE
        if (c->sidebar_interface_id != -1) {
            c->sidebar_interface_id = -1;
            c->redraw_sidebar = true;
            c->redraw_sideicons = true;
        }
        if (c->chat_interface_id != -1) {
            c->chat_interface_id = -1;
            c->redraw_chatback = true;
        }
        if (c->chatback_input_open) {
            c->chatback_input_open = false;
            c->redraw_chatback = true;
        }
        c->viewport_interface_id = -1;
        c->pressed_continue_option = false;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 201) {
        // IF_SETTEXT
        int com = g2(c->in);
        char *text = gjstr(c->in);
        strcpy(_Component.instances[com]->text, text);
        free(text);
        if (_Component.instances[com]->layer == c->tab_interface_id[c->selected_tab]) {
            c->redraw_sidebar = true;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 44) {
        // UPDATE_STAT
        c->redraw_sidebar = true;
        int stat = g1(c->in);
        int xp = g4(c->in);
        int level = g1(c->in);
        c->skillExperience[stat] = xp;
        c->skillLevel[stat] = level;
        c->skillBaseLevel[stat] = 1;
        for (int i = 0; i < 98; i++) {
            if (xp >= _Client.levelExperience[i]) {
                c->skillBaseLevel[stat] = i + 2;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 162) {
        // UPDATE_ZONE_PARTIAL_ENCLOSED
        c->baseX = g1(c->in);
        c->baseZ = g1(c->in);
        while (c->in->pos < c->packet_size) {
            int opcode = g1(c->in);
            readZonePacket(c, c->in, opcode);
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 22) {
        // UPDATE_RUNWEIGHT
        if (c->selected_tab == 12) {
            c->redraw_sidebar = true;
        }
        c->weightCarried = g2b(c->in);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 13) {
        // CAM_SHAKE
        int type = g1(c->in);
        int jitter = g1(c->in);
        int wobbleScale = g1(c->in);
        int wobbleSpeed = g1(c->in);
        c->cameraModifierEnabled[type] = true;
        c->cameraModifierJitter[type] = jitter;
        c->cameraModifierWobbleScale[type] = wobbleScale;
        c->cameraModifierWobbleSpeed[type] = wobbleSpeed;
        c->cameraModifierCycle[type] = 0;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 213) {
        // UPDATE_INV_PARTIAL
        c->redraw_sidebar = true;
        int com = g2(c->in);
        Component *inv = _Component.instances[com];
        while (c->in->pos < c->packet_size) {
            int slot = g1(c->in);
            int id = g2(c->in);
            int count = g1(c->in);
            if (count == 255) {
                count = g4(c->in);
            }
            if (slot >= 0 && slot < inv->width * inv->height) {
                inv->invSlotObjId[slot] = id;
                inv->invSlotObjCount[slot] = count;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 184) {
        // PLAYER_INFO
        getPlayer(c, c->in, c->packet_size);
        if (c->scene_state == 1) {
            c->scene_state = 2;
            _World.levelBuilt = c->currentLevel;
            client_build_scene(c);
        }
        if (_Client.lowmem && c->scene_state == 2 && _World.levelBuilt != c->currentLevel) {
            pixmap_bind(c->area_viewport);
            drawStringCenter(c->font_plain12, 257, 151, "Loading - please wait.", BLACK);
            drawStringCenter(c->font_plain12, 256, 150, "Loading - please wait.", WHITE);
            pixmap_draw(c->area_viewport, 8, 11);
            _World.levelBuilt = c->currentLevel;
            client_build_scene(c);
        }
        if (c->currentLevel != c->minimap_level && c->scene_state == 2) {
            c->minimap_level = c->currentLevel;
            createMinimap(c, c->currentLevel);
        }
        c->packet_type = -1;
        return true;
    }
    rs2_error("T1 - %i,%i - %i,%i\n", c->packet_type, c->packet_size, c->last_packet_type1, c->last_packet_type2);
    // signlink.reporterror("T1 - " + c->packet_type + "," + c->packetSize + " - " + c->lastPacketType1 + "," + c->lastPacketType2);
    client_logout(c);
    // } catch (@Pc(3862) IOException ex) {
    // client_try_reconnect(c);
    // } catch (@Pc(3867) Exception ex) {
    // ex.printStackTrace();
    // const char* error = "T2 - " + c->packet_type + "," + c->lastPacketType1 + "," + c->lastPacketType2 + " - " + c->packetSize + "," + (c->sceneBaseTileX + c->localPlayer.pathTileX[0]) + "," + (c->sceneBaseTileZ + c->localPlayer.pathTileZ[0]) + " - ";
    // for (int i = 0; i < c->packet_size && i < 50; i++) {
    // 	error = error + c->in->data[i] + ",";
    // }
    // signlink.reporterror(error);
    // client_logout(c);
    // }
    return true;
}

void getPlayerLocal(Client *c, Packet *buf, int size) {
    (void)size;
    access_bits(buf);

    int info = gbit(buf, 1);
    if (info != 0) {
        int op = gbit(buf, 2);

        if (op == 0) {
            c->entityUpdateIds[c->entityUpdateCount++] = LOCAL_PLAYER_INDEX;
        } else if (op == 1) {
            int walkDir = gbit(buf, 3);
            pathingentity_movealongroute(&c->local_player->pathing_entity, false, walkDir);

            int extendedInfo = gbit(buf, 1);
            if (extendedInfo == 1) {
                c->entityUpdateIds[c->entityUpdateCount++] = LOCAL_PLAYER_INDEX;
            }
        } else if (op == 2) {
            int walkDir = gbit(buf, 3);
            pathingentity_movealongroute(&c->local_player->pathing_entity, true, walkDir);
            int runDir = gbit(buf, 3);
            pathingentity_movealongroute(&c->local_player->pathing_entity, true, runDir);

            int extendedInfo = gbit(buf, 1);
            if (extendedInfo == 1) {
                c->entityUpdateIds[c->entityUpdateCount++] = LOCAL_PLAYER_INDEX;
            }
        } else if (op == 3) {
            c->currentLevel = gbit(buf, 2);
            // TODO:
            // if (c->showDebug) {
            // 	c->userTileMarkers = ground_new(4);
            // 	c->userTileMarkerIndex = 0;
            // }
            int localX = gbit(buf, 7);
            int localZ = gbit(buf, 7);
            int jump = gbit(buf, 1);
            pathingentity_teleport(&c->local_player->pathing_entity, jump == 1, localX, localZ);

            int extendedInfo = gbit(buf, 1);
            if (extendedInfo == 1) {
                c->entityUpdateIds[c->entityUpdateCount++] = LOCAL_PLAYER_INDEX;
            }
        }
    }
}

void getPlayerOldVis(Client *c, Packet *buf, int size) {
    (void)size;
    int count = gbit(buf, 8);

    if (count < c->player_count) {
        for (int i = count; i < c->player_count; i++) {
            c->entityRemovalIds[c->entityRemovalCount++] = c->player_ids[i];
        }
    }

    if (count > c->player_count) {
        rs2_error("%s Too many players\n", c->username);
        // signlink.reporterror(c->username + " Too many players");
        // throw new RuntimeException("eek");
    }

    c->player_count = 0;
    for (int i = 0; i < count; i++) {
        int index = c->player_ids[i];
        PlayerEntity *player = c->players[index];

        int info = gbit(buf, 1);
        if (info == 0) {
            c->player_ids[c->player_count++] = index;
            player->pathing_entity.cycle = _Client.loop_cycle;
        } else {
            int op = gbit(buf, 2);

            if (op == 0) {
                c->player_ids[c->player_count++] = index;
                player->pathing_entity.cycle = _Client.loop_cycle;
                c->entityUpdateIds[c->entityUpdateCount++] = index;
            } else if (op == 1) {
                c->player_ids[c->player_count++] = index;
                player->pathing_entity.cycle = _Client.loop_cycle;

                int walkDir = gbit(buf, 3);
                pathingentity_movealongroute(&player->pathing_entity, false, walkDir);

                int extendedInfo = gbit(buf, 1);
                if (extendedInfo == 1) {
                    c->entityUpdateIds[c->entityUpdateCount++] = index;
                }
            } else if (op == 2) {
                c->player_ids[c->player_count++] = index;
                player->pathing_entity.cycle = _Client.loop_cycle;

                int walkDir = gbit(buf, 3);
                pathingentity_movealongroute(&player->pathing_entity, true, walkDir);
                int runDir = gbit(buf, 3);
                pathingentity_movealongroute(&player->pathing_entity, true, runDir);

                int extendedInfo = gbit(buf, 1);
                if (extendedInfo == 1) {
                    c->entityUpdateIds[c->entityUpdateCount++] = index;
                }
            } else if (op == 3) {
                c->entityRemovalIds[c->entityRemovalCount++] = index;
            }
        }
    }
}

void getPlayerNewVis(Client *c, int size, Packet *buf) {
    int index;
    while (buf->bit_pos + 10 < size * 8) {
        index = gbit(buf, 11);
        if (index == 2047) {
            break;
        }

        if (!c->players[index]) {
            c->players[index] = playerentity_new();
            if (c->player_appearance_buffer[index]) {
                playerentity_read(c->players[index], c->player_appearance_buffer[index]);
            }
        }

        c->player_ids[c->player_count++] = index;
        PlayerEntity *player = c->players[index];
        player->pathing_entity.cycle = _Client.loop_cycle;
        int dx = gbit(buf, 5);
        if (dx > 15) {
            dx -= 32;
        }
        int dz = gbit(buf, 5);
        if (dz > 15) {
            dz -= 32;
        }
        int jump = gbit(buf, 1);
        pathingentity_teleport(&player->pathing_entity, jump == 1, c->local_player->pathing_entity.pathTileX[0] + dx, c->local_player->pathing_entity.pathTileZ[0] + dz);

        int extendedInfo = gbit(buf, 1);
        if (extendedInfo == 1) {
            c->entityUpdateIds[c->entityUpdateCount++] = index;
        }
    }

    access_bytes(buf);
}

void getPlayerExtended(Client *c, Packet *buf, int size) {
    (void)size;
    for (int i = 0; i < c->entityUpdateCount; i++) {
        int index = c->entityUpdateIds[i];
        PlayerEntity *player = c->players[index];
        int mask = g1(buf);
        if ((mask & 0x80) == 128) {
            mask += g1(buf) << 8;
        }
        getPlayerExtended2(c, player, index, mask, buf);
    }
}

void getPlayerExtended2(Client *c, PlayerEntity *player, int index, int mask, Packet *buf) {
    player->pathing_entity.lastMask = mask;
    player->pathing_entity.lastMaskCycle = _Client.loop_cycle;

    if ((mask & 0x1) == 1) {
        int length = g1(buf);
        int8_t *data = calloc(length, sizeof(int8_t));
        Packet *appearance = packet_new(data, length);
        gdata(buf, length, 0, data);

        if (c->player_appearance_buffer[index]) {
            packet_free(c->player_appearance_buffer[index]);
        }
        c->player_appearance_buffer[index] = appearance;
        playerentity_read(player, appearance);
    }
    if ((mask & 0x2) == 2) {
        int seqId = g2(buf);
        if (seqId == 65535) {
            seqId = -1;
        }
        if (seqId == player->pathing_entity.primarySeqId) {
            player->pathing_entity.primarySeqLoop = 0;
        }
        int delay = g1(buf);
        if (seqId == -1 || player->pathing_entity.primarySeqId == -1 || _SeqType.instances[seqId]->priority > _SeqType.instances[player->pathing_entity.primarySeqId]->priority || _SeqType.instances[player->pathing_entity.primarySeqId]->priority == 0) {
            player->pathing_entity.primarySeqId = seqId;
            player->pathing_entity.primarySeqFrame = 0;
            player->pathing_entity.primarySeqCycle = 0;
            player->pathing_entity.primarySeqDelay = delay;
            player->pathing_entity.primarySeqLoop = 0;
        }
    }
    if ((mask & 0x4) == 4) {
        player->pathing_entity.targetId = g2(buf);
        if (player->pathing_entity.targetId == 65535) {
            player->pathing_entity.targetId = -1;
        }
    }
    if ((mask & 0x8) == 8) {
        strcpy(player->pathing_entity.chat, gjstr(buf));
        player->pathing_entity.chatColor = 0;
        player->pathing_entity.chatStyle = 0;
        player->pathing_entity.chatTimer = 150;
        client_add_message(c, 2, player->pathing_entity.chat, player->name);
    }
    if ((mask & 0x10) == 16) {
        player->pathing_entity.damage = g1(buf);
        player->pathing_entity.damageType = g1(buf);
        player->pathing_entity.combatCycle = _Client.loop_cycle + 400;
        player->pathing_entity.health = g1(buf);
        player->pathing_entity.totalHealth = g1(buf);
    }
    if ((mask & 0x20) == 32) {
        player->pathing_entity.targetTileX = g2(buf);
        player->pathing_entity.targetTileZ = g2(buf);
        player->pathing_entity.lastFaceX = player->pathing_entity.targetTileX;
        player->pathing_entity.lastFaceZ = player->pathing_entity.targetTileZ;
    }
    if ((mask & 0x40) == 64) {
        int colorEffect = g2(buf);
        int type = g1(buf);
        int length = g1(buf);
        int start = buf->pos;
        if (player->name[0]) {
            int64_t username = jstring_to_base37(player->name);
            bool ignored = false;
            if (type <= 1) {
                for (int i = 0; i < c->ignoreCount; i++) {
                    if (c->ignoreName37[i] == username) {
                        ignored = true;
                        break;
                    }
                }
            }
            if (!ignored && c->overrideChat == 0) {
                // try {
                char *uncompressed = wordpack_unpack(buf, length);
                char *filtered = uncompressed; // TODO remove
                // char* filtered = WordFilter.filter(uncompressed);
                strcpy(player->pathing_entity.chat, filtered);
                player->pathing_entity.chatColor = colorEffect >> 8;
                player->pathing_entity.chatStyle = colorEffect & 0xff;
                player->pathing_entity.chatTimer = 150;
                if (type > 1) {
                    client_add_message(c, 1, filtered, player->name);
                } else {
                    client_add_message(c, 2, filtered, player->name);
                }
                // } catch (Exception ex) {
                // 	signlink.reporterror("cde2");
                // }
            }
        }
        buf->pos = start + length;
    }
    if ((mask & 0x100) == 256) {
        player->pathing_entity.spotanimId = g2(buf);
        int heightDelay = g4(buf);
        player->pathing_entity.spotanimOffset = heightDelay >> 16;
        player->pathing_entity.spotanimLastCycle = _Client.loop_cycle + (heightDelay & 0xffff);
        player->pathing_entity.spotanimFrame = 0;
        player->pathing_entity.spotanimCycle = 0;
        if (player->pathing_entity.spotanimLastCycle > _Client.loop_cycle) {
            player->pathing_entity.spotanimFrame = -1;
        }
        if (player->pathing_entity.spotanimId == 65535) {
            player->pathing_entity.spotanimId = -1;
        }
    }
    if ((mask & 0x200) == 512) {
        player->pathing_entity.forceMoveStartSceneTileX = g1(buf);
        player->pathing_entity.forceMoveStartSceneTileZ = g1(buf);
        player->pathing_entity.forceMoveEndSceneTileX = g1(buf);
        player->pathing_entity.forceMoveEndSceneTileZ = g1(buf);
        player->pathing_entity.forceMoveEndCycle = g2(buf) + _Client.loop_cycle;
        player->pathing_entity.forceMoveStartCycle = g2(buf) + _Client.loop_cycle;
        player->pathing_entity.forceMoveFaceDirection = g1(buf);
        player->pathing_entity.pathLength = 0;
        player->pathing_entity.pathTileX[0] = player->pathing_entity.forceMoveEndSceneTileX;
        player->pathing_entity.pathTileZ[0] = player->pathing_entity.forceMoveEndSceneTileZ;
    }
}

void getPlayer(Client *c, Packet *buf, int size) {
    c->entityRemovalCount = 0;
    c->entityUpdateCount = 0;

    getPlayerLocal(c, buf, size);
    getPlayerOldVis(c, buf, size);
    getPlayerNewVis(c, size, buf);
    getPlayerExtended(c, buf, size);

    for (int i = 0; i < c->entityRemovalCount; i++) {
        int index = c->entityRemovalIds[i];
        if (c->players[index]->pathing_entity.cycle != _Client.loop_cycle) {
            free(c->players[i]);
            c->players[index] = NULL;
        }
    }

    if (buf->pos != size) {
        rs2_error("Error packet size mismatch in getplayer pos:%d psize:%d\n", buf->pos, size);
        // signlink.reporterror("Error packet size mismatch in getplayer pos:" + buf.pos + " psize:" + size);
        // throw new RuntimeException("eek");
    }

    for (int index = 0; index < c->player_count; index++) {
        if (!c->players[c->player_ids[index]]) {
            rs2_error("%s null entry in pl list - pos:%d size:%d\n", c->username, index, c->player_count);
            // signlink.reporterror(c->username + " null entry in pl list - pos:" + index + " size:" + c->playerCount);
            // throw new RuntimeException("eek");
        }
    }
}

static void client_clear_caches(void) {
    lrucache_clear(_LocType.modelCacheStatic);
    lrucache_clear(_LocType.modelCacheDynamic);
    lrucache_clear(_NpcType.modelCache);
    lrucache_clear(_ObjType.modelCache);
    lrucache_clear(_ObjType.iconCache);
    lrucache_clear(_PlayerEntity.modelCache);
    lrucache_clear(_SpotAnimType.modelCache);
    bump_allocator_reset();
}

static void client_build_scene(Client *c) {
    // try {
    c->minimap_level = -1;
    linklist_clear(c->merged_locations);
    linklist_clear(c->locList);
    linklist_clear(c->spotanims);
    linklist_clear(c->projectiles);
    pix3d_clear_texels();
    client_clear_caches();
    world3d_reset(c->scene);
    for (int level = 0; level < 4; level++) {
        collisionmap_reset(c->levelCollisionMap[level]);
    }

    World *world = world_new(104, 104, c->levelHeightmap, c->levelTileFlags);
    _World.lowMemory = _World3D.lowMemory;

    int maps = c->sceneMapIndexLength;

    for (int index = 0; index < maps; index++) {
        int mapsquareX = c->sceneMapIndex[index] >> 8;
        int mapsquareZ = c->sceneMapIndex[index] & 0xff;

        // underground pass check
        if (mapsquareX == 33 && mapsquareZ >= 71 && mapsquareZ <= 73) {
            _World.lowMemory = false;
            break;
        }
    }

    if (_World.lowMemory) {
        world3d_set_minlevel(c->scene, c->currentLevel);
    } else {
        world3d_set_minlevel(c->scene, 0);
    }

    int8_t *data = calloc(100000, sizeof(int8_t));

    // NO_TIMEOUT
    p1isaac(c->out, 108);
    for (int i = 0; i < maps; i++) {
        int x = (c->sceneMapIndex[i] >> 8) * 64 - c->sceneBaseTileX;
        int z = (c->sceneMapIndex[i] & 0xff) * 64 - c->sceneBaseTileZ;
        int8_t *src = c->sceneMapLandData[i];

        if (src) {
            Packet *buf = packet_new(src, c->sceneMapLandDataIndexLength[i]);
            int length = g4(buf);
            bzip_decompress(data, src, c->sceneMapLandDataIndexLength[i] - 4, 4);
            free(buf);
            world_load_ground(world, (c->sceneCenterZoneX - 6) * 8, (c->sceneCenterZoneZ - 6) * 8, x, z, data, length);
        } else if (c->sceneCenterZoneZ < 800) {
            clearLandscape(world, z, x, 64, 64);
        }
    }

    // NO_TIMEOUT
    p1isaac(c->out, 108);
    for (int i = 0; i < maps; i++) {
        int8_t *src = c->sceneMapLocData[i];
        if (src) {
            Packet *buf = packet_new(src, c->sceneMapLocDataIndexLength[i]);
            int length = g4(buf);
            bzip_decompress(data, src, c->sceneMapLocDataIndexLength[i] - 4, 4);
            free(buf);
            int x = (c->sceneMapIndex[i] >> 8) * 64 - c->sceneBaseTileX;
            int z = (c->sceneMapIndex[i] & 0xff) * 64 - c->sceneBaseTileZ;
            world_load_locations(world, c->scene, c->locList, c->levelCollisionMap, data, length, x, z);
        }
    }

    free(data);

    // NO_TIMEOUT
    p1isaac(c->out, 108);
    world_build(world, c->scene, c->levelCollisionMap);
    pixmap_bind(c->area_viewport);

    // NO_TIMEOUT
    p1isaac(c->out, 108);
    for (LocEntity *loc = (LocEntity *)linklist_head(c->locList); loc; loc = (LocEntity *)linklist_next(c->locList)) {
        if ((c->levelTileFlags[1][loc->x][loc->z] & 0x2) == 2) {
            loc->level--;
            if (loc->level < 0) {
                linkable_unlink(&loc->link);
                free(loc);
            }
        }
    }

    for (int x = 0; x < 104; x++) {
        for (int z = 0; z < 104; z++) {
            sortObjStacks(c, x, z);
        }
    }

    for (LocAddEntity *loc = (LocAddEntity *)linklist_head(c->spawned_locations); loc; loc = (LocAddEntity *)linklist_next(c->spawned_locations)) {
        addLoc(c, loc->plane, loc->x, loc->z, loc->locIndex, loc->angle, loc->shape, loc->layer);
    }
    // } catch (Exception ignored) {
    // }

    lrucache_clear(_LocType.modelCacheStatic);
    pix3d_init_pool(PIX3D_POOL_COUNT);
    world_free(world);
}

void drawMinimapLoc(Client *c, int tileX, int tileZ, int level, int wallRgb, int doorRgb) {
    int bitset = world3d_get_wallbitset(c->scene, level, tileX, tileZ);
    if (bitset != 0) {
        int info = world3d_get_info(c->scene, level, tileX, tileZ, bitset);
        int angle = info >> 6 & 0x3;
        int shape = info & 0x1f;
        int rgb = wallRgb;
        if (bitset > 0) {
            rgb = doorRgb;
        }

        int *dst = c->image_minimap->pixels;
        int offset = tileX * 4 + (104 - 1 - tileZ) * 512 * 4 + 24624;
        int locId = bitset >> 14 & 0x7fff;

        LocType *loc = loctype_get(locId);
        if (loc->mapscene == -1) {
            if (shape == WALL_STRAIGHT || shape == WALL_L) {
                if (angle == 0) {
                    dst[offset] = rgb;
                    dst[offset + 512] = rgb;
                    dst[offset + 1024] = rgb;
                    dst[offset + 1536] = rgb;
                } else if (angle == 1) {
                    dst[offset] = rgb;
                    dst[offset + 1] = rgb;
                    dst[offset + 2] = rgb;
                    dst[offset + 3] = rgb;
                } else if (angle == 2) {
                    dst[offset + 3] = rgb;
                    dst[offset + 3 + 512] = rgb;
                    dst[offset + 3 + 1024] = rgb;
                    dst[offset + 3 + 1536] = rgb;
                } else if (angle == 3) {
                    dst[offset + 1536] = rgb;
                    dst[offset + 1536 + 1] = rgb;
                    dst[offset + 1536 + 2] = rgb;
                    dst[offset + 1536 + 3] = rgb;
                }
            }

            if (shape == WALL_SQUARECORNER) {
                if (angle == 0) {
                    dst[offset] = rgb;
                } else if (angle == 1) {
                    dst[offset + 3] = rgb;
                } else if (angle == 2) {
                    dst[offset + 3 + 1536] = rgb;
                } else if (angle == 3) {
                    dst[offset + 1536] = rgb;
                }
            }

            if (shape == WALL_L) {
                if (angle == 3) {
                    dst[offset] = rgb;
                    dst[offset + 512] = rgb;
                    dst[offset + 1024] = rgb;
                    dst[offset + 1536] = rgb;
                } else if (angle == 0) {
                    dst[offset] = rgb;
                    dst[offset + 1] = rgb;
                    dst[offset + 2] = rgb;
                    dst[offset + 3] = rgb;
                } else if (angle == 1) {
                    dst[offset + 3] = rgb;
                    dst[offset + 3 + 512] = rgb;
                    dst[offset + 3 + 1024] = rgb;
                    dst[offset + 3 + 1536] = rgb;
                } else if (angle == 2) {
                    dst[offset + 1536] = rgb;
                    dst[offset + 1536 + 1] = rgb;
                    dst[offset + 1536 + 2] = rgb;
                    dst[offset + 1536 + 3] = rgb;
                }
            }
        } else {
            Pix8 *scene = c->image_mapscene[loc->mapscene];
            if (scene) {
                int offsetX = (loc->width * 4 - scene->width) / 2;
                int offsetY = (loc->length * 4 - scene->height) / 2;
                pix8_draw(scene, tileX * 4 + 48 + offsetX, (104 - tileZ - loc->length) * 4 + offsetY + 48);
            }
        }
    }

    bitset = world3d_get_locbitset(c->scene, level, tileX, tileZ);
    if (bitset != 0) {
        int info = world3d_get_info(c->scene, level, tileX, tileZ, bitset);
        int angle = info >> 6 & 0x3;
        int shape = info & 0x1f;
        int locId = bitset >> 14 & 0x7fff;
        LocType *loc = loctype_get(locId);

        if (loc->mapscene != -1) {
            Pix8 *scene = c->image_mapscene[loc->mapscene];
            if (scene) {
                int offsetX = (loc->width * 4 - scene->width) / 2;
                int offsetY = (loc->length * 4 - scene->height) / 2;
                pix8_draw(scene, tileX * 4 + 48 + offsetX, (104 - tileZ - loc->length) * 4 + offsetY + 48);
            }
        } else if (shape == WALL_DIAGONAL) {
            int rgb = 0xeeeeee;
            if (bitset > 0) {
                rgb = 0xee0000;
            }

            int *dst = c->image_minimap->pixels;
            int offset = tileX * 4 + (104 - 1 - tileZ) * 512 * 4 + 24624;

            if (angle == 0 || angle == 2) {
                dst[offset + 1536] = rgb;
                dst[offset + 1024 + 1] = rgb;
                dst[offset + 512 + 2] = rgb;
                dst[offset + 3] = rgb;
            } else {
                dst[offset] = rgb;
                dst[offset + 512 + 1] = rgb;
                dst[offset + 1024 + 2] = rgb;
                dst[offset + 1536 + 3] = rgb;
            }
        }
    }

    bitset = world3d_get_grounddecorationbitset(c->scene, level, tileX, tileZ);
    if (bitset != 0) {
        int locId = bitset >> 14 & 0x7fff;
        LocType *loc = loctype_get(locId);
        if (loc->mapscene != -1) {
            Pix8 *scene = c->image_mapscene[loc->mapscene];
            if (scene) {
                int offsetX = (loc->width * 4 - scene->width) / 2;
                int offsetY = (loc->length * 4 - scene->height) / 2;
                pix8_draw(scene, tileX * 4 + 48 + offsetX, (104 - tileZ - loc->length) * 4 + offsetY + 48);
            }
        }
    }
}

void createMinimap(Client *c, int level) {
    int *pixels = c->image_minimap->pixels;
    int length = c->image_minimap->width * c->image_minimap->height;
    for (int i = 0; i < length; i++) {
        pixels[i] = 0;
    }

    for (int z = 1; z < 104 - 1; z++) {
        int offset = (104 - 1 - z) * 512 * 4 + 24628;

        for (int x = 1; x < 104 - 1; x++) {
            if ((c->levelTileFlags[level][x][z] & 0x18) == 0) {
                world3d_draw_minimaptile(c->scene, level, x, z, pixels, offset, 512);
            }

            if (level < 3 && (c->levelTileFlags[level + 1][x][z] & 0x8) != 0) {
                world3d_draw_minimaptile(c->scene, level + 1, x, z, pixels, offset, 512);
            }

            offset += 4;
        }
    }

    int wallRgb = (((int)(jrand() * 20.0) + 238 - 10) << 16) + (((int)(jrand() * 20.0) + 238 - 10) << 8) + (int)(jrand() * 20.0) + 238 - 10;
    int doorRgb = ((int)(jrand() * 20.0) + 238 - 10) << 16;

    pix24_bind(c->image_minimap);

    for (int z = 1; z < 104 - 1; z++) {
        for (int x = 1; x < 104 - 1; x++) {
            if ((c->levelTileFlags[level][x][z] & 0x18) == 0) {
                drawMinimapLoc(c, x, z, level, wallRgb, doorRgb);
            }

            if (level < 3 && (c->levelTileFlags[level + 1][x][z] & 0x8) != 0) {
                drawMinimapLoc(c, x, z, level + 1, wallRgb, doorRgb);
            }
        }
    }

    pixmap_bind(c->area_viewport);
    c->activeMapFunctionCount = 0;

    for (int x = 0; x < 104; x++) {
        for (int z = 0; z < 104; z++) {
            int bitset = world3d_get_grounddecorationbitset(c->scene, c->currentLevel, x, z);
            if (bitset == 0) {
                continue;
            }

            bitset = bitset >> 14 & 0x7fff;

            int func = loctype_get(bitset)->mapfunction;
            if (func < 0) {
                continue;
            }

            int stx = x;
            int stz = z;

            if (func != 22 && func != 29 && func != 34 && func != 36 && func != 46 && func != 47 && func != 48) {
                int8_t maxX = 104;
                int8_t maxZ = 104;
                int **flags = c->levelCollisionMap[c->currentLevel]->flags;

                for (int i = 0; i < 10; i++) {
                    int random = (int)(jrand() * 4.0);
                    if (random == 0 && stx > 0 && stx > x - 3 && (flags[stx - 1][stz] & 0x280108) == 0) {
                        stx--;
                    }

                    if (random == 1 && stx < maxX - 1 && stx < x + 3 && (flags[stx + 1][stz] & 0x280180) == 0) {
                        stx++;
                    }

                    if (random == 2 && stz > 0 && stz > z - 3 && (flags[stx][stz - 1] & 0x280102) == 0) {
                        stz--;
                    }

                    if (random == 3 && stz < maxZ - 1 && stz < z + 3 && (flags[stx][stz + 1] & 0x280120) == 0) {
                        stz++;
                    }
                }
            }

            c->activeMapFunctions[c->activeMapFunctionCount] = c->image_mapfunction[func];
            c->activeMapFunctionX[c->activeMapFunctionCount] = stx;
            c->activeMapFunctionZ[c->activeMapFunctionCount] = stz;
            c->activeMapFunctionCount++;
        }
    }
}

void closeInterfaces(Client *c) {
    // CLOSE_MODAL
    p1isaac(c->out, 231);

    if (c->sidebar_interface_id != -1) {
        c->sidebar_interface_id = -1;
        c->redraw_sidebar = true;
        c->pressed_continue_option = false;
        c->redraw_sideicons = true;
    }

    if (c->chat_interface_id != -1) {
        c->chat_interface_id = -1;
        c->redraw_chatback = true;
        c->pressed_continue_option = false;
    }

    c->viewport_interface_id = -1;
}

void addLoc(Client *c, int level, int x, int z, int id, int angle, int shape, int layer) {
    if (x < 1 || z < 1 || x > 102 || z > 102) {
        return;
    }

    if (_Client.lowmem && level != c->currentLevel) {
        return;
    }

    int bitset = 0;

    if (layer == 0) {
        bitset = world3d_get_wallbitset(c->scene, level, x, z);
    }

    if (layer == 1) {
        bitset = world3d_get_walldecorationbitset(c->scene, level, z, x);
    }

    if (layer == 2) {
        bitset = world3d_get_locbitset(c->scene, level, x, z);
    }

    if (layer == 3) {
        bitset = world3d_get_grounddecorationbitset(c->scene, level, x, z);
    }

    if (bitset != 0) {
        int otherInfo = world3d_get_info(c->scene, level, x, z, bitset);
        int otherId = bitset >> 14 & 0x7fff;
        int otherShape = otherInfo & 0x1f;
        int otherRotation = otherInfo >> 6;

        if (layer == 0) {
            world3d_remove_wall(c->scene, level, x, z, 1);
            LocType *type = loctype_get(otherId);

            if (type->blockwalk) {
                collisionmap_del_wall(c->levelCollisionMap[level], x, z, otherShape, otherRotation, type->blockrange);
            }
        }

        if (layer == 1) {
            world3d_remove_walldecoration(c->scene, level, x, z);
        }

        if (layer == 2) {
            world3d_remove_loc(c->scene, level, x, z);
            LocType *type = loctype_get(otherId);

            if (x + type->width > 104 - 1 || z + type->width > 104 - 1 || x + type->length > 104 - 1 || z + type->length > 104 - 1) {
                return;
            }

            if (type->blockwalk) {
                collisionmap_del_loc(c->levelCollisionMap[level], x, z, type->width, type->length, otherRotation, type->blockrange);
            }
        }

        if (layer == 3) {
            world3d_remove_grounddecoration(c->scene, level, x, z);
            LocType *type = loctype_get(otherId);

            if (type->blockwalk && type->active) {
                collisionmap_remove_blocked(c->levelCollisionMap[level], x, z);
            }
        }
    }

    if (id >= 0) {
        int tileLevel = level;

        if (level < 3 && (c->levelTileFlags[1][x][z] & 0x2) == 2) {
            tileLevel = level + 1;
        }

        world_add_loc(level, x, z, c->scene, c->levelHeightmap, c->locList, c->levelCollisionMap[level], id, shape, angle, tileLevel);
    }
}

void sortObjStacks(Client *c, int x, int z) {
    LinkList *objStacks = c->level_obj_stacks[c->currentLevel][x][z];
    if (!objStacks) {
        world3d_remove_objstack(c->scene, c->currentLevel, x, z);
        return;
    }

    int topCost = -99999999;
    ObjStackEntity *topObj = NULL;

    for (ObjStackEntity *obj = (ObjStackEntity *)linklist_head(objStacks); obj != NULL; obj = (ObjStackEntity *)linklist_next(objStacks)) {
        ObjType *type = objtype_get(obj->index);
        int cost = type->cost;

        if (type->stackable) {
            cost *= obj->count + 1;
        }

        if (cost > topCost) {
            topCost = cost;
            topObj = obj;
        }
    }

    linklist_add_head(objStacks, &topObj->link);

    int bottomObjId = -1;
    int middleObjId = -1;
    int bottomObjCount = 0;
    int middleObjCount = 0;
    for (ObjStackEntity *obj = (ObjStackEntity *)linklist_head(objStacks); obj; obj = (ObjStackEntity *)linklist_next(objStacks)) {
        if (obj->index != topObj->index && bottomObjId == -1) {
            bottomObjId = obj->index;
            bottomObjCount = obj->count;
        }

        if (obj->index != topObj->index && obj->index != bottomObjId && middleObjId == -1) {
            middleObjId = obj->index;
            middleObjCount = obj->count;
        }
    }

    Model *bottomObj = NULL;
    if (bottomObjId != -1) {
        bottomObj = objtype_get_interfacemodel(objtype_get(bottomObjId), bottomObjCount, true);
    }

    Model *middleObj = NULL;
    if (middleObjId != -1) {
        middleObj = objtype_get_interfacemodel(objtype_get(middleObjId), middleObjCount, true);
    }

    int bitset = x + (z << 7) + 1610612736;
    ObjType *type = objtype_get(topObj->index);
    world3d_add_objstack(c->scene, x, z, getHeightmapY(c, c->currentLevel, x * 128 + 64, z * 128 + 64), c->currentLevel, bitset, objtype_get_interfacemodel(type, topObj->count, true), middleObj, bottomObj);
}

void readZonePacket(Client *c, Packet *buf, int opcode) {
    int pos = g1(buf);
    int x = c->baseX + (pos >> 4 & 0x7);
    int z = c->baseZ + (pos & 0x7);

    if (opcode == 59 || opcode == 76) {
        // LOC_ADD_CHANGE || LOC_DEL
        int info = g1(buf);
        int shape = info >> 2;
        int angle = info & 0x3;
        int layer = LOC_SHAPE_TO_LAYER[shape];
        int id;
        if (opcode == 76) {
            id = -1;
        } else {
            id = g2(buf);
        }
        if (x >= 0 && z >= 0 && x < 104 && z < 104) {
            LocAddEntity *loc = NULL;
            for (LocAddEntity *next = (LocAddEntity *)linklist_head(c->spawned_locations); next != NULL; next = (LocAddEntity *)linklist_next(c->spawned_locations)) {
                if (next->plane == c->currentLevel && next->x == x && next->z == z && next->layer == layer) {
                    loc = next;
                    break;
                }
            }
            if (!loc) {
                int bitset = 0;
                int otherId = -1;
                int otherShape = 0;
                int otherAngle = 0;
                if (layer == 0) {
                    bitset = world3d_get_wallbitset(c->scene, c->currentLevel, x, z);
                }
                if (layer == 1) {
                    bitset = world3d_get_walldecorationbitset(c->scene, c->currentLevel, z, x);
                }
                if (layer == 2) {
                    bitset = world3d_get_locbitset(c->scene, c->currentLevel, x, z);
                }
                if (layer == 3) {
                    bitset = world3d_get_grounddecorationbitset(c->scene, c->currentLevel, x, z);
                }
                if (bitset != 0) {
                    int otherInfo = world3d_get_info(c->scene, c->currentLevel, x, z, bitset);
                    otherId = bitset >> 14 & 0x7fff;
                    otherShape = otherInfo & 0x1f;
                    otherAngle = otherInfo >> 6;
                }
                loc = calloc(1, sizeof(LocAddEntity));
                loc->plane = c->currentLevel;
                loc->layer = layer;
                loc->x = x;
                loc->z = z;
                loc->lastLocIndex = otherId;
                loc->lastShape = otherShape;
                loc->lastAngle = otherAngle;
                linklist_add_tail(c->spawned_locations, &loc->link);
            }
            loc->locIndex = id;
            loc->shape = shape;
            loc->angle = angle;
            addLoc(c, c->currentLevel, x, z, id, angle, shape, layer);
        }
    } else if (opcode == 42) {
        // LOC_ANIM
        int info = g1(buf);
        int shape = info >> 2;
        int layer = LOC_SHAPE_TO_LAYER[shape];
        int id = g2(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104) {
            int bitset = 0;
            if (layer == 0) {
                bitset = world3d_get_wallbitset(c->scene, c->currentLevel, x, z);
            }
            if (layer == 1) {
                bitset = world3d_get_walldecorationbitset(c->scene, c->currentLevel, z, x);
            }
            if (layer == 2) {
                bitset = world3d_get_locbitset(c->scene, c->currentLevel, x, z);
            }
            if (layer == 3) {
                bitset = world3d_get_grounddecorationbitset(c->scene, c->currentLevel, x, z);
            }
            if (bitset != 0) {
                LocEntity *loc = locentity_new(bitset >> 14 & 0x7fff, c->currentLevel, layer, x, z, _SeqType.instances[id], false);
                linklist_add_tail(c->locList, &loc->link);
            }
        }
    } else if (opcode == 223) {
        // OBJ_ADD
        int id = g2(buf);
        int count = g2(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104) {
            ObjStackEntity *obj = objstackentity_new();
            obj->index = id;
            obj->count = count;
            if (!c->level_obj_stacks[c->currentLevel][x][z]) {
                c->level_obj_stacks[c->currentLevel][x][z] = linklist_new();
            }
            linklist_add_tail(c->level_obj_stacks[c->currentLevel][x][z], &obj->link);
            sortObjStacks(c, x, z);
        }
    } else if (opcode == 49) {
        // OBJ_DEL
        int id = g2(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104) {
            LinkList *list = c->level_obj_stacks[c->currentLevel][x][z];
            if (list) {
                for (ObjStackEntity *next = (ObjStackEntity *)linklist_head(list); next; next = (ObjStackEntity *)linklist_next(list)) {
                    if (next->index == (id & 0x7fff)) {
                        linkable_unlink(&next->link);
                        free(next);
                        break;
                    }
                }
                if (!linklist_head(list)) {
                    linklist_free(c->level_obj_stacks[c->currentLevel][x][z]);
                    c->level_obj_stacks[c->currentLevel][x][z] = NULL;
                }
                sortObjStacks(c, x, z);
            }
        }
    } else if (opcode == 69) {
        // MAP_PROJANIM
        int dx = x + g1b(buf);
        int dz = z + g1b(buf);
        int target = g2b(buf);
        int spotanim = g2(buf);
        int srcHeight = g1(buf);
        int dstHeight = g1(buf);
        int startDelay = g2(buf);
        int endDelay = g2(buf);
        int peak = g1(buf);
        int arc = g1(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104 && dx >= 0 && dz >= 0 && dx < 104 && dz < 104) {
            x = x * 128 + 64;
            z = z * 128 + 64;
            dx = dx * 128 + 64;
            dz = dz * 128 + 64;
            ProjectileEntity *proj = projectileentity_new(spotanim, c->currentLevel, x, getHeightmapY(c, c->currentLevel, x, z) - srcHeight, z, startDelay + _Client.loop_cycle, endDelay + _Client.loop_cycle, peak, arc, target, dstHeight);
            projectileentity_update_velocity(proj, dx, getHeightmapY(c, c->currentLevel, dx, dz) - dstHeight, dz, startDelay + _Client.loop_cycle);
            linklist_add_tail(c->projectiles, &proj->entity.link);
        }
    } else if (opcode == 191) {
        // MAP_ANIM
        int id = g2(buf);
        int height = g1(buf);
        int delay = g2(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104) {
            x = x * 128 + 64;
            z = z * 128 + 64;
            SpotAnimEntity *spotanim = spotanimentity_new(id, c->currentLevel, x, z, getHeightmapY(c, c->currentLevel, x, z) - height, _Client.loop_cycle, delay);
            linklist_add_tail(c->spotanims, &spotanim->entity.link);
        }
    } else if (opcode == 50) {
        // OBJ_REVEAL
        int id = g2(buf);
        int count = g2(buf);
        int receiver = g2(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104 && receiver != c->local_pid) {
            ObjStackEntity *obj = objstackentity_new();
            obj->index = id;
            obj->count = count;
            if (!c->level_obj_stacks[c->currentLevel][x][z]) {
                c->level_obj_stacks[c->currentLevel][x][z] = linklist_new();
            }
            linklist_add_tail(c->level_obj_stacks[c->currentLevel][x][z], &obj->link);
            sortObjStacks(c, x, z);
        }
    } else if (opcode == 23) {
        // LOC_MERGE
        int info = g1(buf);
        int shape = info >> 2;
        int angle = info & 0x3;
        int layer = LOC_SHAPE_TO_LAYER[shape];
        int id = g2(buf);
        int start = g2(buf);
        int end = g2(buf);
        int pid = g2(buf);
        int8_t east = g1b(buf);
        int8_t south = g1b(buf);
        int8_t west = g1b(buf);
        int8_t north = g1b(buf);

        PlayerEntity *player;
        if (pid == c->local_pid) {
            player = c->local_player;
        } else {
            player = c->players[pid];
        }

        if (player) {
            LocMergeEntity *loc1 = locmergeentity_new(c->currentLevel, layer, x, z, -1, angle, shape, start + _Client.loop_cycle);
            linklist_add_tail(c->merged_locations, &loc1->link);

            LocMergeEntity *loc2 = locmergeentity_new(c->currentLevel, layer, x, z, id, angle, shape, end + _Client.loop_cycle);
            linklist_add_tail(c->merged_locations, &loc2->link);

            int y0 = c->levelHeightmap[c->currentLevel][x][z];
            int y1 = c->levelHeightmap[c->currentLevel][x + 1][z];
            int y2 = c->levelHeightmap[c->currentLevel][x + 1][z + 1];
            int y3 = c->levelHeightmap[c->currentLevel][x][z + 1];
            LocType *loc = loctype_get(id);

            player->locStartCycle = start + _Client.loop_cycle;
            player->locStopCycle = end + _Client.loop_cycle;
            player->locModel = loctype_get_model(loc, shape, angle, y0, y1, y2, y3, -1);

            int width = loc->width;
            int height = loc->length;
            if (angle == 1 || angle == 3) {
                width = loc->length;
                height = loc->width;
            }

            player->locOffsetX = x * 128 + width * 64;
            player->locOffsetZ = z * 128 + height * 64;
            player->locOffsetY = getHeightmapY(c, c->currentLevel, player->locOffsetX, player->locOffsetZ);

            int8_t tmp;
            if (east > west) {
                tmp = east;
                east = west;
                west = tmp;
            }

            if (south > north) {
                tmp = south;
                south = north;
                north = tmp;
            }

            player->minTileX = x + east;
            player->maxTileX = x + west;
            player->minTileZ = z + south;
            player->maxTileZ = z + north;
        }
    } else if (opcode == 151) {
        // OBJ_COUNT
        int id = g2(buf);
        int oldCount = g2(buf);
        int newCount = g2(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104) {
            LinkList *list = c->level_obj_stacks[c->currentLevel][x][z];
            if (list) {
                for (ObjStackEntity *next = (ObjStackEntity *)linklist_head(list); next; next = (ObjStackEntity *)linklist_next(list)) {
                    if (next->index == (id & 0x7fff) && next->count == oldCount) {
                        next->count = newCount;
                        break;
                    }
                }
                sortObjStacks(c, x, z);
            }
        }
    }
}

void getNpcPos(Client *c, Packet *buf, int size) {
    c->entityRemovalCount = 0;
    c->entityUpdateCount = 0;

    getNpcPosOldVis(c, buf, size);
    getNpcPosNewVis(c, buf, size);
    getNpcPosExtended(c, buf, size);

    for (int i = 0; i < c->entityRemovalCount; i++) {
        int index = c->entityRemovalIds[i];
        if (c->npcs[index]->pathing_entity.cycle != _Client.loop_cycle) {
            c->npcs[index]->type = NULL;
            free(c->npcs[index]);
            c->npcs[index] = NULL;
        }
    }

    if (buf->pos != size) {
        rs2_error("%s size mismatch in getnpcpos - pos:%d psize:%d\n", c->username, buf->pos, size);
        // signlink.reporterror(c->username + " size mismatch in getnpcpos - pos:" + buf.pos + " psize:" + size);
        // throw new RuntimeException("eek");
    }

    for (int i = 0; i < c->npc_count; i++) {
        if (c->npcs[c->npc_ids[i]] == NULL) {
            rs2_error("%s null entry in npc lit - pos:%d size:%d\n", c->username, i, c->npc_count);
            // signlink.reporterror(c->username + " null entry in npc list - pos:" + i + " size:" + c->npcCount);
            // throw new RuntimeException("eek");
        }
    }
}

void getNpcPosExtended(Client *c, Packet *buf, int size) {
    (void)size;
    for (int i = 0; i < c->entityUpdateCount; i++) {
        int id = c->entityUpdateIds[i];
        NpcEntity *npc = c->npcs[id];
        int mask = g1(buf);

        npc->pathing_entity.lastMask = mask;
        npc->pathing_entity.lastMaskCycle = _Client.loop_cycle;

        if ((mask & 0x2) == 2) {
            int seqId = g2(buf);
            if (seqId == 65535) {
                seqId = -1;
            }
            if (seqId == npc->pathing_entity.primarySeqId) {
                npc->pathing_entity.primarySeqLoop = 0;
            }
            int delay = g1(buf);
            if (seqId == -1 || npc->pathing_entity.primarySeqId == -1 || _SeqType.instances[seqId]->priority > _SeqType.instances[npc->pathing_entity.primarySeqId]->priority || _SeqType.instances[npc->pathing_entity.primarySeqId]->priority == 0) {
                npc->pathing_entity.primarySeqId = seqId;
                npc->pathing_entity.primarySeqFrame = 0;
                npc->pathing_entity.primarySeqCycle = 0;
                npc->pathing_entity.primarySeqDelay = delay;
                npc->pathing_entity.primarySeqLoop = 0;
            }
        }
        if ((mask & 0x4) == 4) {
            npc->pathing_entity.targetId = g2(buf);
            if (npc->pathing_entity.targetId == 65535) {
                npc->pathing_entity.targetId = -1;
            }
        }
        if ((mask & 0x8) == 8) {
            strcpy(npc->pathing_entity.chat, gjstr(buf));
            npc->pathing_entity.chatTimer = 100;
        }
        if ((mask & 0x10) == 16) {
            npc->pathing_entity.damage = g1(buf);
            npc->pathing_entity.damageType = g1(buf);
            npc->pathing_entity.combatCycle = _Client.loop_cycle + 400;
            npc->pathing_entity.health = g1(buf);
            npc->pathing_entity.totalHealth = g1(buf);
        }
        if ((mask & 0x20) == 32) {
            npc->type = npctype_get(g2(buf));
            npc->pathing_entity.seqWalkId = npc->type->walkanim;
            npc->pathing_entity.seqTurnAroundId = npc->type->walkanim_b;
            npc->pathing_entity.seqTurnLeftId = npc->type->walkanim_r;
            npc->pathing_entity.seqTurnRightId = npc->type->walkanim_l;
            npc->pathing_entity.seqStandId = npc->type->readyanim;
        }
        if ((mask & 0x40) == 64) {
            npc->pathing_entity.spotanimId = g2(buf);
            int info = g4(buf);
            npc->pathing_entity.spotanimOffset = info >> 16;
            npc->pathing_entity.spotanimLastCycle = _Client.loop_cycle + (info & 0xffff);
            npc->pathing_entity.spotanimFrame = 0;
            npc->pathing_entity.spotanimCycle = 0;
            if (npc->pathing_entity.spotanimLastCycle > _Client.loop_cycle) {
                npc->pathing_entity.spotanimFrame = -1;
            }
            if (npc->pathing_entity.spotanimId == 65535) {
                npc->pathing_entity.spotanimId = -1;
            }
        }
        if ((mask & 0x80) == 128) {
            npc->pathing_entity.targetTileX = g2(buf);
            npc->pathing_entity.targetTileZ = g2(buf);
            npc->pathing_entity.lastFaceX = npc->pathing_entity.targetTileX;
            npc->pathing_entity.lastFaceZ = npc->pathing_entity.targetTileZ;
        }
    }
}

void getNpcPosNewVis(Client *c, Packet *buf, int size) {
    while (buf->bit_pos + 21 < size * 8) {
        int index = gbit(buf, 13);
        if (index == 8191) {
            break;
        }
        if (!c->npcs[index]) {
            c->npcs[index] = npcentity_new();
        }
        NpcEntity *npc = c->npcs[index];
        c->npc_ids[c->npc_count++] = index;
        npc->pathing_entity.cycle = _Client.loop_cycle;
        npc->type = npctype_get(gbit(buf, 11));
        npc->pathing_entity.size = npc->type->size;
        npc->pathing_entity.seqWalkId = npc->type->walkanim;
        npc->pathing_entity.seqTurnAroundId = npc->type->walkanim_b;
        npc->pathing_entity.seqTurnLeftId = npc->type->walkanim_r;
        npc->pathing_entity.seqTurnRightId = npc->type->walkanim_l;
        npc->pathing_entity.seqStandId = npc->type->readyanim;
        int dx = gbit(buf, 5);
        if (dx > 15) {
            dx -= 32;
        }
        int dz = gbit(buf, 5);
        if (dz > 15) {
            dz -= 32;
        }
        pathingentity_teleport(&npc->pathing_entity, false, c->local_player->pathing_entity.pathTileX[0] + dx, c->local_player->pathing_entity.pathTileZ[0] + dz);
        int extendedInfo = gbit(buf, 1);
        if (extendedInfo == 1) {
            c->entityUpdateIds[c->entityUpdateCount++] = index;
        }
    }
    access_bytes(buf);
}

void getNpcPosOldVis(Client *c, Packet *buf, int size) {
    (void)size;
    access_bits(buf);

    int count = gbit(buf, 8);
    if (count < c->npc_count) {
        for (int i = count; i < c->npc_count; i++) {
            c->entityRemovalIds[c->entityRemovalCount++] = c->npc_ids[i];
        }
    }

    if (count > c->npc_count) {
        rs2_error("%s Too many npcs\n", c->username);
        // signlink.reporterror(c->username + " Too many npcs");
        // throw new RuntimeException("eek");
    }

    c->npc_count = 0;
    for (int i = 0; i < count; i++) {
        int index = c->npc_ids[i];
        NpcEntity *npc = c->npcs[index];

        int info = gbit(buf, 1);
        if (info == 0) {
            c->npc_ids[c->npc_count++] = index;
            npc->pathing_entity.cycle = _Client.loop_cycle;
        } else {
            int op = gbit(buf, 2);

            if (op == 0) {
                c->npc_ids[c->npc_count++] = index;
                npc->pathing_entity.cycle = _Client.loop_cycle;
                c->entityUpdateIds[c->entityUpdateCount++] = index;
            } else if (op == 1) {
                c->npc_ids[c->npc_count++] = index;
                npc->pathing_entity.cycle = _Client.loop_cycle;

                int walkDir = gbit(buf, 3);
                pathingentity_movealongroute(&npc->pathing_entity, false, walkDir);

                int extendedInfo = gbit(buf, 1);
                if (extendedInfo == 1) {
                    c->entityUpdateIds[c->entityUpdateCount++] = index;
                }
            } else if (op == 2) {
                c->npc_ids[c->npc_count++] = index;
                npc->pathing_entity.cycle = _Client.loop_cycle;

                int walkDir = gbit(buf, 3);
                pathingentity_movealongroute(&npc->pathing_entity, true, walkDir);
                int runDir = gbit(buf, 3);
                pathingentity_movealongroute(&npc->pathing_entity, true, runDir);

                int extendedInfo = gbit(buf, 1);
                if (extendedInfo == 1) {
                    c->entityUpdateIds[c->entityUpdateCount++] = index;
                }
            } else if (op == 3) {
                c->entityRemovalIds[c->entityRemovalCount++] = index;
            }
        }
    }
}

void client_add_message(Client *c, int type, const char *text, const char *sender) {
    if (type == 0 && c->sticky_chat_interface_id != -1) {
        strcpy(c->modal_message, text);
        c->shell->mouse_click_button = 0;
    }

    if (c->chat_interface_id == -1) {
        c->redraw_chatback = true;
    }

    for (int i = 99; i > 0; i--) {
        c->message_type[i] = c->message_type[i - 1];
        strcpy(c->message_sender[i], c->message_sender[i - 1]);
        strcpy(c->message_text[i], c->message_text[i - 1]);
    }

    // TODO: debug
    // if (c->showDebug && type == 0) {
    // 	text = "[" + (loopCycle / 30) + "]: " + text;
    // }

    c->message_type[0] = type;
    strcpy(c->message_sender[0], sender);
    strcpy(c->message_text[0], text);
}

void updateVarp(Client *c, int id) {
    int clientcode = _VarpType.instances[id]->clientcode;
    if (clientcode == 0) {
        return;
    }

    int value = c->varps[id];
    if (clientcode == 1) {
        if (value == 1) {
            pix3d_set_brightness(0.9);
        } else if (value == 2) {
            pix3d_set_brightness(0.8);
        } else if (value == 3) {
            pix3d_set_brightness(0.7);
        } else if (value == 4) {
            pix3d_set_brightness(0.6);
        }

        lrucache_clear(_ObjType.iconCache);
        c->redraw_background = true;
    } else if (clientcode == 3) {
        bool lastMidiActive = c->midiActive;
        if (value == 0) {
            platform_set_midi_volume(1.0);
            c->midiActive = true;
        } else if (value == 1) {
            platform_set_midi_volume(0.75);
            c->midiActive = true;
        } else if (value == 2) {
            platform_set_midi_volume(0.5);
            c->midiActive = true;
        } else if (value == 3) {
            platform_set_midi_volume(0.25);
            c->midiActive = true;
        } else if (value == 4) {
            c->midiActive = false;
        }

        if (c->midiActive != lastMidiActive) {
            if (c->midiActive) {
                platform_set_midi(c->currentMidi, c->midiCrc, c->midiSize);
            } else {
                platform_stop_midi();
            }

            c->nextMusicDelay = 0;
        }
    } else if (clientcode == 4) {
        if (value == 0) {
            c->wave_enabled = true;
            platform_set_wave_volume(128);
        } else if (value == 1) {
            c->wave_enabled = true;
            platform_set_wave_volume(96);
        } else if (value == 2) {
            c->wave_enabled = true;
            platform_set_wave_volume(64);
        } else if (value == 3) {
            c->wave_enabled = true;
            platform_set_wave_volume(32);
        } else if (value == 4) {
            c->wave_enabled = false;
        }
    } else if (clientcode == 5) {
        c->mouseButtonsOption = value;
    } else if (clientcode == 6) {
        c->chatEffects = value;
    } else if (clientcode == 8) {
        c->split_private_chat = value;
        c->redraw_chatback = true;
    }
}

void reset_interface_animation(int id) {
    Component *parent = _Component.instances[id];
    for (int i = 0; i < parent->childCount && parent->childId[i] != -1; i++) {
        Component *child = _Component.instances[parent->childId[i]];
        if (child->type == 1) {
            reset_interface_animation(child->id);
        }
        child->seqFrame = 0;
        child->seqCycle = 0;
    }
}

void client_try_reconnect(Client *c) {
    if (c->idle_timeout > 0) {
        client_logout(c);
    } else {
        pixmap_bind(c->area_viewport);
        drawStringCenter(c->font_plain12, 257, 144, "Connection lost", BLACK);
        drawStringCenter(c->font_plain12, 256, 143, "Connection lost", WHITE);
        drawStringCenter(c->font_plain12, 257, 159, "Please wait - attempting to reestablish", BLACK);
        drawStringCenter(c->font_plain12, 256, 158, "Please wait - attempting to reestablish", WHITE);
        pixmap_draw(c->area_viewport, 8, 11);
        c->flag_scene_tile_x = 0;
        ClientStream *stream = c->stream;
        c->ingame = false;

        client_login(c, c->username, c->password, true);
        if (!c->ingame) {
            client_logout(c);
        }

        // try {
        clientstream_close(stream);
        // } catch (@Pc(80) Exception ex) {
        // }
    }
}

void client_logout(Client *c) {
    // try {
    if (c->stream) {
        clientstream_close(c->stream);
    }
    // } catch (@Pc(9) Exception ignored) {
    // }

    c->stream = NULL;
    c->ingame = false;
    c->title_screen_state = 0;
    if (!_Custom.remember_username) {
        c->username[0] = '\0';
    }
    if (!_Custom.remember_password) {
        c->password[0] = '\0';
    }

    inputtracking_set_disabled(&_InputTracking);
    client_clear_caches();
    world3d_reset(c->scene);

    for (int level = 0; level < 4; level++) {
        collisionmap_reset(c->levelCollisionMap[level]);
    }

    platform_stop_midi();

    c->currentMidi[0] = '\0';
    c->nextMusicDelay = 0;
}

void client_update_title(Client *c) {
    if (c->title_screen_state == 0) {
        int x = c->shell->screen_width / 2 - 80;
        int y = c->shell->screen_height / 2 + 20;

        y += 20;
        if (c->shell->mouse_click_button == 1 && c->shell->mouse_click_x >= x - 75 && c->shell->mouse_click_x <= x + 75 && c->shell->mouse_click_y >= y - 20 && c->shell->mouse_click_y <= y + 20) {
            c->title_screen_state = 3;
            c->title_login_field = 0;
        }

        x = c->shell->screen_width / 2 + 80;
        if (c->shell->mouse_click_button == 1 && c->shell->mouse_click_x >= x - 75 && c->shell->mouse_click_x <= x + 75 && c->shell->mouse_click_y >= y - 20 && c->shell->mouse_click_y <= y + 20) {
            c->login_message0 = "";
            c->login_message1 = "Enter your username & password.";
            c->title_screen_state = 2;
            c->title_login_field = 0;
        }
    } else if (c->title_screen_state == 2) {
        int y = c->shell->screen_height / 2 - 40;
        y += 30;
        y += 25;

        if (c->shell->mouse_click_button == 1 && c->shell->mouse_click_y >= y - 15 && c->shell->mouse_click_y < y) {
            c->title_login_field = 0;
        }
        y += 15;

        if (c->shell->mouse_click_button == 1 && c->shell->mouse_click_y >= y - 15 && c->shell->mouse_click_y < y) {
            c->title_login_field = 1;
        }
        y += 15;

        int buttonX = c->shell->screen_width / 2 - 80;
        int buttonY = c->shell->screen_height / 2 + 50;
        buttonY += 20;

        if (c->shell->mouse_click_button == 1 && c->shell->mouse_click_x >= buttonX - 75 && c->shell->mouse_click_x <= buttonX + 75 && c->shell->mouse_click_y >= buttonY - 20 && c->shell->mouse_click_y <= buttonY + 20) {
            client_login(c, c->username, c->password, false);
        }

        buttonX = c->shell->screen_width / 2 + 80;
        if (c->shell->mouse_click_button == 1 && c->shell->mouse_click_x >= buttonX - 75 && c->shell->mouse_click_x <= buttonX + 75 && c->shell->mouse_click_y >= buttonY - 20 && c->shell->mouse_click_y <= buttonY + 20) {
            c->title_screen_state = 0;
            if (!_Custom.remember_username) {
                c->username[0] = '\0';
            }
            if (!_Custom.remember_password) {
                c->password[0] = '\0';
            }
        }

        while (true) {
            while (true) {
                int key = poll_key(c->shell);
                if (key == -1) {
                    return;
                }

                bool valid = false;
                for (size_t i = 0; i < strlen(CHARSET); i++) {
                    if (key == CHARSET[i]) {
                        valid = true;
                        break;
                    }
                }

                if (c->title_login_field == 0) {
                    if (key == 8 && strlen(c->username) > 0) {
                        c->username[strlen(c->username) - 1] = '\0';
                    }

                    if (key == 9 || key == 10 || key == 13) {
                        c->title_login_field = 1;
                    }

                    if (valid) {
                        size_t len = strlen(c->username);
                        if (len < USERNAME_LENGTH) {
                            c->username[len] = (char)key;
                            c->username[len + 1] = '\0';
                        }
                    }
                } else if (c->title_login_field == 1) {
                    if (key == 8 && strlen(c->password) > 0) {
                        c->password[strlen(c->password) - 1] = '\0';
                    }

                    if (key == 9 || key == 10 || key == 13) {
                        c->title_login_field = 0;
                    }

                    if (valid) {
                        size_t len = strlen(c->password);
                        if (len < PASSWORD_LENGTH) {
                            c->password[len] = (char)key;
                            c->password[len + 1] = '\0';
                        }
                    }
                }
            }
        }
    } else if (c->title_screen_state == 3) {
        int x = c->shell->screen_width / 2;
        int y = c->shell->screen_height / 2 + 50;
        y += 20;

        if (c->shell->mouse_click_button == 1 && c->shell->mouse_click_x >= x - 75 && c->shell->mouse_click_x <= x + 75 && c->shell->mouse_click_y >= y - 20 && c->shell->mouse_click_y <= y + 20) {
            c->title_screen_state = 0;
        }
    }
}

void client_login(Client *c, const char *username, const char *password, bool reconnect) {
    // signlink.errorname = username;
    // try {
    if (!reconnect) {
        c->login_message0 = "";
        c->login_message1 = "Connecting to server...";
        client_draw_title_screen(c);
    }

#ifdef __EMSCRIPTEN__
    c->stream = clientstream_opensocket(_Custom.http_port);
#else
    c->stream = clientstream_opensocket(_Client.portoff + 43594);
#endif
    if (!c->stream) {
        goto login_fail;
    }
    clientstream_read_bytes(c->stream, c->in->data, 0, 8);
    c->in->pos = 0;

    c->server_seed = g8(c->in);
    int seed[] = {(int)(jrand() * 9.9999999E7), (int)(jrand() * 9.9999999E7), (int)(c->server_seed >> 32), (int)c->server_seed};

    c->out->pos = 0;
    p1(c->out, 10);
    p4(c->out, seed[0]);
    p4(c->out, seed[1]);
    p4(c->out, seed[2]);
    p4(c->out, seed[3]);
    p4(c->out, _Client.uid);
    pjstr(c->out, username);
    pjstr(c->out, password);
    rsaenc(c->out, _Client.rsa_modulus, _Client.rsa_exponent);

    c->login->pos = 0;
    if (reconnect) {
        p1(c->login, 18);
    } else {
        p1(c->login, 16);
    }

    p1(c->login, c->out->pos + 36 + 1 + 1);
    p1(c->login, _Client.clientversion);
    p1(c->login, _Client.lowmem ? 1 : 0);

    for (int i = 0; i < 9; i++) {
        p4(c->login, c->archive_checksum[i]);
    }
    pdata(c->login, c->out->data, c->out->pos, 0);

    memset(c->out->random.randrsl, 0,
           sizeof(c->out->random.randrsl));

    memset(c->random_in.randrsl, 0,
           sizeof(c->random_in.randrsl));

    memcpy(c->out->random.randrsl, seed, sizeof(seed));
    isaac_init(&c->out->random, 1);
    for (int i = 0; i < 4; i++) {
        seed[i] += 50;
    }
    memcpy(c->random_in.randrsl, seed, sizeof(seed));
    isaac_init(&c->random_in, 1);
    clientstream_write(c->stream, c->login->data, c->login->pos, 0);

    int reply = clientstream_read_byte(c->stream);
    if (reply == 1) {
        rs2_sleep(2000);
        client_login(c, username, password, reconnect);
    } else if (reply == 2 || reply == 18) {
        c->rights = reply == 18;
        inputtracking_set_disabled(&_InputTracking);

        c->ingame = true;
        c->out->pos = 0;
        c->in->pos = 0;
        c->packet_type = -1;
        c->last_packet_type0 = -1;
        c->last_packet_type1 = -1;
        c->last_packet_type2 = -1;
        c->packet_size = 0;
        c->idle_net_cycles = 0;
        c->system_update_timer = 0;
        c->idle_timeout = 0;
        c->hint_type = 0;
        c->menu_size = 0;
        c->menu_visible = false;
        c->shell->idle_cycles = 0;

        for (int i = 0; i < 100; i++) {
            c->message_text[i][0] = '\0';
        }

        c->obj_selected = 0;
        c->spell_selected = 0;
        c->scene_state = 0;
        c->wave_count = 0;

        c->camera_anticheat_offset_x = (int)(jrand() * 100.0) - 50;
        c->camera_anticheat_offset_z = (int)(jrand() * 110.0) - 55;
        c->camera_anticheat_angle = (int)(jrand() * 80.0) - 40;
        c->minimap_anticheat_angle = (int)(jrand() * 120.0) - 60;
        c->minimap_zoom = (int)(jrand() * 30.0) - 20;
        c->orbit_camera_yaw = (int)(jrand() * 20.0) - 10 & 0x7ff;

        c->minimap_level = -1;
        c->flag_scene_tile_x = 0;
        c->flag_scene_tile_z = 0;

        c->player_count = 0;
        c->npc_count = 0;

        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            free(c->players[i]);
            c->players[i] = NULL;
            if (c->player_appearance_buffer[i]) {
                packet_free(c->player_appearance_buffer[i]);
                c->player_appearance_buffer[i] = NULL;
            }
        }

        for (int i = 0; i < MAX_NPC_COUNT; i++) {
            if (c->npcs[i]) {
                free(c->npcs[i]);
                c->npcs[i] = NULL;
            }
        }

        c->local_player = c->players[LOCAL_PLAYER_INDEX] = playerentity_new();
        linklist_clear(c->projectiles);
        linklist_clear(c->spotanims);
        linklist_clear(c->merged_locations);
        for (int level = 0; level < 4; level++) {
            for (int x = 0; x < 104; x++) {
                for (int z = 0; z < 104; z++) {
                    if (c->level_obj_stacks[level][x][z]) {
                        linklist_free(c->level_obj_stacks[level][x][z]);
                        c->level_obj_stacks[level][x][z] = NULL;
                    }
                }
            }
        }

        // TODO: why not linklist_clear originally?
        linklist_free(c->spawned_locations);
        c->spawned_locations = linklist_new();
        c->friend_count = 0;
        c->sticky_chat_interface_id = -1;
        c->chat_interface_id = -1;
        c->viewport_interface_id = -1;
        c->sidebar_interface_id = -1;
        c->pressed_continue_option = false;
        c->selected_tab = 3;
        c->chatback_input_open = false;
        c->menu_visible = false;
        c->show_social_input = false;
        c->modal_message[0] = '\0';
        c->in_multizone = 0;
        c->flashing_tab = -1;
        c->design_gender_male = true;

        client_validate_character_design(c);
        for (int i = 0; i < 5; i++) {
            c->design_colors[i] = 0;
        }

        _Client.oplogic1 = 0;
        _Client.oplogic2 = 0;
        _Client.oplogic3 = 0;
        _Client.oplogic4 = 0;
        _Client.oplogic5 = 0;
        _Client.oplogic6 = 0;
        _Client.oplogic7 = 0;
        _Client.oplogic8 = 0;
        _Client.oplogic9 = 0;

        client_prepare_game_screen(c);
    } else if (reply == 3) {
        c->login_message0 = "";
        c->login_message1 = "Invalid username or password.";
    } else if (reply == 4) {
        c->login_message0 = "Your account has been disabled.";
        c->login_message1 = "Please check your message-centre for details.";
    } else if (reply == 5) {
        c->login_message0 = "Your account is already logged in.";
        c->login_message1 = "Try again in 60 secs...";
    } else if (reply == 6) {
        c->login_message0 = "RuneScape has been updated!";
        c->login_message1 = "Please reload this page.";
    } else if (reply == 7) {
        c->login_message0 = "This world is full.";
        c->login_message1 = "Please use a different world.";
    } else if (reply == 8) {
        c->login_message0 = "Unable to connect.";
        c->login_message1 = "Login server offline.";
    } else if (reply == 9) {
        c->login_message0 = "Login limit exceeded.";
        c->login_message1 = "Too many connections from your address.";
    } else if (reply == 10) {
        c->login_message0 = "Unable to connect.";
        c->login_message1 = "Bad session id.";
    } else if (reply == 11) {
        c->login_message0 = "Login server rejected session.";
        c->login_message1 = "Please try again.";
    } else if (reply == 12) {
        c->login_message0 = "You need a members account to login to this world.";
        c->login_message1 = "Please subscribe, or use a different world.";
    } else if (reply == 13) {
        c->login_message0 = "Could not complete login.";
        c->login_message1 = "Please try using a different world.";
    } else if (reply == 14) {
        c->login_message0 = "The server is being updated.";
        c->login_message1 = "Please wait 1 minute and try again.";
    } else if (reply == 15) {
        c->ingame = true;
        c->out->pos = 0;
        c->in->pos = 0;
        c->packet_type = -1;
        c->last_packet_type0 = -1;
        c->last_packet_type1 = -1;
        c->last_packet_type2 = -1;
        c->packet_size = 0;
        c->idle_net_cycles = 0;
        c->system_update_timer = 0;
        c->menu_size = 0;
        c->menu_visible = false;
    } else if (reply == 16) {
        c->login_message0 = "Login attempts exceeded.";
        c->login_message1 = "Please wait 1 minute and try again.";
    } else if (reply == 17) {
        c->login_message0 = "You are standing in a members-only area.";
        c->login_message1 = "To play on this world move to a free area first";
    }
    return;
login_fail:
    c->login_message0 = "";
    c->login_message1 = "Error connecting to server.";
}

void client_prepare_game_screen(Client *c) {
    if (c->area_chatback) {
        return;
    }

    client_unload_title(c);

    if (c->shell->draw_area) {
        pixmap_free(c->shell->draw_area);
        c->shell->draw_area = NULL;
    }
    pixmap_free(c->image_title0);
    pixmap_free(c->image_title1);
    pixmap_free(c->image_title2);
    pixmap_free(c->image_title3);
    pixmap_free(c->image_title4);
    pixmap_free(c->image_title5);
    pixmap_free(c->image_title6);
    pixmap_free(c->image_title7);
    pixmap_free(c->image_title8);
    c->image_title2 = NULL;
    c->image_title3 = NULL;
    c->image_title4 = NULL;
    c->image_title0 = NULL;
    c->image_title1 = NULL;
    c->image_title5 = NULL;
    c->image_title6 = NULL;
    c->image_title7 = NULL;
    c->image_title8 = NULL;
    c->area_chatback = pixmap_new(479, 96);
    c->area_mapback = pixmap_new(168, 160);
    pix2d_clear();
    pix8_draw(c->image_mapback, 0, 0);
    c->area_sidebar = pixmap_new(190, 261);
    c->area_viewport = pixmap_new(512, 334);
    pix2d_clear();
    c->area_backbase1 = pixmap_new(501, 61);
    c->area_backbase2 = pixmap_new(288, 40);
    c->area_backhmid1 = pixmap_new(269, 66);
    c->redraw_background = true;
}

void client_unload_title(Client *c) {
    c->flame_active = false;
    pix8_free(c->image_titlebox);
    pix8_free(c->image_titlebutton);
#ifndef DISABLE_FLAMES
    for (int i = 0; i < 12; i++) {
        pix8_free(c->image_runes[i]);
    }
    free(c->image_runes);
    free(c->flame_gradient0);
    free(c->flame_gradient1);
    free(c->flame_gradient2);
    free(c->flame_gradient);
    free(c->flame_buffer0);
    free(c->flame_buffer1);
    free(c->flame_buffer3);
    free(c->flame_buffer2);
    pix24_free(c->image_flames_left);
    pix24_free(c->image_flames_right);
#endif

    c->image_titlebox = NULL;
    c->image_titlebutton = NULL;
    for (int i = 0; i < 12; i++) {
        c->image_runes = NULL;
    }
    c->image_runes = NULL;
    c->flame_gradient = NULL;
    c->flame_gradient0 = NULL;
    c->flame_gradient1 = NULL;
    c->flame_gradient2 = NULL;
    c->flame_buffer0 = NULL;
    c->flame_buffer1 = NULL;
    c->flame_buffer3 = NULL;
    c->flame_buffer2 = NULL;
    c->image_flames_left = NULL;
    c->image_flames_right = NULL;
}

void client_validate_character_design(Client *c) {
    c->update_design_model = true;

    for (int i = 0; i < 7; i++) {
        c->designIdentikits[i] = -1;

        for (int j = 0; j < _IdkType.count; j++) {
            if (!_IdkType.instances[j]->disable && _IdkType.instances[j]->type == i + (c->design_gender_male ? 0 : 7)) {
                c->designIdentikits[i] = j;
                break;
            }
        }
    }
}

void client_draw(Client *c) {
    if (c->error_started || c->error_loading || c->error_host) {
        client_draw_error(c);
    } else {
        if (c->ingame) {
            client_draw_game(c);
        } else {
            client_draw_title_screen(c);
        }

        c->drag_cycles = 0;
    }
}

void client_draw_game(Client *c) {
    if (c->redraw_background) {
        c->redraw_background = false;
        pixmap_draw(c->area_backleft1, 0, 11);
        pixmap_draw(c->area_backleft2, 0, 375);
        pixmap_draw(c->area_backright1, 729, 5);
        pixmap_draw(c->area_backright2, 752, 231);
        pixmap_draw(c->area_backtop1, 0, 0);
        pixmap_draw(c->area_backtop2, 561, 0);
        pixmap_draw(c->area_backvmid1, 520, 11);
        pixmap_draw(c->area_backvmid2, 520, 231);
        pixmap_draw(c->area_backvmid3, 501, 375);
        pixmap_draw(c->area_backhmid2, 0, 345);
        c->redraw_sidebar = true;
        c->redraw_chatback = true;
        c->redraw_sideicons = true;
        c->redraw_privacy_settings = true;
        if (c->scene_state != 2) {
            pixmap_draw(c->area_viewport, 8, 11);
            pixmap_draw(c->area_mapback, 561, 5);
        }
    }

    if (c->scene_state == 2) {
        client_draw_scene(c);
    }

    if (c->menu_visible && c->menu_area == 1) {
        c->redraw_sidebar = true;
    }

    if (c->sidebar_interface_id != -1) {
        bool redraw = client_update_interface_animation(c, c->sidebar_interface_id, c->scene_delta);
        if (redraw) {
            c->redraw_sidebar = true;
        }
    }

    if (c->selected_area == 2) {
        c->redraw_sidebar = true;
    }

    if (c->obj_drag_area == 2) {
        c->redraw_sidebar = true;
    }

    if (c->redraw_sidebar) {
        client_draw_sidebar(c);
        c->redraw_sidebar = false;
    }

    if (c->chat_interface_id == -1) {
        c->chat_interface->scrollPosition = c->chat_scroll_height - c->chat_scroll_offset - 77;
        if (c->shell->mouse_x > 453 && c->shell->mouse_x < 565 && c->shell->mouse_y > 350) {
            client_handle_scroll_input(c, c->shell->mouse_x - 22, c->shell->mouse_y - 375, c->chat_scroll_height, 77, false, 463, 0, c->chat_interface);
        }

        int offset = c->chat_scroll_height - c->chat_interface->scrollPosition - 77;
        if (offset < 0) {
            offset = 0;
        }

        if (offset > c->chat_scroll_height - 77) {
            offset = c->chat_scroll_height - 77;
        }

        if (c->chat_scroll_offset != offset) {
            c->chat_scroll_offset = offset;
            c->redraw_chatback = true;
        }
    }

    if (c->chat_interface_id != -1) {
        bool redraw = client_update_interface_animation(c, c->chat_interface_id, c->scene_delta);
        if (redraw) {
            c->redraw_chatback = true;
        }
    }

    if (c->selected_area == 3) {
        c->redraw_chatback = true;
    }

    if (c->obj_drag_area == 3) {
        c->redraw_chatback = true;
    }

    if (c->modal_message[0]) {
        c->redraw_chatback = true;
    }

    if (c->menu_visible && c->menu_area == 2) {
        c->redraw_chatback = true;
    }

    if (c->redraw_chatback) {
        client_draw_chatback(c);
        c->redraw_chatback = false;
    }

    if (c->scene_state == 2) {
        client_draw_minimap(c);
        pixmap_draw(c->area_mapback, 561, 5);
    }

    if (c->flashing_tab != -1) {
        c->redraw_sideicons = true;
    }

    if (c->redraw_sideicons) {
        if (c->flashing_tab != -1 && c->flashing_tab == c->selected_tab) {
            c->flashing_tab = -1;
            // TUTORIAL_CLICKSIDE
            p1isaac(c->out, 175);
            p1(c->out, c->selected_tab);
        }

        c->redraw_sideicons = false;
        pixmap_bind(c->area_backhmid1);
        pix8_draw(c->image_backhmid1, 0, 0);

        if (c->sidebar_interface_id == -1) {
            if (c->tab_interface_id[c->selected_tab] != -1) {
                if (c->selected_tab == 0) {
                    pix8_draw(c->image_redstone1, 29, 30);
                } else if (c->selected_tab == 1) {
                    pix8_draw(c->image_redstone2, 59, 29);
                } else if (c->selected_tab == 2) {
                    pix8_draw(c->image_redstone2, 87, 29);
                } else if (c->selected_tab == 3) {
                    pix8_draw(c->image_redstone3, 115, 29);
                } else if (c->selected_tab == 4) {
                    pix8_draw(c->image_redstone2h, 156, 29);
                } else if (c->selected_tab == 5) {
                    pix8_draw(c->image_redstone2h, 184, 29);
                } else if (c->selected_tab == 6) {
                    pix8_draw(c->image_redstone1h, 212, 30);
                }
            }

            if (c->tab_interface_id[0] != -1 && (c->flashing_tab != 0 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[0], 35, 34);
            }

            if (c->tab_interface_id[1] != -1 && (c->flashing_tab != 1 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[1], 59, 32);
            }

            if (c->tab_interface_id[2] != -1 && (c->flashing_tab != 2 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[2], 86, 32);
            }

            if (c->tab_interface_id[3] != -1 && (c->flashing_tab != 3 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[3], 121, 33);
            }

            if (c->tab_interface_id[4] != -1 && (c->flashing_tab != 4 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[4], 157, 34);
            }

            if (c->tab_interface_id[5] != -1 && (c->flashing_tab != 5 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[5], 185, 32);
            }

            if (c->tab_interface_id[6] != -1 && (c->flashing_tab != 6 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[6], 212, 34);
            }
        }

        pixmap_draw(c->area_backhmid1, 520, 165);
        pixmap_bind(c->area_backbase2);
        pix8_draw(c->image_backbase2, 0, 0);

        if (c->sidebar_interface_id == -1) {
            if (c->tab_interface_id[c->selected_tab] != -1) {
                if (c->selected_tab == 7) {
                    pix8_draw(c->image_redstone1v, 49, 0);
                } else if (c->selected_tab == 8) {
                    pix8_draw(c->image_redstone2v, 81, 0);
                } else if (c->selected_tab == 9) {
                    pix8_draw(c->image_redstone2v, 108, 0);
                } else if (c->selected_tab == 10) {
                    pix8_draw(c->image_redstone3v, 136, 1);
                } else if (c->selected_tab == 11) {
                    pix8_draw(c->image_redstone2hv, 178, 0);
                } else if (c->selected_tab == 12) {
                    pix8_draw(c->image_redstone2hv, 205, 0);
                } else if (c->selected_tab == 13) {
                    pix8_draw(c->image_redstone1hv, 233, 0);
                }
            }

            if (c->tab_interface_id[8] != -1 && (c->flashing_tab != 8 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[7], 80, 2);
            }

            if (c->tab_interface_id[9] != -1 && (c->flashing_tab != 9 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[8], 107, 3);
            }

            if (c->tab_interface_id[10] != -1 && (c->flashing_tab != 10 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[9], 142, 4);
            }

            if (c->tab_interface_id[11] != -1 && (c->flashing_tab != 11 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[10], 179, 2);
            }

            if (c->tab_interface_id[12] != -1 && (c->flashing_tab != 12 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[11], 206, 2);
            }

            if (c->tab_interface_id[13] != -1 && (c->flashing_tab != 13 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[12], 230, 2);
            }
        }
        pixmap_draw(c->area_backbase2, 501, 492);
        pixmap_bind(c->area_viewport);
    }

    if (c->redraw_privacy_settings) {
        c->redraw_privacy_settings = false;
        pixmap_bind(c->area_backbase1);
        pix8_draw(c->image_backbase1, 0, 0);

        drawStringTaggableCenter(c->font_plain12, "Public chat", 57, 33, WHITE, true);
        if (c->public_chat_setting == 0) {
            drawStringTaggableCenter(c->font_plain12, "On", 57, 46, GREEN, true);
        }
        if (c->public_chat_setting == 1) {
            drawStringTaggableCenter(c->font_plain12, "Friends", 57, 46, YELLOW, true);
        }
        if (c->public_chat_setting == 2) {
            drawStringTaggableCenter(c->font_plain12, "Off", 57, 46, RED, true);
        }
        if (c->public_chat_setting == 3) {
            drawStringTaggableCenter(c->font_plain12, "Hide", 57, 46, CYAN, true);
        }

        drawStringTaggableCenter(c->font_plain12, "Private chat", 186, 33, WHITE, true);
        if (c->private_chat_setting == 0) {
            drawStringTaggableCenter(c->font_plain12, "On", 186, 46, GREEN, true);
        }
        if (c->private_chat_setting == 1) {
            drawStringTaggableCenter(c->font_plain12, "Friends", 186, 46, YELLOW, true);
        }
        if (c->private_chat_setting == 2) {
            drawStringTaggableCenter(c->font_plain12, "Off", 186, 46, RED, true);
        }

        drawStringTaggableCenter(c->font_plain12, "Trade/duel", 326, 33, WHITE, true);
        if (c->trade_chat_setting == 0) {
            drawStringTaggableCenter(c->font_plain12, "On", 326, 46, GREEN, true);
        }
        if (c->trade_chat_setting == 1) {
            drawStringTaggableCenter(c->font_plain12, "Friends", 326, 46, YELLOW, true);
        }
        if (c->trade_chat_setting == 2) {
            drawStringTaggableCenter(c->font_plain12, "Off", 326, 46, RED, true);
        }

        drawStringTaggableCenter(c->font_plain12, "Report abuse", 462, 38, WHITE, true);
        pixmap_draw(c->area_backbase1, 0, 471);
        pixmap_bind(c->area_viewport);
    }

    c->scene_delta = 0;
}

void client_handle_scroll_input(Client *c, int mouseX, int mouseY, int scrollableHeight, int height, bool redraw, int left, int top, Component *component) {
    if (c->scrollGrabbed) {
        c->scrollInputPadding = 32;
    } else {
        c->scrollInputPadding = 0;
    }

    c->scrollGrabbed = false;

    if (mouseX >= left && mouseX < left + 16 && mouseY >= top && mouseY < top + 16) {
        component->scrollPosition -= c->drag_cycles * 4;
        if (redraw) {
            c->redraw_sidebar = true;
        }
    } else if (mouseX >= left && mouseX < left + 16 && mouseY >= top + height - 16 && mouseY < top + height) {
        component->scrollPosition += c->drag_cycles * 4;
        if (redraw) {
            c->redraw_sidebar = true;
        }
    } else if (mouseX >= left - c->scrollInputPadding && mouseX < left + c->scrollInputPadding + 16 && mouseY >= top + 16 && mouseY < top + height - 16 && c->drag_cycles > 0) {
        int gripSize = (height - 32) * height / scrollableHeight;
        if (gripSize < 8) {
            gripSize = 8;
        }
        int gripY = mouseY - top - gripSize / 2 - 16;
        int maxY = height - gripSize - 32;
        component->scrollPosition = (scrollableHeight - height) * gripY / maxY;
        if (redraw) {
            c->redraw_sidebar = true;
        }
        c->scrollGrabbed = true;
    }
}

bool client_update_interface_animation(Client *c, int id, int delta) {
    bool updated = false;
    Component *parent = _Component.instances[id];
    for (int i = 0; i < parent->childCount && parent->childId[i] != -1; i++) {
        Component *child = _Component.instances[parent->childId[i]];
        if (child->type == 1) {
            updated |= client_update_interface_animation(c, child->id, delta);
        }
        if (child->type == 6 && (child->anim != -1 || child->activeAnim != -1)) {
            bool active = client_execute_interface_script(c, child);
            int seqId;
            if (active) {
                seqId = child->activeAnim;
            } else {
                seqId = child->anim;
            }
            if (seqId != -1) {
                SeqType *type = _SeqType.instances[seqId];
                child->seqCycle += delta;
                while (child->seqCycle > type->delay[child->seqFrame]) {
                    child->seqCycle -= type->delay[child->seqFrame] + 1;
                    child->seqFrame++;
                    if (child->seqFrame >= type->frameCount) {
                        child->seqFrame -= type->replayoff;
                        if (child->seqFrame < 0 || child->seqFrame >= type->frameCount) {
                            child->seqFrame = 0;
                        }
                    }
                    updated = true;
                }
            }
        }
    }
    return updated;
}

bool client_execute_interface_script(Client *c, Component *com) {
    if (!com->scriptComparator) {
        return false;
    }

    for (int i = 0; i < com->comparatorCount; i++) {
        int value = client_execute_clientscript1(c, com, i);
        int operand = com->scriptOperand[i];

        if (com->scriptComparator[i] == 2) {
            if (value >= operand) {
                return false;
            }
        } else if (com->scriptComparator[i] == 3) {
            if (value <= operand) {
                return false;
            }
        } else if (com->scriptComparator[i] == 4) {
            if (value == operand) {
                return false;
            }
        } else if (value != operand) {
            return false;
        }
    }

    return true;
}

int client_execute_clientscript1(Client *c, Component *component, int scriptId) {
    if (!component->scripts || scriptId >= component->scriptCount) {
        return -2;
    }

    // try {
    int *script = component->scripts[scriptId];
    int _register = 0;
    int pc = 0;

    while (true) {
        int opcode = script[pc++];
        if (opcode == 0) {
            return _register;
        }

        if (opcode == 1) { // load_skill_level {skill}
            _register += c->skillLevel[script[pc++]];
        } else if (opcode == 2) { // load_skill_base_level {skill}
            _register += c->skillBaseLevel[script[pc++]];
        } else if (opcode == 3) { // load_skill_exp {skill}
            _register += c->skillExperience[script[pc++]];
        } else if (opcode == 4) { // load_inv_count {interface id} {obj id}
            Component *com = _Component.instances[script[pc++]];
            int obj = script[pc++] + 1;

            for (int i = 0; i < com->width * com->height; i++) {
                if (com->invSlotObjId[i] == obj) {
                    _register += com->invSlotObjCount[i];
                }
            }
        } else if (opcode == 5) { // load_var {id}
            _register += c->varps[script[pc++]];
        } else if (opcode == 6) { // load_next_level_xp {skill}
            _register += _Client.levelExperience[c->skillBaseLevel[script[pc++]] - 1];
        } else if (opcode == 7) {
            _register += c->varps[script[pc++]] * 100 / 46875;
        } else if (opcode == 8) { // load_combat_level
            _register += c->local_player->combatLevel;
        } else if (opcode == 9) { // load_total_level
            for (int i = 0; i < 19; i++) {
                if (i == 18) {
                    // runecrafting
                    i = 20;
                }

                _register += c->skillBaseLevel[i];
            }
        } else if (opcode == 10) { // load_inv_contains {interface id} {obj id}
            Component *com = _Component.instances[script[pc++]];
            int obj = script[pc++] + 1;

            for (int i = 0; i < com->width * com->height; i++) {
                if (com->invSlotObjId[i] == obj) {
                    _register += 999999999;
                    break;
                }
            }
        } else if (opcode == 11) { // load_energy
            _register += c->energy;
        } else if (opcode == 12) { // load_weight
            _register += c->weightCarried;
        } else if (opcode == 13) { // load_bool {varp} {bit: 0..31}
            int varp = c->varps[script[pc++]];
            int lsb = script[pc++];

            _register += (varp & 0x1 << lsb) == 0 ? 0 : 1;
        }
    }
    // } catch (@Pc(282) Exception ex) {
    // 	return -1;
    // }
}

void projectFromGround(Client *c, PathingEntity *entity, int height) {
    projectFromGround2(c, entity->x, height, entity->z);
}

void projectFromGround2(Client *c, int x, int height, int z) {
    if (x < 128 || z < 128 || x > 13056 || z > 13056) {
        c->projectX = -1;
        c->projectY = -1;
        return;
    }

    int y = getHeightmapY(c, c->currentLevel, x, z) - height;
    project(c, x, y, z);
}

void project(Client *c, int x, int y, int z) {
    int dx = x - c->cameraX;
    int dy = y - c->cameraY;
    int dz = z - c->cameraZ;

    int sinPitch = _Pix3D.sin_table[c->cameraPitch];
    int cosPitch = _Pix3D.cos_table[c->cameraPitch];
    int sinYaw = _Pix3D.sin_table[c->cameraYaw];
    int cosYaw = _Pix3D.cos_table[c->cameraYaw];

    int tmp = (dz * sinYaw + dx * cosYaw) >> 16;
    dz = (dz * cosYaw - dx * sinYaw) >> 16;
    dx = tmp;

    tmp = (dy * cosPitch - dz * sinPitch) >> 16;
    dz = (dy * sinPitch + dz * cosPitch) >> 16;
    dy = tmp;

    if (dz >= 50) {
        c->projectX = _Pix3D.center_x + (dx << 9) / dz;
        c->projectY = _Pix3D.center_y + (dy << 9) / dz;
    } else {
        c->projectX = -1;
        c->projectY = -1;
    }
}

static void draw2DEntityElements(Client *c) {
    c->chatCount = 0;

    for (int index = -1; index < c->player_count + c->npc_count; index++) {
        PathingEntity *entity;
        if (index == -1) {
            entity = &c->local_player->pathing_entity;
        } else if (index < c->player_count) {
            entity = &c->players[c->player_ids[index]]->pathing_entity;
        } else {
            entity = &c->npcs[c->npc_ids[index - c->player_count]]->pathing_entity;
        }

        if (!entity || !pathingentity_is_visible(entity)) {
            continue;
        }

        // TODO
        // if (c->showDebug) {
        // 	// true tile overlay
        // 	if (entity.pathLength > 0 || entity.forceMoveEndCycle >= loopCycle || entity.forceMoveStartCycle > loopCycle) {
        // 		int halfUnit = 64 * entity.size;
        // 		c->drawTileOverlay(entity.pathTileX[0] * 128 + halfUnit, entity.pathTileZ[0] * 128 + halfUnit, c->currentLevel, entity.size, 0x00FFFF, false);
        // 	}

        // 	// local tile overlay
        // 	c->drawTileOverlay(entity.x, entity.z, c->currentLevel, entity.size, 0x666666, false);

        // 	int offsetY = 0;
        // 	c->projectFromGround(entity, entity.height + 30);

        // 	if (index < c->playerCount) {
        // 		// player debug
        // 		PlayerEntity player = (PlayerEntity) entity;

        // 		c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, player.name, 0xffffff);
        // 		offsetY -= 15;

        // 		if (player.lastMask != -1 && loopCycle - player.lastMaskCycle < 30) {
        // 			if ((player.lastMask & 0x1) == 0x1) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Appearance Update", 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x2) == 0x2) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Play Seq: " + player.primarySeqId, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x4) == 0x4) {
        // 				int target = player.targetId;
        // 				if (target > 32767) {
        // 					target -= 32768;
        // 				}
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Face Entity: " + target, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x8) == 0x8) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Say", 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x10) == 0x10) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Hit: Type " + player.damageType + " Amount " + player.damage + " HP " + player.health + "/" + player.totalHealth, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x20) == 0x20) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Face Coord: " + (player.lastFaceX / 2) + " " + (player.lastFaceZ / 2), 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x40) == 0x40) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Chat", 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x100) == 0x100) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Play Spotanim: " + player.spotanimId, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x200) == 0x200) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Exact Move", 0xffffff);
        // 				offsetY -= 15;
        // 			}
        // 		}
        // 	} else {
        // 		// npc debug
        // 		NpcEntity npc = (NpcEntity) entity;

        // 		c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, npc.type.name, 0xffffff);
        // 		offsetY -= 15;

        // 		if (npc.lastMask != -1 && loopCycle - npc.lastMaskCycle < 30) {
        // 			if ((npc.lastMask & 0x2) == 0x2) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Play Seq: " + npc.primarySeqId, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((npc.lastMask & 0x4) == 0x4) {
        // 				int target = npc.targetId;
        // 				if (target > 32767) {
        // 					target -= 32768;
        // 				}
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Face Entity: " + target, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((npc.lastMask & 0x8) == 0x8) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Say", 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((npc.lastMask & 0x10) == 0x10) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Hit: Type " + npc.damageType + " Amount " + npc.damage + " HP " + npc.health + "/" + npc.totalHealth, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((npc.lastMask & 0x20) == 0x20) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Change Type: " + npc.type.index, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((npc.lastMask & 0x40) == 0x40) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Play Spotanim: " + npc.spotanimId, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((npc.lastMask & 0x80) == 0x80) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Face Coord: " + (npc.lastFaceX / 2) + " " + (npc.lastFaceZ / 2), 0xffffff);
        // 				offsetY -= 15;
        // 			}
        // 		}
        // 	}
        // }

        if (index < c->player_count) {
            int y = 30;

            PlayerEntity *player = (PlayerEntity *)entity;
            if (player->headicons != 0) {
                projectFromGround(c, entity, entity->height + 15);

                if (c->projectX > -1) {
                    for (int icon = 0; icon < 8; icon++) {
                        if ((player->headicons & 0x1 << icon) != 0) {
                            pix24_draw(c->image_headicons[icon], c->projectX - 12, c->projectY - y);
                            y -= 25;
                        }
                    }
                }
            }

            if (index >= 0 && c->hint_type == 10 && c->hint_player == c->player_ids[index]) {
                projectFromGround(c, entity, entity->height + 15);
                if (c->projectX > -1) {
                    pix24_draw(c->image_headicons[7], c->projectX - 12, c->projectY - y);
                }
            }
        } else if (c->hint_type == 1 && c->hint_npc == c->npc_ids[index - c->player_count] && _Client.loop_cycle % 20 < 10) {
            projectFromGround(c, entity, entity->height + 15);
            if (c->projectX > -1) {
                pix24_draw(c->image_headicons[2], c->projectX - 12, c->projectY - 28);
            }
        }

        if (entity->chat[0] && (index >= c->player_count || c->public_chat_setting == 0 || c->public_chat_setting == 3 || (c->public_chat_setting == 1 && client_is_friend(c, ((PlayerEntity *)entity)->name)))) {
            projectFromGround(c, entity, entity->height);

            if (c->projectX > -1 && c->chatCount < MAX_CHATS) {
                c->chatWidth[c->chatCount] = stringWidth(c->font_bold12, entity->chat) / 2;
                c->chatHeight[c->chatCount] = c->font_bold12->height;
                c->chatX[c->chatCount] = c->projectX;
                c->chatY[c->chatCount] = c->projectY;

                c->chatColors[c->chatCount] = entity->chatColor;
                c->chatStyles[c->chatCount] = entity->chatStyle;
                c->chatTimers[c->chatCount] = entity->chatTimer;
                strcpy(c->chats[c->chatCount++], entity->chat);

                if (c->chatEffects == 0 && entity->chatStyle == 1) {
                    c->chatHeight[c->chatCount] += 10;
                    c->chatY[c->chatCount] += 5;
                }

                if (c->chatEffects == 0 && entity->chatStyle == 2) {
                    c->chatWidth[c->chatCount] = 60;
                }
            }
        }

        if (entity->combatCycle > _Client.loop_cycle + 100) {
            projectFromGround(c, entity, entity->height + 15);

            if (c->projectX > -1) {
                int w = entity->health * 30 / entity->totalHealth;
                if (w > 30) {
                    w = 30;
                }
                pix2d_fill_rect(c->projectX - 15, c->projectY - 3, GREEN, w, 5);
                pix2d_fill_rect(c->projectX - 15 + w, c->projectY - 3, RED, 30 - w, 5);
            }
        }

        if (entity->combatCycle > _Client.loop_cycle + 330) {
            projectFromGround(c, entity, entity->height / 2);

            if (c->projectX > -1) {
                pix24_draw(c->image_hitmarks[entity->damageType], c->projectX - 12, c->projectY - 12);
                char *damage = valueof(entity->damage);
                drawStringCenter(c->font_plain11, c->projectX, c->projectY + 4, damage, BLACK);
                drawStringCenter(c->font_plain11, c->projectX - 1, c->projectY + 3, damage, WHITE);
                free(damage);
            }
        }
    }

    // TODO
    // if (c->showDebug) {
    // 	for (int i = 0; i < c->userTileMarkers.length; i++) {
    // 		if (c->userTileMarkers[i] == null || c->userTileMarkers[i].level != c->currentLevel || c->userTileMarkers[i].x < 0 || c->userTileMarkers[i].z < 0 || c->userTileMarkers[i].x >= 104 || c->userTileMarkers[i].z >= 104) {
    // 			continue;
    // 		}

    // 		c->drawTileOverlay(c->userTileMarkers[i].x * 128 + 64, c->userTileMarkers[i].z * 128 + 64, c->userTileMarkers[i].level, 1, 0xffff00, false);
    // 	}
    // }

    for (int i = 0; i < c->chatCount; i++) {
        int x = c->chatX[i];
        int y = c->chatY[i];
        int padding = c->chatWidth[i];
        int height = c->chatHeight[i];
        bool sorting = true;
        while (sorting) {
            sorting = false;
            for (int j = 0; j < i; j++) {
                if (y + 2 > c->chatY[j] - c->chatHeight[j] && y - height < c->chatY[j] + 2 && x - padding < c->chatX[j] + c->chatWidth[j] && x + padding > c->chatX[j] - c->chatWidth[j] && c->chatY[j] - c->chatHeight[j] < y) {
                    y = c->chatY[j] - c->chatHeight[j];
                    sorting = true;
                }
            }
        }
        c->projectX = c->chatX[i];
        c->projectY = c->chatY[i] = y;
        const char *message = c->chats[i];
        if (c->chatEffects == 0) {
            int color = YELLOW;
            if (c->chatColors[i] < 6) {
                color = CHAT_COLORS[c->chatColors[i]];
            }
            if (c->chatColors[i] == 6) {
                color = c->scene_cycle % 20 < 10 ? RED : YELLOW;
            }
            if (c->chatColors[i] == 7) {
                color = c->scene_cycle % 20 < 10 ? BLUE : CYAN;
            }
            if (c->chatColors[i] == 8) {
                color = c->scene_cycle % 20 < 10 ? 0xb000 : 0x80ff80;
            }
            if (c->chatColors[i] == 9) {
                int delta = 150 - c->chatTimers[i];
                if (delta < 50) {
                    color = delta * 1280 + RED;
                } else if (delta < 100) {
                    color = YELLOW - (delta - 50) * 327680;
                } else if (delta < 150) {
                    color = (delta - 100) * 5 + GREEN;
                }
            }
            if (c->chatColors[i] == 10) {
                int delta = 150 - c->chatTimers[i];
                if (delta < 50) {
                    color = delta * 5 + RED;
                } else if (delta < 100) {
                    color = MAGENTA - (delta - 50) * 327680;
                } else if (delta < 150) {
                    color = (delta - 100) * 327680 + BLUE - (delta - 100) * 5;
                }
            }
            if (c->chatColors[i] == 11) {
                int delta = 150 - c->chatTimers[i];
                if (delta < 50) {
                    color = WHITE - delta * 327685;
                } else if (delta < 100) {
                    color = (delta - 50) * 327685 + GREEN;
                } else if (delta < 150) {
                    color = WHITE - (delta - 100) * 327680;
                }
            }
            if (c->chatStyles[i] == 0) {
                drawStringCenter(c->font_bold12, c->projectX, c->projectY + 1, message, BLACK);
                drawStringCenter(c->font_bold12, c->projectX, c->projectY, message, color);
            }
            if (c->chatStyles[i] == 1) {
                drawCenteredWave(c->font_bold12, c->projectX, c->projectY + 1, message, BLACK, c->scene_cycle);
                drawCenteredWave(c->font_bold12, c->projectX, c->projectY, message, color, c->scene_cycle);
            }
            if (c->chatStyles[i] == 2) {
                int w = stringWidth(c->font_bold12, message);
                int offsetX = (150 - c->chatTimers[i]) * (w + 100) / 150;
                pix2d_set_clipping(334, c->projectX + 50, 0, c->projectX - 50);
                drawString(c->font_bold12, c->projectX + 50 - offsetX, c->projectY + 1, message, BLACK);
                drawString(c->font_bold12, c->projectX + 50 - offsetX, c->projectY, message, color);
                pix2d_reset_clipping();
            }
        } else {
            drawStringCenter(c->font_bold12, c->projectX, c->projectY + 1, message, BLACK);
            drawStringCenter(c->font_bold12, c->projectX, c->projectY, message, YELLOW);
        }
    }
}

static void drawTileHint(Client *c) {
    if (c->hint_type != 2) {
        return;
    }

    projectFromGround2(c, ((c->hint_tile_x - c->sceneBaseTileX) << 7) + c->hint_offset_x, c->hint_height * 2, ((c->hint_tile_z - c->sceneBaseTileZ) << 7) + c->hint_offset_z);

    if (c->projectX > -1 && _Client.loop_cycle % 20 < 10) {
        pix24_draw(c->image_headicons[2], c->projectX - 12, c->projectY - 28);
    }
}

static void updateTextures(Client *c, int cycle) {
    if (!_Client.lowmem) {
        if (_Pix3D.textureCycle[17] >= cycle) {
            Pix8 *texture = _Pix3D.textures[17];
            int bottom = texture->width * texture->height - 1;
            int adjustment = texture->width * c->scene_delta * 2;

            int8_t *src = texture->pixels;
            int8_t *dst = c->textureBuffer;
            for (int i = 0; i <= bottom; i++) {
                dst[i] = src[i - adjustment & bottom];
            }

            texture->pixels = dst;
            c->textureBuffer = src;
            pix3d_push_texture(17);
        }

        if (_Pix3D.textureCycle[24] >= cycle) {
            Pix8 *texture = _Pix3D.textures[24];
            int bottom = texture->width * texture->height - 1;
            int adjustment = texture->width * c->scene_delta * 2;

            int8_t *src = texture->pixels;
            int8_t *dst = c->textureBuffer;
            for (int i = 0; i <= bottom; i++) {
                dst[i] = src[i - adjustment & bottom];
            }

            texture->pixels = dst;
            c->textureBuffer = src;
            pix3d_push_texture(24);
        }
    }
}

static void drawPrivateMessages(Client *c) {
    if (c->split_private_chat == 0) {
        return;
    }

    PixFont *font = c->font_plain12;
    int lineOffset = 0;
    if (c->system_update_timer != 0) {
        lineOffset = 1;
    }

    for (int i = 0; i < 100; i++) {
        if (c->message_text[i][0] == '\0') {
            continue;
        }

        int type = c->message_type[i];
        int y;
        char buf[USERNAME_LENGTH + CHAT_LENGTH + 8];
        if ((type == 3 || type == 7) && (type == 7 || c->private_chat_setting == 0 || (c->private_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
            y = 329 - lineOffset * 13;
            sprintf(buf, "From %s: %s", c->message_sender[i], c->message_text[i]);
            drawString(font, 4, y, buf, BLACK);
            sprintf(buf, "From %s: %s", c->message_sender[i], c->message_text[i]);
            drawString(font, 4, y - 1, buf, CYAN);

            lineOffset++;
            if (lineOffset >= 5) {
                return;
            }
        }

        if (type == 5 && c->private_chat_setting < 2) {
            y = 329 - lineOffset * 13;
            drawString(font, 4, y, c->message_text[i], BLACK);
            drawString(font, 4, y - 1, c->message_text[i], CYAN);

            lineOffset++;
            if (lineOffset >= 5) {
                return;
            }
        }

        if (type == 6 && c->private_chat_setting < 2) {
            y = 329 - lineOffset * 13;
            sprintf(buf, "To %s: %s", c->message_sender[i], c->message_text[i]);
            drawString(font, 4, y, buf, BLACK);
            sprintf(buf, "To %s: %s", c->message_sender[i], c->message_text[i]);
            drawString(font, 4, y - 1, buf, CYAN);

            lineOffset++;
            if (lineOffset >= 5) {
                return;
            }
        }
    }
}

static void drawWildyLevel(Client *c) {
    int x = (c->local_player->pathing_entity.x >> 7) + c->sceneBaseTileX;
    int z = (c->local_player->pathing_entity.z >> 7) + c->sceneBaseTileZ;

    if (x >= 2944 && x < 3392 && z >= 3520 && z < 6400) {
        c->wildernessLevel = (z - 3520) / 8 + 1;
    } else if (x >= 2944 && x < 3392 && z >= 9920 && z < 12800) {
        c->wildernessLevel = (z - 9920) / 8 + 1;
    } else {
        c->wildernessLevel = 0;
    }

    c->worldLocationState = 0;
    if (x >= 3328 && x < 3392 && z >= 3200 && z < 3264) {
        int localX = x & 63;
        int localZ = z & 63;

        if (localX >= 4 && localX <= 29 && localZ >= 44 && localZ <= 58) {
            c->worldLocationState = 1;
        } else if (localX >= 36 && localX <= 61 && localZ >= 44 && localZ <= 58) {
            c->worldLocationState = 1;
        } else if (localX >= 4 && localX <= 29 && localZ >= 25 && localZ <= 39) {
            c->worldLocationState = 1;
        } else if (localX >= 36 && localX <= 61 && localZ >= 25 && localZ <= 39) {
            c->worldLocationState = 1;
        } else if (localX >= 4 && localX <= 29 && localZ >= 6 && localZ <= 20) {
            c->worldLocationState = 1;
        } else if (localX >= 36 && localX <= 61 && localZ >= 6 && localZ <= 20) {
            c->worldLocationState = 1;
        }
    }

    if (c->worldLocationState == 0 && x >= 3328 && x <= 3393 && z >= 3203 && z <= 3325) {
        c->worldLocationState = 2;
    }

    c->overrideChat = 0;
    if (x >= 3053 && x <= 3156 && z >= 3056 && z <= 3136) {
        c->overrideChat = 1;
    } else if (x >= 3072 && x <= 3118 && z >= 9492 && z <= 9535) {
        c->overrideChat = 1;
    }

    if (c->overrideChat == 1 && x >= 3139 && x <= 3199 && z >= 3008 && z <= 3062) {
        c->overrideChat = 0;
    }
}

static void drawTooltip(Client *c) {
    if (c->menu_size < 2 && c->obj_selected == 0 && c->spell_selected == 0) {
        return;
    }

    char tooltip[DOUBLE_STR];
    if (c->obj_selected == 1 && c->menu_size < 2) {
        sprintf(tooltip, "Use %s with...", c->objSelectedName);
    } else if (c->spell_selected == 1 && c->menu_size < 2) {
        sprintf(tooltip, "%s...", c->spellCaption);
    } else {
        strcpy(tooltip, c->menu_option[c->menu_size - 1]);
    }

    if (c->menu_size > 2) {
        char tmp[MAX_STR];
        strcpy(tmp, tooltip);
        sprintf(tooltip, "%s@whi@ / %d more options", tmp, (c->menu_size - 2));
    }

    drawStringTooltip(c->font_bold12, 4, 15, tooltip, WHITE, true, _Client.loop_cycle / 1000);
}

static void draw3DEntityElements(Client *c) {
    drawPrivateMessages(c);
    if (c->cross_mode == 1) {
        pix24_draw(c->image_crosses[c->cross_cycle / 100], c->crossX - 8 - 8, c->crossY - 8 - 11);
    }

    if (c->cross_mode == 2) {
        pix24_draw(c->image_crosses[c->cross_cycle / 100 + 4], c->crossX - 8 - 8, c->crossY - 8 - 11);
    }

    if (c->viewport_interface_id != -1) {
        client_update_interface_animation(c, c->viewport_interface_id, c->scene_delta);
        client_draw_interface(c, _Component.instances[c->viewport_interface_id], 0, 0, 0);
    }

    drawWildyLevel(c);

    if (!c->menu_visible) {
        client_handle_input(c);
        drawTooltip(c);
    } else if (c->menu_area == 0) {
        client_draw_menu(c);
    }

    if (c->in_multizone == 1) {
        if (c->wildernessLevel > 0 || c->worldLocationState == 1) {
            pix24_draw(c->image_headicons[1], 472, 258);
        } else {
            pix24_draw(c->image_headicons[1], 472, 296);
        }
    }

    char buf[HALF_STR];
    if (c->wildernessLevel > 0) {
        pix24_draw(c->image_headicons[0], 472, 296);
        sprintf(buf, "Level: %d", c->wildernessLevel);
        drawStringCenter(c->font_plain12, 484, 329, buf, YELLOW);
    }

    if (c->worldLocationState == 1) {
        pix24_draw(c->image_headicons[6], 472, 296);
        drawStringCenter(c->font_plain12, 484, 329, "Arena", YELLOW);
    }

    if (c->system_update_timer != 0) {
        int seconds = c->system_update_timer / 50;
        int minutes = seconds / 60;
        seconds %= 60;

        if (seconds < 10) {
            sprintf(buf, "System update in: %d:0%d", minutes, seconds);
            drawString(c->font_plain12, 4, 329, buf, YELLOW);
        } else {
            sprintf(buf, "System update in: %d:%d", minutes, seconds);
            drawString(c->font_plain12, 4, 329, buf, YELLOW);
        }
    }

    draw_info_overlay(c);
}

void client_draw_scene(Client *c) {
    c->scene_cycle++;
    pushPlayers(c);
    pushNpcs(c);
    pushProjectiles(c);
    pushSpotanims(c);
    // TODO re-add
    // pushLocs(c);

    if (!c->cutscene) {
        int pitch = c->orbit_camera_pitch;

        if (c->cameraPitchClamp / 256 > pitch) {
            pitch = c->cameraPitchClamp / 256;
        }

        if (c->cameraModifierEnabled[4] && c->cameraModifierWobbleScale[4] + 128 > pitch) {
            pitch = c->cameraModifierWobbleScale[4] + 128;
        }

        int yaw = c->orbit_camera_yaw + c->camera_anticheat_angle & 0x7ff;
        orbitCamera(c, c->orbitCameraX, getHeightmapY(c, c->currentLevel, c->local_player->pathing_entity.x, c->local_player->pathing_entity.z) - 50, c->orbitCameraZ, yaw, pitch, pitch * 3 + 600);

        _Client.cyclelogic2++;
        if (_Client.cyclelogic2 > 1802) {
            _Client.cyclelogic2 = 0;
            // ANTICHEAT_CYCLELOGIC2
            p1isaac(c->out, 146);
            p1(c->out, 0);
            int start = c->out->pos;
            p2(c->out, 29711);
            p1(c->out, 70);
            p1(c->out, (int)(jrand() * 256.0));
            p1(c->out, 242);
            p1(c->out, 186);
            p1(c->out, 39);
            p1(c->out, 61);
            if ((int)(jrand() * 2.0) == 0) {
                p1(c->out, 13);
            }
            if ((int)(jrand() * 2.0) == 0) {
                p2(c->out, 57856);
            }
            p2(c->out, (int)(jrand() * 65536.0));
            psize1(c->out, c->out->pos - start);
        }
    }

    int level;
    if (c->cutscene) {
        level = getTopLevelCutscene(c);
    } else {
        level = getTopLevel(c);
    }

    int cameraX = c->cameraX;
    int cameraY = c->cameraY;
    int cameraZ = c->cameraZ;
    int cameraPitch = c->cameraPitch;
    int cameraYaw = c->cameraYaw;
    int jitter;
    for (int type = 0; type < 5; type++) {
        if (c->cameraModifierEnabled[type]) {
            jitter = (int)(jrand() * (double)(c->cameraModifierJitter[type] * 2 + 1) - (double)c->cameraModifierJitter[type] + sin((double)c->cameraModifierCycle[type] * ((double)c->cameraModifierWobbleSpeed[type] / 100.0)) * (double)c->cameraModifierWobbleScale[type]);
            if (type == 0) {
                c->cameraX += jitter;
            }
            if (type == 1) {
                c->cameraY += jitter;
            }
            if (type == 2) {
                c->cameraZ += jitter;
            }
            if (type == 3) {
                c->cameraYaw = c->cameraYaw + jitter & 0x7ff;
            }
            if (type == 4) {
                c->cameraPitch += jitter;
                if (c->cameraPitch < 128) {
                    c->cameraPitch = 128;
                }
                if (c->cameraPitch > 383) {
                    c->cameraPitch = 383;
                }
            }
        }
    }
    jitter = _Pix3D.cycle;
    _Model.check_hover = true;
    _Model.picked_count = 0;
    _Model.mouse_x = c->shell->mouse_x - 8;
    _Model.mouse_y = c->shell->mouse_y - 11;
    pix2d_clear();
    world3d_draw(c->scene, c->cameraX, c->cameraY, c->cameraZ, level, c->cameraYaw, c->cameraPitch, _Client.loop_cycle);
    world3d_clear_temporarylocs(c->scene);
    draw2DEntityElements(c);
    drawTileHint(c);
    updateTextures(c, jitter);
    draw3DEntityElements(c);
    pixmap_draw(c->area_viewport, 8, 11);
    c->cameraX = cameraX;
    c->cameraY = cameraY;
    c->cameraZ = cameraZ;
    c->cameraPitch = cameraPitch;
    c->cameraYaw = cameraYaw;
}

int getTopLevel(Client *c) {
    int top = 3;
    if (c->cameraPitch < 310) {
        int cameraLocalTileX = c->cameraX >> 7;
        int cameraLocalTileZ = c->cameraZ >> 7;
        int playerLocalTileX = c->local_player->pathing_entity.x >> 7;
        int playerLocalTileZ = c->local_player->pathing_entity.z >> 7;
        if ((c->levelTileFlags[c->currentLevel][cameraLocalTileX][cameraLocalTileZ] & 0x4) != 0) {
            top = c->currentLevel;
        }
        int tileDeltaX;
        if (playerLocalTileX > cameraLocalTileX) {
            tileDeltaX = playerLocalTileX - cameraLocalTileX;
        } else {
            tileDeltaX = cameraLocalTileX - playerLocalTileX;
        }
        int tileDeltaZ;
        if (playerLocalTileZ > cameraLocalTileZ) {
            tileDeltaZ = playerLocalTileZ - cameraLocalTileZ;
        } else {
            tileDeltaZ = cameraLocalTileZ - playerLocalTileZ;
        }
        int delta;
        int accumulator;
        if (tileDeltaX > tileDeltaZ) {
            delta = tileDeltaZ * 65536 / tileDeltaX;
            accumulator = 32768;
            while (cameraLocalTileX != playerLocalTileX) {
                if (cameraLocalTileX < playerLocalTileX) {
                    cameraLocalTileX++;
                } else if (cameraLocalTileX > playerLocalTileX) {
                    cameraLocalTileX--;
                }
                if ((c->levelTileFlags[c->currentLevel][cameraLocalTileX][cameraLocalTileZ] & 0x4) != 0) {
                    top = c->currentLevel;
                }
                accumulator += delta;
                if (accumulator >= 65536) {
                    accumulator -= 65536;
                    if (cameraLocalTileZ < playerLocalTileZ) {
                        cameraLocalTileZ++;
                    } else if (cameraLocalTileZ > playerLocalTileZ) {
                        cameraLocalTileZ--;
                    }
                    if ((c->levelTileFlags[c->currentLevel][cameraLocalTileX][cameraLocalTileZ] & 0x4) != 0) {
                        top = c->currentLevel;
                    }
                }
            }
        } else {
            delta = tileDeltaX * 65536 / tileDeltaZ;
            accumulator = 32768;
            while (cameraLocalTileZ != playerLocalTileZ) {
                if (cameraLocalTileZ < playerLocalTileZ) {
                    cameraLocalTileZ++;
                } else if (cameraLocalTileZ > playerLocalTileZ) {
                    cameraLocalTileZ--;
                }
                if ((c->levelTileFlags[c->currentLevel][cameraLocalTileX][cameraLocalTileZ] & 0x4) != 0) {
                    top = c->currentLevel;
                }
                accumulator += delta;
                if (accumulator >= 65536) {
                    accumulator -= 65536;
                    if (cameraLocalTileX < playerLocalTileX) {
                        cameraLocalTileX++;
                    } else if (cameraLocalTileX > playerLocalTileX) {
                        cameraLocalTileX--;
                    }
                    if ((c->levelTileFlags[c->currentLevel][cameraLocalTileX][cameraLocalTileZ] & 0x4) != 0) {
                        top = c->currentLevel;
                    }
                }
            }
        }
    }
    if ((c->levelTileFlags[c->currentLevel][c->local_player->pathing_entity.x >> 7][c->local_player->pathing_entity.z >> 7] & 0x4) != 0) {
        top = c->currentLevel;
    }
    return top;
}

int getTopLevelCutscene(Client *c) {
    int y = getHeightmapY(c, c->currentLevel, c->cameraX, c->cameraZ);
    return y - c->cameraY >= 800 || (c->levelTileFlags[c->currentLevel][c->cameraX >> 7][c->cameraZ >> 7] & 0x4) == 0 ? 3 : c->currentLevel;
}

int getHeightmapY(Client *c, int level, int sceneX, int sceneZ) {
    int tileX = MIN(sceneX >> 7, 104 - 1);
    int tileZ = MIN(sceneZ >> 7, 104 - 1);
    int realLevel = level;
    if (level < 3 && (c->levelTileFlags[1][tileX][tileZ] & 0x2) == 2) {
        realLevel = level + 1;
    }

    int tileLocalX = sceneX & 0x7f;
    int tileLocalZ = sceneZ & 0x7f;
    int y00 = (c->levelHeightmap[realLevel][tileX][tileZ] * (128 - tileLocalX) + c->levelHeightmap[realLevel][tileX + 1][tileZ] * tileLocalX) >> 7;
    int y11 = (c->levelHeightmap[realLevel][tileX][tileZ + 1] * (128 - tileLocalX) + c->levelHeightmap[realLevel][tileX + 1][tileZ + 1] * tileLocalX) >> 7;
    return (y00 * (128 - tileLocalZ) + y11 * tileLocalZ) >> 7;
}

void orbitCamera(Client *c, int targetX, int targetY, int targetZ, int yaw, int pitch, int distance) {
    int invPitch = 2048 - pitch & 0x7ff;
    int invYaw = 2048 - yaw & 0x7ff;
    int x = 0;
    int z = 0;
    int y = distance;
    int sin;
    int cos;
    int tmp;

    if (invPitch != 0) {
        sin = _Pix3D.sin_table[invPitch];
        cos = _Pix3D.cos_table[invPitch];
        tmp = (z * cos - distance * sin) >> 16;
        y = (z * sin + distance * cos) >> 16;
        z = tmp;
    }

    if (invYaw != 0) {
        sin = _Pix3D.sin_table[invYaw];
        cos = _Pix3D.cos_table[invYaw];
        tmp = (y * sin + x * cos) >> 16;
        y = (y * cos - x * sin) >> 16;
        x = tmp;
    }

    c->cameraX = targetX - x;
    c->cameraY = targetY - z;
    c->cameraZ = targetZ - y;
    c->cameraPitch = pitch;
    c->cameraYaw = yaw;
}

void pushLocs(Client *c) {
    for (LocEntity *loc = (LocEntity *)linklist_head(c->locList); loc; loc = (LocEntity *)linklist_next(c->locList)) {
        bool append = false;
        loc->seqCycle += c->scene_delta;
        if (loc->seqFrame == -1) {
            loc->seqFrame = 0;
            append = true;
        }

        while (loc->seqCycle > loc->seq->delay[loc->seqFrame]) {
            loc->seqCycle -= loc->seq->delay[loc->seqFrame] + 1;
            loc->seqFrame++;

            append = true;

            if (loc->seqFrame >= loc->seq->frameCount) {
                loc->seqFrame -= loc->seq->replayoff;

                if (loc->seqFrame < 0 || loc->seqFrame >= loc->seq->frameCount) {
                    linkable_unlink(&loc->link);
                    free(loc);
                    append = false;
                    break;
                }
            }
        }

        if (append) {
            int level = loc->level;
            int x = loc->x;
            int z = loc->z;

            int bitset = 0;
            if (loc->type == 0) {
                bitset = world3d_get_wallbitset(c->scene, level, x, z);
            }

            if (loc->type == 1) {
                bitset = world3d_get_walldecorationbitset(c->scene, level, z, x);
            }

            if (loc->type == 2) {
                bitset = world3d_get_locbitset(c->scene, level, x, z);
            }

            if (loc->type == 3) {
                bitset = world3d_get_grounddecorationbitset(c->scene, level, x, z);
            }

            if (bitset != 0 && (bitset >> 14 & 0x7fff) == loc->index) {
                int heightmapSW = c->levelHeightmap[level][x][z];
                int heightmapSE = c->levelHeightmap[level][x + 1][z];
                int heightmapNE = c->levelHeightmap[level][x + 1][z + 1];
                int heightmapNW = c->levelHeightmap[level][x][z + 1];

                LocType *type = loctype_get(loc->index);
                int seqId = -1;
                if (loc->seqFrame != -1) {
                    seqId = loc->seq->frames[loc->seqFrame];
                }

                if (loc->type == 2) {
                    int info = world3d_get_info(c->scene, level, x, z, bitset);
                    int shape = info & 0x1f;
                    int rotation = info >> 6;

                    if (shape == CENTREPIECE_DIAGONAL) {
                        shape = CENTREPIECE_STRAIGHT;
                    }

                    Model *model = loctype_get_model(type, shape, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, seqId);
                    world3d_set_locmodel(c->scene, level, x, z, model);
                } else if (loc->type == 1) {
                    Model *model = loctype_get_model(type, WALLDECOR_STRAIGHT_NOOFFSET, 0, heightmapSW, heightmapSE, heightmapNE, heightmapNW, seqId);
                    world3d_set_walldecorationmodel(c->scene, level, x, z, model);
                } else if (loc->type == 0) {
                    int info = world3d_get_info(c->scene, level, x, z, bitset);
                    int shape = info & 0x1f;
                    int rotation = info >> 6;

                    if (shape == WALL_L) {
                        int nextRotation = rotation + 1 & 0x3;
                        Model *model1 = loctype_get_model(type, WALL_L, rotation + 4, heightmapSW, heightmapSE, heightmapNE, heightmapNW, seqId);
                        Model *model2 = loctype_get_model(type, WALL_L, nextRotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, seqId);
                        world3d_set_wallmodels(c->scene, x, z, level, model1, model2);
                    } else {
                        Model *model = loctype_get_model(type, shape, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, seqId);
                        world3d_set_wallmodel(c->scene, level, x, z, model);
                    }
                } else if (loc->type == 3) {
                    int info = world3d_get_info(c->scene, level, x, z, bitset);
                    int rotation = info >> 6;
                    Model *model = loctype_get_model(type, GROUNDDECOR, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, seqId);
                    world3d_set_grounddecorationmodel(c->scene, level, x, z, model);
                }
            } else {
                linkable_unlink(&loc->link);
                free(loc);
            }
        }
    }
}

void pushSpotanims(Client *c) {
    for (SpotAnimEntity *entity = (SpotAnimEntity *)linklist_head(c->spotanims); entity; entity = (SpotAnimEntity *)linklist_next(c->spotanims)) {
        if (entity->level != c->currentLevel || entity->seqComplete) {
            linkable_unlink(&entity->entity.link);
            free(entity);
        } else if (_Client.loop_cycle >= entity->startCycle) {
            spotanimentity_update(entity, c->scene_delta);
            if (entity->seqComplete) {
                linkable_unlink(&entity->entity.link);
                free(entity);
            } else {
                world3d_add_temporary(c->scene, entity->level, entity->x, entity->y, entity->z, NULL, &entity->entity, -1, 0, 60, false);
            }
        }
    }
}

void pushProjectiles(Client *c) {
    for (ProjectileEntity *proj = (ProjectileEntity *)linklist_head(c->projectiles); proj; proj = (ProjectileEntity *)linklist_next(c->projectiles)) {
        if (proj->level != c->currentLevel || _Client.loop_cycle > proj->lastCycle) {
            linkable_unlink(&proj->entity.link);
            free(proj);
        } else if (_Client.loop_cycle >= proj->startCycle) {
            if (proj->target > 0) {
                NpcEntity *npc = c->npcs[proj->target - 1];
                if (npc) {
                    projectileentity_update_velocity(proj, npc->pathing_entity.x, getHeightmapY(c, proj->level, npc->pathing_entity.x, npc->pathing_entity.z) - proj->offsetY, npc->pathing_entity.z, _Client.loop_cycle);
                }
            }

            if (proj->target < 0) {
                int index = -proj->target - 1;
                PlayerEntity *player;
                if (index == c->local_pid) {
                    player = c->local_player;
                } else {
                    player = c->players[index];
                }
                if (player) {
                    projectileentity_update_velocity(proj, player->pathing_entity.x, getHeightmapY(c, proj->level, player->pathing_entity.x, player->pathing_entity.z) - proj->offsetY, player->pathing_entity.z, _Client.loop_cycle);
                }
            }

            projectileentity_update(proj, c->scene_delta);
            world3d_add_temporary(c->scene, c->currentLevel, (int)proj->x, (int)proj->y, (int)proj->z, NULL, &proj->entity, -1, proj->yaw, 60, false);
        }
    }
}

void pushNpcs(Client *c) {
    for (int i = 0; i < c->npc_count; i++) {
        NpcEntity *npc = c->npcs[c->npc_ids[i]];
        int bitset = (c->npc_ids[i] << 14) + 0x20000000;

        if (!npc || !npcentity_is_visible(npc)) {
            continue;
        }

        int x = npc->pathing_entity.x >> 7;
        int z = npc->pathing_entity.z >> 7;

        if (x < 0 || x >= 104 || z < 0 || z >= 104) {
            continue;
        }

        if (npc->pathing_entity.size == 1 && (npc->pathing_entity.x & 0x7f) == 64 && (npc->pathing_entity.z & 0x7f) == 64) {
            if (c->tileLastOccupiedCycle[x][z] == c->scene_cycle) {
                continue;
            }

            c->tileLastOccupiedCycle[x][z] = c->scene_cycle;
        }

        world3d_add_temporary(c->scene, c->currentLevel, npc->pathing_entity.x, getHeightmapY(c, c->currentLevel, npc->pathing_entity.x, npc->pathing_entity.z), npc->pathing_entity.z, NULL, &npc->pathing_entity.entity, bitset, npc->pathing_entity.yaw, (npc->pathing_entity.size - 1) * 64 + 60, npc->pathing_entity.seqStretches);
    }
}

void pushPlayers(Client *c) {
    if (c->local_player->pathing_entity.x >> 7 == c->flagSceneTileX && c->local_player->pathing_entity.z >> 7 == c->flagSceneTileZ) {
        c->flagSceneTileX = 0;
    }

    for (int i = -1; i < c->player_count; i++) {
        PlayerEntity *player;
        int id;
        if (i == -1) {
            player = c->local_player;
            id = LOCAL_PLAYER_INDEX << 14;
        } else {
            player = c->players[c->player_ids[i]];
            id = c->player_ids[i] << 14;
        }

        if (!player || !playerentity_is_visible(player)) {
            continue;
        }

        player->lowmem = ((_Client.lowmem && c->player_count > 50) || c->player_count > 200) && i != -1 && player->pathing_entity.secondarySeqId == player->pathing_entity.seqStandId;
        int stx = player->pathing_entity.x >> 7;
        int stz = player->pathing_entity.z >> 7;

        if (stx < 0 || stx >= 104 || stz < 0 || stz >= 104) {
            continue;
        }

        if (!player->locModel || _Client.loop_cycle < player->locStartCycle || _Client.loop_cycle >= player->locStopCycle) {
            if ((player->pathing_entity.x & 0x7f) == 64 && (player->pathing_entity.z & 0x7f) == 64) {
                if (c->tileLastOccupiedCycle[stx][stz] == c->scene_cycle) {
                    continue;
                }

                c->tileLastOccupiedCycle[stx][stz] = c->scene_cycle;
            }

            player->y = getHeightmapY(c, c->currentLevel, player->pathing_entity.x, player->pathing_entity.z);
            world3d_add_temporary(c->scene, c->currentLevel, player->pathing_entity.x, player->y, player->pathing_entity.z, NULL, &player->pathing_entity.entity, id, player->pathing_entity.yaw, 60, player->pathing_entity.seqStretches);
        } else {
            player->lowmem = false;
            player->y = getHeightmapY(c, c->currentLevel, player->pathing_entity.x, player->pathing_entity.z);
            world3d_add_temporary2(c->scene, c->currentLevel, player->pathing_entity.x, player->y, player->pathing_entity.z, player->minTileX, player->minTileZ, player->maxTileX, player->maxTileZ, NULL, &player->pathing_entity.entity, id, player->pathing_entity.yaw);
        }
    }
}

void client_draw_minimap(Client *c) {
    pixmap_bind(c->area_mapback);
    int angle = c->orbit_camera_yaw + c->minimap_anticheat_angle & 0x7ff;
    int anchorX = c->local_player->pathing_entity.x / 32 + 48;
    int anchorY = 464 - c->local_player->pathing_entity.z / 32;

    pix24_draw_rotated_masked(c->image_minimap, 21, 9, 146, 151, c->minimap_mask_line_offsets, c->minimap_mask_line_lengths, anchorX, anchorY, angle, c->minimap_zoom + 256);
    pix24_draw_rotated_masked(c->image_compass, 0, 0, 33, 33, c->compass_mask_line_offsets, c->compass_mask_line_lengths, 25, 25, c->orbit_camera_yaw, 256);
    for (int i = 0; i < c->activeMapFunctionCount; i++) {
        anchorX = c->activeMapFunctionX[i] * 4 + 2 - c->local_player->pathing_entity.x / 32;
        anchorY = c->activeMapFunctionZ[i] * 4 + 2 - c->local_player->pathing_entity.z / 32;
        client_draw_on_minimap(c, anchorY, c->activeMapFunctions[i], anchorX);
    }

    for (int ltx = 0; ltx < 104; ltx++) {
        for (int ltz = 0; ltz < 104; ltz++) {
            LinkList *stack = c->level_obj_stacks[c->currentLevel][ltx][ltz];
            if (stack) {
                anchorX = ltx * 4 + 2 - c->local_player->pathing_entity.x / 32;
                anchorY = ltz * 4 + 2 - c->local_player->pathing_entity.z / 32;
                client_draw_on_minimap(c, anchorY, c->image_mapdot0, anchorX);
            }
        }
    }

    for (int i = 0; i < c->npc_count; i++) {
        NpcEntity *npc = c->npcs[c->npc_ids[i]];
        if (npc && npcentity_is_visible(npc) && npc->type->minimap) {
            anchorX = npc->pathing_entity.x / 32 - c->local_player->pathing_entity.x / 32;
            anchorY = npc->pathing_entity.z / 32 - c->local_player->pathing_entity.z / 32;
            client_draw_on_minimap(c, anchorY, c->image_mapdot1, anchorX);
        }
    }

    for (int i = 0; i < c->player_count; i++) {
        PlayerEntity *player = c->players[c->player_ids[i]];
        if (player && playerentity_is_visible(player)) {
            anchorX = player->pathing_entity.x / 32 - c->local_player->pathing_entity.x / 32;
            anchorY = player->pathing_entity.z / 32 - c->local_player->pathing_entity.z / 32;

            bool friend = false;
            int64_t name37 = jstring_to_base37(player->name);
            for (int j = 0; j < c->friend_count; j++) {
                if (name37 == c->friendName37[j] && c->friendWorld[j] != 0) {
                    friend = true;
                    break;
                }
            }

            if (friend) {
                client_draw_on_minimap(c, anchorY, c->image_mapdot3, anchorX);
            } else {
                client_draw_on_minimap(c, anchorY, c->image_mapdot2, anchorX);
            }
        }
    }

    if (c->flagSceneTileX != 0) {
        anchorX = c->flagSceneTileX * 4 + 2 - c->local_player->pathing_entity.x / 32;
        anchorY = c->flagSceneTileZ * 4 + 2 - c->local_player->pathing_entity.z / 32;
        client_draw_on_minimap(c, anchorY, c->image_mapflag, anchorX);
    }

    pix2d_fill_rect(93, 82, WHITE, 3, 3);
    pixmap_bind(c->area_viewport);
}

void client_draw_on_minimap(Client *c, int dy, Pix24 *image, int dx) {
    int angle = c->orbit_camera_yaw + c->minimap_anticheat_angle & 0x7ff;
    int distance = dx * dx + dy * dy;
    if (distance > 6400) {
        return;
    }

    int sinAngle = _Pix3D.sin_table[angle];
    int cosAngle = _Pix3D.cos_table[angle];

    sinAngle = sinAngle * 256 / (c->minimap_zoom + 256);
    cosAngle = cosAngle * 256 / (c->minimap_zoom + 256);

    int x = (dy * sinAngle + dx * cosAngle) >> 16;
    int y = (dy * cosAngle - dx * sinAngle) >> 16;

    if (distance > 2500) {
        pix24_draw_masked(image, x + 94 - image->crop_w / 2, 83 - y - image->crop_h / 2, c->image_mapback);
    } else {
        pix24_draw(image, x + 94 - image->crop_w / 2, 83 - y - image->crop_h / 2);
    }
}

void client_draw_chatback(Client *c) {
    pixmap_bind(c->area_chatback);
    _Pix3D.line_offset = c->area_chatback_offsets;
    pix8_draw(c->image_chatback, 0, 0);
    if (c->show_social_input) {
        char buf[CHAT_LENGTH + 2];
        sprintf(buf, "%s*", c->social_input);
        drawStringCenter(c->font_bold12, 239, 40, c->social_message, BLACK);
        drawStringCenter(c->font_bold12, 239, 60, buf, DARKBLUE);
    } else if (c->chatback_input_open) {
        char buf[CHATBACK_LENGTH + 2];
        sprintf(buf, "%s*", c->chatback_input);
        drawStringCenter(c->font_bold12, 239, 40, "Enter amount:", BLACK);
        drawStringCenter(c->font_bold12, 239, 60, buf, DARKBLUE);
    } else if (c->modal_message[0]) {
        drawStringCenter(c->font_bold12, 239, 40, c->modal_message, BLACK);
        drawStringCenter(c->font_bold12, 239, 60, "Click to continue", DARKBLUE);
    } else if (c->chat_interface_id != -1) {
        client_draw_interface(c, _Component.instances[c->chat_interface_id], 0, 0, 0);
    } else if (c->sticky_chat_interface_id == -1) {
        PixFont *font = c->font_plain12;
        if (_Custom.chat_era == 0) {
            font = c->font_quill8;
        }
        int line = 0;
        pix2d_set_clipping(77, 463, 0, 0);
        for (int i = 0; i < 100; i++) {
            if (c->message_text[i][0]) {
                int type = c->message_type[i];
                int offset = c->chat_scroll_offset + 70 - line * 14;
                if (type == 0) {
                    if (offset > 0 && offset < 110) {
                        drawString(font, 4, offset, c->message_text[i], BLACK);
                    }
                    line++;
                }
                if (type == 1) {
                    if (offset > 0 && offset < 110) {
                        char buf[USERNAME_LENGTH + 2];
                        sprintf(buf, "%s:", c->message_sender[i]);
                        drawString(font, 4, offset, buf, WHITE);
                        drawString(font, stringWidth(font, c->message_sender[i]) + 12, offset, c->message_text[i], BLUE);
                    }
                    line++;
                }
                if (type == 2 && (c->public_chat_setting == 0 || (c->public_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
                    if (offset > 0 && offset < 110) {
                        char buf[USERNAME_LENGTH + 2];
                        sprintf(buf, "%s:", c->message_sender[i]);
                        drawString(font, 4, offset, buf, BLACK);
                        drawString(font, stringWidth(font, c->message_sender[i]) + 12, offset, c->message_text[i], BLUE);
                    }
                    line++;
                }
                if ((type == 3 || type == 7) && c->split_private_chat == 0 && (type == 7 || c->private_chat_setting == 0 || (c->private_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
                    if (offset > 0 && offset < 110) {
                        char buf[USERNAME_LENGTH + 7];
                        sprintf(buf, "From %s:", c->message_sender[i]);
                        drawString(font, 4, offset, buf, BLACK);
                        sprintf(buf, "From %s", c->message_sender[i]);
                        drawString(font, stringWidth(font, buf) + 12, offset, c->message_text[i], DARKRED);
                    }
                    line++;
                }
                if (type == 4 && (c->trade_chat_setting == 0 || (c->trade_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
                    if (offset > 0 && offset < 110) {
                        char buf[USERNAME_LENGTH + CHAT_LENGTH + 2];
                        sprintf(buf, "%s %s", c->message_sender[i], c->message_text[i]);
                        drawString(font, 4, offset, buf, TRADE_MESSAGE);
                    }
                    line++;
                }
                if (type == 5 && c->split_private_chat == 0 && c->private_chat_setting < 2) {
                    if (offset > 0 && offset < 110) {
                        drawString(font, 4, offset, c->message_text[i], DARKRED);
                    }
                    line++;
                }
                if (type == 6 && c->split_private_chat == 0 && c->private_chat_setting < 2) {
                    if (offset > 0 && offset < 110) {
                        char buf[USERNAME_LENGTH + 6];
                        sprintf(buf, "To %s:", c->message_sender[i]);
                        drawString(font, 4, offset, buf, BLACK);
                        sprintf(buf, "To %s", c->message_sender[i]);
                        drawString(font, stringWidth(font, buf) + 12, offset, c->message_text[i], DARKRED);
                    }
                    line++;
                }
                if (type == 8 && (c->trade_chat_setting == 0 || (c->trade_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
                    if (offset > 0 && offset < 110) {
                        char buf[USERNAME_LENGTH + CHAT_LENGTH + 2];
                        sprintf(buf, "%s %s", c->message_sender[i], c->message_text[i]);
                        drawString(font, 4, offset, buf, DUEL_MESSAGE);
                    }
                    line++;
                }
            }
        }
        pix2d_reset_clipping();
        c->chat_scroll_height = line * 14 + 7;
        if (c->chat_scroll_height < 78) {
            c->chat_scroll_height = 78;
        }
        client_draw_scrollbar(c, 463, 0, c->chat_scroll_height - c->chat_scroll_offset - 77, c->chat_scroll_height, 77);

        if (_Custom.chat_era == 0) {
            // 186-194?
            char buf2[CHAT_LENGTH + 2];
            sprintf(buf2, "%s*", c->chat_typed);
            drawString(font, 3, 90, buf2, BLACK);
        } else if (_Custom.chat_era == 1) {
            // <204
            char buf2[CHAT_LENGTH + 2];
            sprintf(buf2, "%s*", c->chat_typed);
            drawString(font, 3, 90, buf2, BLUE);
        } else if (_Custom.chat_era == 2) {
            // 204+
            char buf[USERNAME_LENGTH + 3];
            sprintf(buf, "%s:", jstring_format_name(c->username));
            drawString(font, 4, 90, buf, BLACK);
            sprintf(buf, "%s: ", c->username);

            char buf2[CHAT_LENGTH + 2];
            sprintf(buf2, "%s*", c->chat_typed);
            drawString(font, stringWidth(font, buf) + 6, 90, buf2, BLUE);
        }

        pix2d_hline(0, 77, BLACK, 479);
    } else {
        client_draw_interface(c, _Component.instances[c->sticky_chat_interface_id], 0, 0, 0);
    }
    if (c->menu_visible && c->menu_area == 2) {
        client_draw_menu(c);
    }
    pixmap_draw(c->area_chatback, 22, 375);
    pixmap_bind(c->area_viewport);
    _Pix3D.line_offset = c->area_viewport_offsets;
}

bool client_is_friend(Client *c, const char *username) {
    if (!username) {
        return false;
    }

    for (int i = 0; i < c->friend_count; i++) {
        if (platform_strcasecmp(username, c->friendName[i]) == 0) {
            return true;
        }
    }

    return (platform_strcasecmp(username, c->local_player->name) == 0);
}

void client_draw_scrollbar(Client *c, int x, int y, int scrollY, int scrollHeight, int height) {
    pix8_draw(c->image_scrollbar0, x, y);
    pix8_draw(c->image_scrollbar1, x, y + height - 16);
    pix2d_fill_rect(x, y + 16, SCROLLBAR_TRACK, 16, height - 32);

    int gripSize = (height - 32) * height / scrollHeight;
    if (gripSize < 8) {
        gripSize = 8;
    }

    int gripY = (height - gripSize - 32) * scrollY / (scrollHeight - height);
    pix2d_fill_rect(x, y + gripY + 16, SCROLLBAR_GRIP_FOREGROUND, 16, gripSize);

    pix2d_vline(x, y + gripY + 16, SCROLLBAR_GRIP_HIGHLIGHT, gripSize);
    pix2d_vline(x + 1, y + gripY + 16, SCROLLBAR_GRIP_HIGHLIGHT, gripSize);

    pix2d_hline(x, y + gripY + 16, SCROLLBAR_GRIP_HIGHLIGHT, 16);
    pix2d_hline(x, y + gripY + 17, SCROLLBAR_GRIP_HIGHLIGHT, 16);

    pix2d_vline(x + 15, y + gripY + 16, SCROLLBAR_GRIP_LOWLIGHT, gripSize);
    pix2d_vline(x + 14, y + gripY + 17, SCROLLBAR_GRIP_LOWLIGHT, gripSize - 1);

    pix2d_hline(x, y + gripY + gripSize + 15, SCROLLBAR_GRIP_LOWLIGHT, 16);
    pix2d_hline(x + 1, y + gripY + gripSize + 14, SCROLLBAR_GRIP_LOWLIGHT, 15);
}

void client_draw_sidebar(Client *c) {
    pixmap_bind(c->area_sidebar);
    _Pix3D.line_offset = c->area_sidebar_offsets;
    pix8_draw(c->image_invback, 0, 0);
    if (c->sidebar_interface_id != -1) {
        client_draw_interface(c, _Component.instances[c->sidebar_interface_id], 0, 0, 0);
    } else if (c->tab_interface_id[c->selected_tab] != -1) {
        client_draw_interface(c, _Component.instances[c->tab_interface_id[c->selected_tab]], 0, 0, 0);
    }
    if (c->menu_visible && c->menu_area == 1) {
        client_draw_menu(c);
    }
    pixmap_draw(c->area_sidebar, 562, 231);
    pixmap_bind(c->area_viewport);
    _Pix3D.line_offset = c->area_viewport_offsets;
}

void client_update_interface_content(Client *c, Component *component) {
    int clientCode = component->clientCode;

    if (clientCode >= 1 && clientCode <= 100) {
        clientCode--;
        if (clientCode >= c->friend_count) {
            strcpy(component->text, "");
            component->buttonType = 0;
        } else {
            strcpy(component->text, c->friendName[clientCode]);
            component->buttonType = 1;
        }
    } else if (clientCode >= 101 && clientCode <= 200) {
        clientCode -= 101;
        if (clientCode >= c->friend_count) {
            strcpy(component->text, "");
            component->buttonType = 0;
        } else {
            if (c->friendWorld[clientCode] == 0) {
                strcpy(component->text, "@red@Offline");
            } else if (c->friendWorld[clientCode] == _Client.nodeid) {
                sprintf(component->text, "@gre@World-%d", c->friendWorld[clientCode] - 9);
            } else {
                sprintf(component->text, "@yel@World-%d", c->friendWorld[clientCode] - 9);
            }
            component->buttonType = 1;
        }
    } else if (clientCode == 203) {
        component->scroll = c->friend_count * 15 + 20;
        if (component->scroll <= component->height) {
            component->scroll = component->height + 1;
        }
    } else if (clientCode >= 401 && clientCode <= 500) {
        clientCode -= 401;
        if (clientCode >= c->ignoreCount) {
            strcpy(component->text, "");
            component->buttonType = 0;
        } else {
            strcpy(component->text, jstring_format_name(jstring_from_base37(c->ignoreName37[clientCode])));
            component->buttonType = 1;
        }
    } else if (clientCode == 503) {
        component->scroll = c->ignoreCount * 15 + 20;
        if (component->scroll <= component->height) {
            component->scroll = component->height + 1;
        }
    } else if (clientCode == 327) {
        component->xan = 150;
        component->yan = (int)(sin((double)_Client.loop_cycle / 40.0) * 256.0) & 0x7ff;
        if (c->update_design_model) {
            c->update_design_model = false;

            Model **models = calloc(7, sizeof(Model *));
            int modelCount = 0;
            for (int part = 0; part < 7; part++) {
                int kit = c->designIdentikits[part];
                if (kit >= 0) {
                    models[modelCount++] = idktype_get_model(_IdkType.instances[kit]);
                }
            }

            Model *model = model_from_models(models, modelCount, false);
            for (int part = 0; part < 5; part++) {
                if (c->design_colors[part] != 0) {
                    model_recolor(model, DESIGN_BODY_COLOR[part][0], DESIGN_BODY_COLOR[part][c->design_colors[part]]);
                    if (part == 1) {
                        model_recolor(model, DESIGN_HAIR_COLOR[0], DESIGN_HAIR_COLOR[c->design_colors[part]]);
                    }
                }
            }

            model_create_label_references(model, false);
            model_apply_transform(model, _SeqType.instances[c->local_player->pathing_entity.seqStandId]->frames[0]);
            model_calculate_normals(model, 64, 850, -30, -50, -30, true, false);
            component->model = model;
        }
    } else if (clientCode == 324) {
        if (!c->genderButtonImage0) {
            c->genderButtonImage0 = component->graphic;
            c->genderButtonImage1 = component->activeGraphic;
        }
        if (c->design_gender_male) {
            component->graphic = c->genderButtonImage1;
        } else {
            component->graphic = c->genderButtonImage0;
        }
    } else if (clientCode == 325) {
        if (!c->genderButtonImage0) {
            c->genderButtonImage0 = component->graphic;
            c->genderButtonImage1 = component->activeGraphic;
        }
        if (c->design_gender_male) {
            component->graphic = c->genderButtonImage0;
        } else {
            component->graphic = c->genderButtonImage1;
        }
    } else if (clientCode == 600) {
        strcpy(component->text, c->reportAbuseInput);
        if (_Client.loop_cycle % 20 < 10) {
            strcat(component->text, "|");
        } else {
            strcat(component->text, " ");
        }
    } else if (clientCode == 613) {
        if (!c->rights) {
            strcpy(component->text, "");
        } else if (c->reportAbuseMuteOption) {
            component->colour = RED;
            strcpy(component->text, "Moderator option: Mute player for 48 hours: <ON>");
        } else {
            component->colour = WHITE;
            strcpy(component->text, "Moderator option: Mute player for 48 hours: <OFF>");
        }
    } else if (clientCode == 650 || clientCode == 655) {
        if (c->lastAddress == 0) {
            strcpy(component->text, "");
        } else {
            char text[HALF_STR];
            if (c->daysSinceLastLogin == 0) {
                strcpy(text, "earlier today");
            } else if (c->daysSinceLastLogin == 1) {
                strcpy(text, "yesterday");
            } else {
                sprintf(text, "%d days ago", c->daysSinceLastLogin);
            }
            sprintf(component->text, "You last logged in %s from: %s", text, _Client.dns);
        }
    } else if (clientCode == 651) {
        if (c->unreadMessages == 0) {
            strcpy(component->text, "0 unread messages");
            component->colour = YELLOW;
        }
        if (c->unreadMessages == 1) {
            strcpy(component->text, "1 unread message");
            component->colour = GREEN;
        }
        if (c->unreadMessages > 1) {
            sprintf(component->text, "%d unread messages", c->unreadMessages);
            component->colour = GREEN;
        }
    } else if (clientCode == 652) {
        if (c->daysSinceRecoveriesChanged == 201) {
            strcpy(component->text, "");
        } else if (c->daysSinceRecoveriesChanged == 200) {
            strcpy(component->text, "You have not yet set any password recovery questions.");
        } else {
            char text[HALF_STR];
            if (c->daysSinceRecoveriesChanged == 0) {
                strcpy(text, "Earlier today");
            } else if (c->daysSinceRecoveriesChanged == 1) {
                strcpy(text, "Yesterday");
            } else {
                sprintf(text, "%d days ago", c->daysSinceRecoveriesChanged);
            }
            sprintf(component->text, "%s you changed your recovery questions", text);
        }
    } else if (clientCode == 653) {
        if (c->daysSinceRecoveriesChanged == 201) {
            strcpy(component->text, "");
        } else if (c->daysSinceRecoveriesChanged == 200) {
            strcpy(component->text, "We strongly recommend you do so now to secure your account.");
        } else {
            strcpy(component->text, "If you do not remember making this change then cancel it immediately");
        }
    } else if (clientCode == 654) {
        if (c->daysSinceRecoveriesChanged == 201) {
            strcpy(component->text, "");
        } else if (c->daysSinceRecoveriesChanged == 200) {
            strcpy(component->text, "Do this from the 'account management' area on our front webpage");
        } else {
            strcpy(component->text, "Do this from the 'account management' area on our front webpage");
        }
    }
}

void client_set_lowmem(void) {
    _World3D.lowMemory = true;
    _Pix3D.lowMemory = true;
    _Client.lowmem = true;
    _World.lowMemory = true;
}

void client_set_highmem(void) {
    _World3D.lowMemory = false;
    _Pix3D.lowMemory = false;
    _Client.lowmem = false;
    _World.lowMemory = false;
}

static const char *formatObjCountTagged(int amount) {
    static char s[SIXTY_STR];

    char tmp[14];
    sprintf(tmp, "%d", amount);

    int len = (int)strlen(tmp);
    for (int i = len - 3; i > 0; i -= 3) {
        for (int k = len; k >= i; k--) {
            tmp[k + 1] = tmp[k];
        }
        tmp[i] = ',';
        len++;
    }

    if (len > 8) {
        sprintf(s, " @gre@%.*s million @whi@(%s)", len - 8, tmp, tmp);
    } else if (len > 4) {
        sprintf(s, " @cya@%.*sK @whi@(%s)", len - 4, tmp, tmp);
    } else {
        sprintf(s, " %s", tmp);
    }

    return s;
}

static const char *formatObjCount(int amount) {
    static char s[12];
    if (amount < 100000) {
        sprintf(s, "%d", amount);
    } else if (amount < 10000000) {
        sprintf(s, "%dK", amount / 1000);
    } else {
        sprintf(s, "%dM", amount / 1000000);
    }
    return s;
}

static char *getIntString(int value) {
    return value < 999999999 ? valueof(value) : platform_strndup("*", 1);
}

static void client_draw_interface(Client *c, Component *com, int x, int y, int scrollY) {
    if (com->type != 0 || !com->childId || (com->hide && c->viewportHoveredInterfaceIndex != com->id && c->sidebarHoveredInterfaceIndex != com->id && c->chatHoveredInterfaceIndex != com->id)) {
        return;
    }

    int left = _Pix2D.left;
    int top = _Pix2D.top;
    int right = _Pix2D.right;
    int bottom = _Pix2D.bottom;

    pix2d_set_clipping(y + com->height, x + com->width, y, x);
    int children = com->childCount;

    for (int i = 0; i < children; i++) {
        int childX = com->childX[i] + x;
        int childY = com->childY[i] + y - scrollY;

        Component *child = _Component.instances[com->childId[i]];
        childX += child->x;
        childY += child->y;

        if (child->clientCode > 0) {
            client_update_interface_content(c, child);
        }

        if (child->type == 0) {
            if (child->scrollPosition > child->scroll - child->height) {
                child->scrollPosition = child->scroll - child->height;
            }

            if (child->scrollPosition < 0) {
                child->scrollPosition = 0;
            }

            client_draw_interface(c, child, childX, childY, child->scrollPosition);
            if (child->scroll > child->height) {
                client_draw_scrollbar(c, childX + child->width, childY, child->scrollPosition, child->scroll, child->height);
            }
        } else if (child->type == 2) {
            int slot = 0;

            for (int row = 0; row < child->height; row++) {
                for (int col = 0; col < child->width; col++) {
                    int slotX = childX + col * (child->marginX + 32);
                    int slotY = childY + row * (child->marginY + 32);

                    if (slot < 20) {
                        slotX += child->invSlotOffsetX[slot];
                        slotY += child->invSlotOffsetY[slot];
                    }

                    if (child->invSlotObjId[slot] > 0) {
                        int dx = 0;
                        int dy = 0;
                        int id = child->invSlotObjId[slot] - 1;

                        if ((slotX >= -32 && slotX <= 512 && slotY >= -32 && slotY <= 334) || (c->obj_drag_area != 0 && c->objDragSlot == slot)) {
                            Pix24 *icon = NULL;
                            if (_Custom.item_outlines) {
                                int outline_color = 0;
                                if (c->obj_selected == 1 && c->objSelectedSlot == slot && c->objSelectedInterface == child->id) {
                                    outline_color = WHITE;
                                }
                                icon = objtype_get_icon_outline(id, child->invSlotObjCount[slot], outline_color);
                            } else {
                                icon = objtype_get_icon(id, child->invSlotObjCount[slot]);
                            }
                            if (c->obj_drag_area != 0 && c->objDragSlot == slot && c->objDragInterfaceId == child->id) {
                                dx = c->shell->mouse_x - c->objGrabX;
                                dy = c->shell->mouse_y - c->objGrabY;

                                if (dx < 5 && dx > -5) {
                                    dx = 0;
                                }

                                if (dy < 5 && dy > -5) {
                                    dy = 0;
                                }

                                if (c->obj_drag_cycles < 5) {
                                    dx = 0;
                                    dy = 0;
                                }

                                pix24_draw_alpha(icon, 128, slotX + dx, slotY + dy);
                            } else if (c->selected_area != 0 && c->selectedItem == slot && c->selectedInterface == child->id) {
                                pix24_draw_alpha(icon, 128, slotX, slotY);
                            } else {
                                pix24_draw(icon, slotX, slotY);
                            }

                            if (icon->crop_w == 33 || child->invSlotObjCount[slot] != 1) {
                                int count = child->invSlotObjCount[slot];
                                drawString(c->font_plain11, slotX + dx + 1, slotY + 10 + dy, formatObjCount(count), BLACK);
                                drawString(c->font_plain11, slotX + dx, slotY + 9 + dy, formatObjCount(count), YELLOW);
                            }
                        }
                    } else if (child->invSlotSprite && slot < 20) {
                        Pix24 *image = child->invSlotSprite[slot];

                        if (image) {
                            pix24_draw(image, slotX, slotY);
                        }
                    }

                    slot++;
                }
            }
        } else if (child->type == 3) {
            if (child->fill) {
                pix2d_fill_rect(childX, childY, child->colour, child->width, child->height);
            } else {
                pix2d_draw_rect(childX, childY, child->colour, child->width, child->height);
            }
        } else if (child->type == 4) {
            PixFont *font = child->font;
            int color = child->colour;
            char text[DOUBLE_STR];
            strcpy(text, child->text);

            if ((c->chatHoveredInterfaceIndex == child->id || c->sidebarHoveredInterfaceIndex == child->id || c->viewportHoveredInterfaceIndex == child->id) && child->overColour != 0) {
                color = child->overColour;
            }

            if (client_execute_interface_script(c, child)) {
                color = child->activeColour;

                if (strlen(child->activeText) > 0) {
                    strcpy(text, child->activeText);
                }
            }

            if (child->buttonType == BUTTON_CONTINUE && c->pressed_continue_option) {
                strcpy(text, "Please wait...");
                color = child->colour;
            }

            for (int lineY = childY + font->height; strlen(text) > 0; lineY += font->height) {
                if (indexof(text, "%") != -1) {
                    do {
                        int index = indexof(text, "%1");
                        if (index == -1) {
                            break;
                        }

                        char *sub = substring(text, 0, index);
                        char *value = getIntString(client_execute_clientscript1(c, child, 0));
                        char *sub1 = substring(text, index + 2, strlen(text));
                        sprintf(text, "%s%s%s", sub, value, sub1);
                        free(sub);
                        free(value);
                        free(sub1);
                    } while (true);

                    do {
                        int index = indexof(text, "%2");
                        if (index == -1) {
                            break;
                        }

                        char *sub = substring(text, 0, index);
                        char *value = getIntString(client_execute_clientscript1(c, child, 1));
                        char *sub1 = substring(text, index + 2, strlen(text));
                        sprintf(text, "%s%s%s", sub, value, sub1);
                        free(sub);
                        free(value);
                        free(sub1);
                    } while (true);

                    do {
                        int index = indexof(text, "%3");
                        if (index == -1) {
                            break;
                        }

                        char *sub = substring(text, 0, index);
                        char *value = getIntString(client_execute_clientscript1(c, child, 2));
                        char *sub1 = substring(text, index + 2, strlen(text));
                        sprintf(text, "%s%s%s", sub, value, sub1);
                        free(sub);
                        free(value);
                        free(sub1);
                    } while (true);

                    do {
                        int index = indexof(text, "%4");
                        if (index == -1) {
                            break;
                        }

                        char *sub = substring(text, 0, index);
                        char *value = getIntString(client_execute_clientscript1(c, child, 3));
                        char *sub1 = substring(text, index + 2, strlen(text));
                        sprintf(text, "%s%s%s", sub, value, sub1);
                        free(sub);
                        free(value);
                        free(sub1);
                    } while (true);

                    do {
                        int index = indexof(text, "%5");
                        if (index == -1) {
                            break;
                        }

                        char *sub = substring(text, 0, index);
                        char *value = getIntString(client_execute_clientscript1(c, child, 4));
                        char *sub1 = substring(text, index + 2, strlen(text));
                        sprintf(text, "%s%s%s", sub, value, sub1);
                        free(sub);
                        free(value);
                        free(sub1);
                    } while (true);
                }

                int newline = indexof(text, "\\n");
                char split[DOUBLE_STR];
                if (newline != -1) {
                    char *sub = substring(text, 0, newline);
                    strcpy(split, sub);
                    free(sub);
                    char *sub1 = substring(text, newline + 2, strlen(text));
                    strcpy(text, sub1);
                    free(sub1);
                } else {
                    strcpy(split, text);
                    strcpy(text, "");
                }

                if (child->center) {
                    drawStringTaggableCenter(font, split, childX + child->width / 2, lineY, color, child->shadowed);
                } else {
                    drawStringTaggable(font, childX, lineY, split, color, child->shadowed);
                }
            }
        } else if (child->type == 5) {
            Pix24 *image;
            if (client_execute_interface_script(c, child)) {
                image = child->activeGraphic;
            } else {
                image = child->graphic;
            }

            if (image) {
                pix24_draw(image, childX, childY);
            }
        } else if (child->type == 6) {
            int tmpX = _Pix3D.center_x;
            int tmpY = _Pix3D.center_y;

            _Pix3D.center_x = childX + child->width / 2;
            _Pix3D.center_y = childY + child->height / 2;

            int eyeY = _Pix3D.sin_table[child->xan] * child->zoom >> 16;
            int eyeZ = _Pix3D.cos_table[child->xan] * child->zoom >> 16;

            bool active = client_execute_interface_script(c, child);
            int seqId;
            if (active) {
                seqId = child->activeAnim;
            } else {
                seqId = child->anim;
            }

            Model *model;
            bool _free = false;
            if (seqId == -1) {
                model = component_get_model2(child, -1, -1, active, &_free);
            } else {
                SeqType *seq = _SeqType.instances[seqId];
                model = component_get_model2(child, seq->frames[child->seqFrame], seq->iframes[child->seqFrame], active, &_free);
            }

            if (model) {
                model_draw_simple(model, 0, child->yan, 0, child->xan, 0, eyeY, eyeZ);
                if (_free) {
                    model_free_label_references(model);
                    model_free_calculate_normals(model);
                    model_free_share_colored(model, true, true, false);
                }
            }

            _Pix3D.center_x = tmpX;
            _Pix3D.center_y = tmpY;
        } else if (child->type == 7) {
            PixFont *font = child->font;
            int slot = 0;
            for (int row = 0; row < child->height; row++) {
                for (int col = 0; col < child->width; col++) {
                    if (child->invSlotObjId[slot] > 0) {
                        ObjType *obj = objtype_get(child->invSlotObjId[slot] - 1);
                        char text[MAX_STR];
                        strcpy(text, obj->name);
                        if (obj->stackable || child->invSlotObjCount[slot] != 1) {
                            char tmp[HALF_STR];
                            strcpy(tmp, text);
                            sprintf(text, "%s x%s", tmp, formatObjCountTagged(child->invSlotObjCount[slot]));
                        }

                        int textX = childX + col * (child->marginX + 115);
                        int textY = childY + row * (child->marginY + 12);

                        if (child->center) {
                            drawStringTaggableCenter(font, text, textX + child->width / 2, textY, child->colour, child->shadowed);
                        } else {
                            drawStringTaggable(font, textX, textY, text, child->colour, child->shadowed);
                        }
                    }

                    slot++;
                }
            }
        }
    }

    pix2d_set_clipping(bottom, right, top, left);
}

void client_draw_menu(Client *c) {
    int x = c->menu_x;
    int y = c->menu_y;
    int w = c->menu_width;
    int h = c->menu_height;
    int background = OPTIONS_MENU;
    pix2d_fill_rect(x, y, background, w, h);
    pix2d_fill_rect(x + 1, y + 1, BLACK, w - 2, 16);
    pix2d_draw_rect(x + 1, y + 18, BLACK, w - 2, h - 19);

    drawString(c->font_bold12, x + 3, y + 14, "Choose Option", background);
    int mouseX = c->shell->mouse_x;
    int mouseY = c->shell->mouse_y;
    if (c->menu_area == 0) {
        mouseX -= 8;
        mouseY -= 11;
    }
    if (c->menu_area == 1) {
        mouseX -= 562;
        mouseY -= 231;
    }
    if (c->menu_area == 2) {
        mouseX -= 22;
        mouseY -= 375;
    }

    for (int i = 0; i < c->menu_size; i++) {
        int optionY = y + (c->menu_size - 1 - i) * 15 + 31;
        int rgb = WHITE;
        if (mouseX > x && mouseX < x + w && mouseY > optionY - 13 && mouseY < optionY + 3) {
            rgb = YELLOW;
        }
        drawStringTaggable(c->font_bold12, x + 3, optionY, c->menu_option[i], rgb, true);
    }
}

void client_draw_error(Client *c) {
    platform_fill_rect(0, 0, 789, 532, BLACK);
    gameshell_set_framerate(c->shell, 1);

    int color;
    if (c->error_loading) {
        c->flame_active = false;
        int y = 35;

        color = YELLOW;
        gameshell_draw_string(c->shell, "Sorry, an error has occured whilst loading RuneScape", 30, y, color, true, 16);
        y += 50;

        color = WHITE;
        gameshell_draw_string(c->shell, "To fix this try the following (in order):", 30, y, color, true, 16);
        y += 50;

        gameshell_draw_string(c->shell, "1: Try closing ALL open web-browser windows, and reloading", 30, y, color, true, 12);
        y += 30;

        gameshell_draw_string(c->shell, "2: Try clearing your web-browsers cache from tools->internet options", 30, y, color, true, 12);
        y += 30;

        gameshell_draw_string(c->shell, "3: Try using a different game-world", 30, y, color, true, 12);
        y += 30;

        gameshell_draw_string(c->shell, "4: Try rebooting your computer", 30, y, color, true, 12);
        y += 30;

        gameshell_draw_string(c->shell, "5: Try selecting a different version of Java from the play-game menu", 30, y, color, true, 12);
    }

    if (c->error_host) {
        c->flame_active = false;
        color = WHITE;
        gameshell_draw_string(c->shell, "Error - unable to load game!", 50, 50, color, true, 20);
        gameshell_draw_string(c->shell, "To play RuneScape make sure you play from", 50, 100, color, true, 20);
        gameshell_draw_string(c->shell, "http://www.runescape.com", 50, 150, color, true, 20);
    }

    if (c->error_started) {
        c->flame_active = false;
        int y = 35;

        color = YELLOW;
        gameshell_draw_string(c->shell, "Error a copy of RuneScape already appears to be loaded", 30, y, color, true, 13);
        y += 50;

        color = WHITE;
        gameshell_draw_string(c->shell, "To fix this try the following (in order):", 30, y, color, true, 13);
        y += 50;

        gameshell_draw_string(c->shell, "1: Try closing ALL open web-browser windows, and reloading", 30, y, color, true, 12);
        y += 30;

        gameshell_draw_string(c->shell, "2: Try rebooting your computer, and reloading", 30, y, color, true, 12);
        y += 30;
    }

    platform_update_surface();
}

void client_draw_title_screen(Client *c) {
    client_load_title(c);
    pixmap_bind(c->image_title4);
    pix8_draw(c->image_titlebox, 0, 0);

    int w = 360;
    int h = 200;

    if (c->title_screen_state == 0) {
        int y = h / 2 - 20;
        drawStringTaggableCenter(c->font_bold12, "Welcome to RuneScape", w / 2, y, YELLOW, true);

        int x = w / 2 - 80;
        y = h / 2 + 20;

        pix8_draw(c->image_titlebutton, x - 73, y - 20);
        drawStringTaggableCenter(c->font_bold12, "New user", x, y + 5, WHITE, true);

        x = w / 2 + 80;
        pix8_draw(c->image_titlebutton, x - 73, y - 20);
        drawStringTaggableCenter(c->font_bold12, "Existing User", x, y + 5, WHITE, true);
    } else if (c->title_screen_state == 2) {
        int y = h / 2 - 40;
        if (strlen(c->login_message0) > 0) {
            drawStringTaggableCenter(c->font_bold12, c->login_message0, w / 2, y - 15, YELLOW, true);
            drawStringTaggableCenter(c->font_bold12, c->login_message1, w / 2, y, YELLOW, true);
            y += 30;
        } else {
            drawStringTaggableCenter(c->font_bold12, c->login_message1, w / 2, y - 7, YELLOW, true);
            y += 30;
        }

        char username[MAX_STR];
        sprintf(username, "Username: %s%s", c->username, c->title_login_field == 0 && _Client.loop_cycle % 40 < 20 ? "@yel@|" : "");
        drawStringTaggable(c->font_bold12, w / 2 - 90, y, username, WHITE, true);
        y += 15;

        char password[MAX_STR];
        char *censored = jstring_to_asterisk(c->password);
        sprintf(password, "Password: %s%s", censored, c->title_login_field == 1 && _Client.loop_cycle % 40 < 20 ? "@yel@|" : "");
        free(censored);
        drawStringTaggable(c->font_bold12, w / 2 - 88, y, password, WHITE, true);
        y += 15;

        int x = w / 2 - 80;
        y = h / 2 + 50;
        pix8_draw(c->image_titlebutton, x - 73, y - 20);
        drawStringTaggableCenter(c->font_bold12, "Login", x, y + 5, WHITE, true);

        x = w / 2 + 80;
        pix8_draw(c->image_titlebutton, x - 73, y - 20);
        drawStringTaggableCenter(c->font_bold12, "Cancel", x, y + 5, WHITE, true);
    } else if (c->title_screen_state == 3) {
        drawStringTaggableCenter(c->font_bold12, "Create a free account", w / 2, h / 2 - 60, YELLOW, true);

        int y = h / 2 - 35;
        drawStringTaggableCenter(c->font_bold12, "To create a new account you need to", w / 2, y, WHITE, true);
        y += 15;

        drawStringTaggableCenter(c->font_bold12, "go back to the main RuneScape webpage", w / 2, y, WHITE, true);
        y += 15;

        drawStringTaggableCenter(c->font_bold12, "and choose the red 'create account'", w / 2, y, WHITE, true);
        y += 15;

        drawStringTaggableCenter(c->font_bold12, "button at the top right of that page.", w / 2, y, WHITE, true);
        y += 15;

        int x = w / 2;
        y = h / 2 + 50;
        pix8_draw(c->image_titlebutton, x - 73, y - 20);
        drawStringTaggableCenter(c->font_bold12, "Cancel", x, y + 5, WHITE, true);
    }

    pixmap_draw(c->image_title4, 214, 186);
    if (c->redraw_background) {
        c->redraw_background = false;
        pixmap_draw(c->image_title2, 128, 0);
        pixmap_draw(c->image_title3, 214, 386);
        pixmap_draw(c->image_title5, 0, 265);
        pixmap_draw(c->image_title6, 574, 265);
        pixmap_draw(c->image_title7, 128, 186);
        pixmap_draw(c->image_title8, 574, 186);
    }
}

void client_unload(Client *c) {
    bump_allocator_free();

    model_free_global();
    animbase_free_global();
    animframe_free_global();
    component_free_global();
    pix3d_free_global();
    tone_free_global();
    wave_free_global();
    objtype_free_global();
    loctype_free_global();
    npctype_free_global();
    seqtype_free_global();
    varptype_free_global();
    playerentity_free_global();
    spotanimtype_free_global();
    idktype_free_global();
    flotype_free_global();
    packet_free_global();
    world3d_free_global();

    client_free(c);
}

#ifdef __EMSCRIPTEN__
#include "emscripten.h"

EM_JS(bool, get_host_js, (char *socketip, size_t len, int *http_port), {
    const url = new URL(window.location.href);
    stringToUTF8(url.hostname, socketip, len);
    if (url.port && url.hostname != 'localhost' && url.hostname != '127.0.0.1') {
        HEAP32[http_port >> 2] = parseInt(url.port, 10);
    }
    const secured = url.protocol == 'https';
    const protocol = secured ? 'wss' : 'ws';
    // TODO: check https://github.com/emscripten-core/emscripten/issues/22969
    SOCKFS.websocketArgs = {'url' : protocol + '://'};
    return secured;
})

static bool secured = false;
#endif

int main(int argc, char **argv) {
    // init screen before logging is required for some platforms
    if (!platform_init()) {
        rs2_error("Failed to init platform!\n");
        rs2_sleep(5000);
        return 1;
    }
    // to print argv on emscripten you need to print index to flush instead of just \n?
    rs2_log("RS2 user client - release #%d\n", _Client.clientversion);

#ifdef __EMSCRIPTEN__
    // we fetch instead of preload config.ini to avoid leaking account details
    emscripten_wget("config.ini", "config.ini");

    if (argc != 5) {
        if (load_ini_args()) {
            _Client.lowmem ? client_set_lowmem() : client_set_highmem();
            goto init;
        }
        rs2_error("Usage: node-id, port-offset, [lowmem/highmem], [free/members]\n");
        return 0;
    }

    secured = get_host_js(_Client.socketip, MAX_STR - 1, &_Custom.http_port);
    _Client.nodeid = atoi(argv[1]);
    _Client.portoff = atoi(argv[2]);

    const char *lowmem = argv[3];
    if (lowmem && strcmp(lowmem, "1") == 0) {
        client_set_lowmem();
    } else {
        client_set_highmem();
    }

    const char *_free = argv[4];
    _Client.members = !_free || strcmp(_free, "1") != 0;
#else
    if (argc == 1 && load_ini_args()) {
        _Client.lowmem ? client_set_lowmem() : client_set_highmem();
        goto init;
    }

    if (argc != 5) {
        rs2_error("Usage: node-id, port-offset, [lowmem/highmem], [free/members]\n");
        return 0;
    } else {
        _Client.nodeid = atoi(argv[1]);
        _Client.portoff = atoi(argv[2]);
        if (strcmp(argv[3], "lowmem") == 0) {
            client_set_lowmem();
        } else {
            if (strcmp(argv[3], "highmem") != 0) {
                rs2_error("Usage: node-id, port-offset, [lowmem/highmem], [free/members]\n");
                return 0;
            }
            client_set_highmem();
        }
        if (strcmp(argv[4], "free") == 0) {
            _Client.members = false;
        } else {
            if (strcmp(argv[4], "members") != 0) {
                rs2_error("Usage: node-id, port-offset, [lowmem/highmem], [free/members]\n");
                return 0;
            }
            _Client.members = true;
        }
    }
#endif

init:
    srand(0);
    client_init_global();
    model_init_global();
    packet_init_global();
    pix3d_init_global();
    pixfont_init_global();
    playerentity_init_global();
    world_init_global();
    world3d_init_global();

    Client *c = client_new();
    load_ini_config(c);
    gameshell_init_application(c, SCREEN_WIDTH, SCREEN_HEIGHT);
    return 0;
}

void client_free(Client *c) {
    free(c->stream);
    client_scenemap_free(c);
    gameshell_free(c->shell);
    pixfont_free(c->font_plain11);
    pixfont_free(c->font_plain12);
    pixfont_free(c->font_bold12);
    pixfont_free(c->font_quill8);
    jagfile_free(c->archive_title);

    if (c->area_chatback) {
        // other images are allocated at same time
        pixmap_free(c->area_chatback);
        pixmap_free(c->area_mapback);
        pixmap_free(c->area_sidebar);
        pixmap_free(c->area_viewport);
        pixmap_free(c->area_backbase1);
        pixmap_free(c->area_backbase2);
        pixmap_free(c->area_backhmid1);
    }

    if (c->image_titlebox) {
        // other images are allocated at same time
        pix8_free(c->image_titlebox);
        pix8_free(c->image_titlebutton);
#ifndef DISABLE_FLAMES
        for (int i = 0; i < 12; i++) {
            pix8_free(c->image_runes[i]);
        }
        free(c->image_runes);
        free(c->flame_gradient0);
        free(c->flame_gradient1);
        free(c->flame_gradient2);
        free(c->flame_gradient);
        free(c->flame_buffer0);
        free(c->flame_buffer1);
        free(c->flame_buffer3);
        free(c->flame_buffer2);
        pix24_free(c->image_flames_left);
        pix24_free(c->image_flames_right);
#endif
        pixmap_free(c->image_title0);
        pixmap_free(c->image_title1);
        pixmap_free(c->image_title2);
        pixmap_free(c->image_title3);
        pixmap_free(c->image_title4);
        pixmap_free(c->image_title5);
        pixmap_free(c->image_title6);
        pixmap_free(c->image_title7);
        pixmap_free(c->image_title8);
    }

    pix24_free(c->image_minimap);
    pix8_free(c->image_invback);
    pix8_free(c->image_chatback);
    pix8_free(c->image_mapback);
    pix8_free(c->image_backbase1);
    pix8_free(c->image_backbase2);
    pix8_free(c->image_backhmid1);
    for (int i = 0; i < 13; i++) {
        pix8_free(c->image_sideicons[i]);
    }
    free(c->image_sideicons);
    pix24_free(c->image_compass);
    // NOTE: null checks as pix can return null?
    for (int i = 0; i < 50; i++) {
        if (c->image_mapscene[i]) {
            pix8_free(c->image_mapscene[i]);
        }
    }
    free(c->image_mapscene);
    for (int i = 0; i < 50; i++) {
        if (c->image_mapfunction[i]) {
            pix24_free(c->image_mapfunction[i]);
        }
    }
    free(c->image_mapfunction);
    for (int i = 0; i < 20; i++) {
        if (c->image_hitmarks[i]) {
            pix24_free(c->image_hitmarks[i]);
        }
    }
    free(c->image_hitmarks);
    for (int i = 0; i < 20; i++) {
        if (c->image_headicons[i]) {
            pix24_free(c->image_headicons[i]);
        }
    }
    free(c->image_headicons);
    pix24_free(c->image_mapflag);
    for (int i = 0; i < 8; i++) {
        pix24_free(c->image_crosses[i]);
    }
    free(c->image_crosses);
    pix24_free(c->image_mapdot0);
    pix24_free(c->image_mapdot1);
    pix24_free(c->image_mapdot2);
    pix24_free(c->image_mapdot3);
    pix8_free(c->image_scrollbar0);
    pix8_free(c->image_scrollbar1);
    pix8_free(c->image_redstone1);
    pix8_free(c->image_redstone2);
    pix8_free(c->image_redstone3);
    pix8_free(c->image_redstone1h);
    pix8_free(c->image_redstone2h);
    pix8_free(c->image_redstone1v);
    pix8_free(c->image_redstone2v);
    pix8_free(c->image_redstone3v);
    pix8_free(c->image_redstone1hv);
    pix8_free(c->image_redstone2hv);
    pixmap_free(c->area_backleft1);
    pixmap_free(c->area_backleft2);
    pixmap_free(c->area_backright1);
    pixmap_free(c->area_backright2);
    pixmap_free(c->area_backtop1);
    pixmap_free(c->area_backtop2);
    pixmap_free(c->area_backvmid1);
    pixmap_free(c->area_backvmid2);
    pixmap_free(c->area_backvmid3);
    pixmap_free(c->area_backhmid2);
    free(c->compass_mask_line_offsets);
    free(c->compass_mask_line_lengths);
    free(c->minimap_mask_line_offsets);
    free(c->minimap_mask_line_lengths);
    packet_free(c->out);
    packet_free(c->in);
    packet_free(c->login);
    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        free(c->players[i]);
        if (c->player_appearance_buffer[i]) {
            packet_free(c->player_appearance_buffer[i]);
        }
    }
    for (int i = 0; i < MAX_NPC_COUNT; i++) {
        free(c->npcs[i]);
    }

    free(c->design_colors);

    linklist_free(c->projectiles);
    linklist_free(c->spotanims);
    linklist_free(c->merged_locations);
    linklist_free(c->spawned_locations);
    for (int level = 0; level < 4; level++) {
        for (int x = 0; x < 104; x++) {
            for (int z = 0; z < 104; z++) {
                if (c->level_obj_stacks[level][x][z]) {
                    linklist_free(c->level_obj_stacks[level][x][z]);
                }
            }
            free(c->level_obj_stacks[level][x]);
        }
        free(c->level_obj_stacks[level]);
    }
    free(c->level_obj_stacks);
    linklist_free(c->locList);
    free(c->levelTileFlags);
    free(c->levelHeightmap);
    world3d_free(c->scene, 104, 4, 104);
    for (int level = 0; level < 4; level++) {
        collisionmap_free(c->levelCollisionMap[level]);
    }
    free(c->chat_interface);
    free(c->textureBuffer);
    free(c->area_chatback_offsets);
    free(c->area_sidebar_offsets);
    free(c->area_viewport_offsets);

    free(c);
}

Client *client_new(void) {
    Client *c = calloc(1, sizeof(Client));

    c->shell = gameshell_new();
    c->image_sideicons = calloc(13, sizeof(Pix8 *));
    c->image_mapscene = calloc(50, sizeof(Pix8 *));
    c->image_mapfunction = calloc(50, sizeof(Pix24 *));
    c->image_hitmarks = calloc(20, sizeof(Pix24 *));
    c->image_headicons = calloc(20, sizeof(Pix24 *));
    c->image_crosses = calloc(8, sizeof(Pix24 *));
    c->compass_mask_line_offsets = calloc(33, sizeof(int));
    c->compass_mask_line_lengths = calloc(33, sizeof(int));
    c->minimap_mask_line_offsets = calloc(151, sizeof(int));
    c->minimap_mask_line_lengths = calloc(151, sizeof(int));

    c->out = packet_alloc(1);
    c->in = packet_alloc(1);
    c->login = packet_alloc(1);
    c->orbit_camera_pitch = 128;

    c->minimap_level = -1;
    c->sticky_chat_interface_id = -1;
    c->chat_interface_id = -1;
    c->viewport_interface_id = -1;
    c->sidebar_interface_id = -1;
    c->selected_tab = 3;
    c->flashing_tab = -1;
    c->design_gender_male = true;
    c->design_colors = malloc(5 * sizeof(int));
    memset(c->tab_interface_id, -1, sizeof(c->tab_interface_id));
    c->projectiles = linklist_new();
    c->spotanims = linklist_new();
    c->merged_locations = linklist_new();
    c->spawned_locations = linklist_new();
    c->level_obj_stacks = calloc(4, sizeof(LinkList ***));
    for (int level = 0; level < 4; level++) {
        c->level_obj_stacks[level] = calloc(104, sizeof(LinkList **));
        for (int x = 0; x < 104; x++) {
            c->level_obj_stacks[level][x] = calloc(104, sizeof(LinkList *));
            for (int z = 0; z < 104; z++) {
                c->level_obj_stacks[level][x][z] = linklist_new();
            }
        }
    }
    c->chat_interface = calloc(1, sizeof(Component));
    c->locList = linklist_new();
    c->local_pid = -1;
    c->chat_scroll_height = 78;
    c->cameraOffsetXModifier = 2;
    c->cameraOffsetZModifier = 2;
    c->cameraOffsetYawModifier = 1;
    c->minimapAngleModifier = 2;
    c->minimapZoomModifier = 1;
    c->reportAbuseInterfaceID = -1;
    c->projectX = -1;
    c->projectY = -1;
    c->textureBuffer = calloc(16384, sizeof(int8_t));
    c->wave_enabled = true;
    c->midiActive = true;
    return c;
}

#if defined(__EMSCRIPTEN__) && (!defined(SDL) || SDL == 0)
void *client_openurl(const char *name, int *size) {
    void *buffer = NULL;
    int error = 0;
    char url[PATH_MAX];
    sprintf(url, "%s://%s:%d/%s", secured ? "https" : "http", _Client.socketip, _Custom.http_port, name);
    emscripten_wget_data(url, &buffer, size, &error);
    if (error) {
        rs2_error("Error downloading %s: %d\n", url, error);
        return NULL;
    }
    return buffer;
}

Jagfile *load_archive(Client *c, const char *name, int crc, const char *display_name, int progress) {
    int retry = 5;
    // int8_t *data = signlink.cacheload(name);
    int8_t *data = NULL; // TODO cacheload
    int size = 0;
    if (data) {
        int crc_value = rs_crc32(data, size);
        if (crc_value != crc) {
            rs2_error("%s archive CRC check failed\n", display_name);
            free(data);
            data = NULL;
            size = 0;
        }
    }

    if (data) {
        return jagfile_new(data, size);
    }

    while (!data) {
        char message[PATH_MAX];
        snprintf(message, sizeof(message), "Requesting %s", display_name);
        client_draw_progress(c, message, progress);

        snprintf(message, sizeof(message), "%s%d", name, crc);
        data = client_openurl(message, &size);
        if (!data) {
            for (int i = retry; i > 0; i--) {
                snprintf(message, sizeof(message), "Error loading - Will retry in %d secs.", i);
                client_draw_progress(c, message, progress);
                rs2_sleep(1000);
            }

            retry *= 2;
            if (retry > 60) {
                retry = 60;
            }
        }
    }

    // signlink.cachesave(name, data);
    return jagfile_new(data, size);
}
#else
void *client_openurl(const char *name, int *size) {
    (void)name, (void)size;
    return NULL;
}
Jagfile *load_archive(Client *c, const char *name, int crc, const char *display_name, int progress) {
    // TODO
    (void)display_name, (void)progress;
    int8_t *data;
    int8_t *header = malloc(6);
    char filename[PATH_MAX];
#ifdef _arch_dreamcast
    snprintf(filename, sizeof(filename), "cache/client/%s.", name);
#elif defined(NXDK)
    snprintf(filename, sizeof(filename), "D:\\cache\\client\\%s", name);
#else
    snprintf(filename, sizeof(filename), "cache/client/%s", name);
#endif
    // rs2_log("Loading %s\n", filename);
    // TODO: add load messages?
    (void)c;

#ifdef ANDROID
    SDL_RWops *file = SDL_RWFromFile(filename, "rb");
#else
    FILE *file = fopen(filename, "rb");
#endif
    if (!file) {
        rs2_error("Failed to open file\n", strerror(errno));
        free(header);
        return NULL;
    }

#ifdef ANDROID
    if (SDL_RWread(file, header, 1, 6) != 6) {
#else
    if (fread(header, 1, 6, file) != 6) {
#endif
        rs2_error("Failed to read header\n", strerror(errno));
    }
    Packet *packet = packet_new(header, 6);
    packet->pos = 3;
    int file_size = g3(packet) + 6;
    int total_read = 6;
    data = malloc(file_size);
    memcpy(data, header, total_read); // or packet->data instead of header
    size_t remaining = file_size - total_read;
#ifdef ANDROID
    if (SDL_RWread(file, data + total_read, 1, remaining) != remaining) {
#else
    if (fread(data + total_read, 1, remaining, file) != remaining) {
#endif
        rs2_error("Failed to read file\n", strerror(errno));
    }
#ifdef ANDROID
    SDL_RWclose(file);
#else
    fclose(file);
#endif
    packet_free(packet);

    int crc_value = rs_crc32(data, file_size);
    if (crc_value != crc) {
        rs2_error("%s archive CRC check failed\n", display_name);
        // NOTE: we don't download cache on desktop but it's ok to ignore
        // free(data);
        // data = NULL;
    }

    return jagfile_new(data, file_size);
}
#endif

void client_load_title(Client *c) {
    if (c->image_title2) {
        return;
    }

    if (c->shell->draw_area) {
        pixmap_free(c->shell->draw_area);
        c->shell->draw_area = NULL;
    }
    if (c->area_chatback) {
        // other images are allocated at same time
        pixmap_free(c->area_chatback);
        pixmap_free(c->area_mapback);
        pixmap_free(c->area_sidebar);
        pixmap_free(c->area_viewport);
        pixmap_free(c->area_backbase1);
        pixmap_free(c->area_backbase2);
        pixmap_free(c->area_backhmid1);
        c->area_chatback = NULL;
        c->area_mapback = NULL;
        c->area_sidebar = NULL;
        c->area_viewport = NULL;
        c->area_backbase1 = NULL;
        c->area_backbase2 = NULL;
        c->area_backhmid1 = NULL;
    }

    c->image_title0 = pixmap_new(128, 265);
    pix2d_clear();

    c->image_title1 = pixmap_new(128, 265);
    pix2d_clear();

    c->image_title2 = pixmap_new(533, 186);
    pix2d_clear();

    c->image_title3 = pixmap_new(360, 146);
    pix2d_clear();

    c->image_title4 = pixmap_new(360, 200);
    pix2d_clear();

    c->image_title5 = pixmap_new(214, 267);
    pix2d_clear();

    c->image_title6 = pixmap_new(215, 267);
    pix2d_clear();

    c->image_title7 = pixmap_new(86, 79);
    pix2d_clear();

    c->image_title8 = pixmap_new(87, 79);
    pix2d_clear();

    if (c->archive_title) {
        client_load_title_background(c);
        client_load_title_images(c);
    }

    c->redraw_background = true;
}

void client_draw_progress(Client *c, const char *message, int progress) {
    client_load_title(c);
    if (!c->archive_title) {
        gameshell_draw_progress(c->shell, message, progress);
    } else {
        pixmap_bind(c->image_title4);
        int x = 360;
        int y = 200;
        int offsetY = 20;
        drawStringCenter(c->font_bold12, x / 2, y / 2 - offsetY - 26, "RuneScape is loading - please wait...", WHITE);
        int midY = y / 2 - offsetY - 18;
        pix2d_draw_rect(x / 2 - 152, midY, PROGRESS_RED, 304, 34);
        pix2d_draw_rect(x / 2 - 151, midY + 1, BLACK, 302, 32);
        pix2d_fill_rect(x / 2 - 150, midY + 2, PROGRESS_RED, progress * 3, 30);
        pix2d_fill_rect(x / 2 - 150 + progress * 3, midY + 2, BLACK, 300 - progress * 3, 30);
        drawStringCenter(c->font_bold12, x / 2, y / 2 + 5 - offsetY, message, WHITE);
        pixmap_draw(c->image_title4, 214, 186);
        if (c->redraw_background) {
            c->redraw_background = false;
            if (!c->flame_active) {
                pixmap_draw(c->image_title0, 0, 0);
                pixmap_draw(c->image_title1, 661, 0);
            }
            pixmap_draw(c->image_title2, 128, 0);
            pixmap_draw(c->image_title3, 214, 386);
            pixmap_draw(c->image_title5, 0, 265);
            pixmap_draw(c->image_title6, 574, 265);
            pixmap_draw(c->image_title7, 128, 186);
            pixmap_draw(c->image_title8, 574, 186);
        }
    }

#ifdef __EMSCRIPTEN__
    platform_update_surface();
#endif
}
#endif
