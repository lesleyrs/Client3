#ifdef playground
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../client.h"
#include "../defines.h"
#include "../gameshell.h"
#include "../jagfile.h"
#include "../model.h"
#include "../packet.h"
#include "../pix3d.h"
#include "../pixmap.h"

ClientData _Client = {0};
extern Pix3D _Pix3D;

const int metadata_count = 3554;
int lastHistoryRefresh = 0;
bool historyRefresh = true;

int eyeX = 0;
int eyeY = 0;
int eyeZ = 0;
int eyePitch = 0;
int eyeYaw = 0;

int modifier = 2;
// TODO: if using separate html files for emscripten, could parse these from url
int model_id = 0;
int model_x = 0;
int model_y = 0;
int model_z = 420;
int model_yaw = 0;

Jagfile *load_archive_simple(const char *name, int crc, const char *display_name, int progress);

void client_run_flames(Client *c) {
    (void)c;
}

void client_load(Client *c) {
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

    c->archive_title = load_archive_simple("title", c->archive_checksum[1], "title screen", 10);

    c->font_plain11 = pixfont_from_archive(c->archive_title, "p11");
    c->font_plain12 = pixfont_from_archive(c->archive_title, "p12");
    c->font_bold12 = pixfont_from_archive(c->archive_title, "b12");
    c->font_quill8 = pixfont_from_archive(c->archive_title, "q8");

    Jagfile *config = load_archive_simple("config", c->archive_checksum[2], "config", 15);
    Jagfile *interface_archive = load_archive_simple("interface", c->archive_checksum[3], "interface", 20);
    Jagfile *media = load_archive_simple("media", c->archive_checksum[4], "2d graphics", 30);
    Jagfile *models = load_archive_simple("models", c->archive_checksum[5], "3d graphics", 40);
    Jagfile *textures = load_archive_simple("textures", c->archive_checksum[6], "textures", 60);
    Jagfile *wordenc = load_archive_simple("wordenc", c->archive_checksum[7], "chat system", 65);
    Jagfile *sounds = load_archive_simple("sounds", c->archive_checksum[8], "sound effects", 70);

    gameshell_draw_progress(c->shell, "Unpacking media", 75);

    gameshell_draw_progress(c->shell, "Unpacking textures", 80);
    pix3d_unpack_textures(textures);
    pix3d_set_brightness(0.8);
    pix3d_init_pool(20);

    gameshell_draw_progress(c->shell, "Unpacking models", 83);
    model_unpack(models);
    // animbase_unpack(models);
    // animframe_unpack(models);

    gameshell_draw_progress(c->shell, "Unpacking config", 86);
    // SeqType.unpack(config);
    // LocType.unpack(config);
    // FloType.unpack(config);
    // ObjType.unpack(config);
    // NpcType.unpack(config);
    // IdkType.unpack(config);
    // SpotAnimType.unpack(config);
    // VarpType.unpack(config);

    if (!_Client.lowmem) {
        gameshell_draw_progress(c->shell, "Unpacking sounds", 90);
        // byte[] data = sounds.read("sounds.dat", null);
        // Packet soundDat = new Packet(data);
        // wave_unpack(soundDat);
    }

    gameshell_draw_progress(c->shell, "Unpacking interfaces", 92);
    // PixFont *fonts[] = {c->font_plain11, c->font_plain12, c->font_bold12, c->font_quill8};
    // component_unpack(media, inter, fonts);

    gameshell_draw_progress(c->shell, "Preparing game engine", 97);
    // wordfilter_unpack(wordenc);

    pixmap_bind(c->shell->draw_area);
    pix3d_init2d();

    jagfile_free(config);
    jagfile_free(interface_archive);
    jagfile_free(media);
    jagfile_free(models);
    jagfile_free(textures);
    jagfile_free(wordenc);
    jagfile_free(sounds);
}

static void update_keys_pressed(GameShell *shell) {
    while (true) {
        const int key = poll_key(shell);
        if (key == -1) {
            break;
        }

        if (key == 'r') {
            modifier = 2;
            historyRefresh = true;
        } else if (key == '1') {
            model_id--;
            if (model_id < 0) {
                model_id = metadata_count - 100 - 1;
            }
            historyRefresh = true;
        } else if (key == '2') {
            model_id++;
            if (model_id >= metadata_count - 100) {
                model_id = 0;
            }
            historyRefresh = true;
        }
    }
}

static void update_keys_held(GameShell *shell) {
    if (shell->action_key['[']) {
        modifier--;
    } else if (shell->action_key[']']) {
        modifier++;
    }

    if (shell->action_key[1]) {
        // left arrow
        model_yaw += modifier;
        historyRefresh = true;
    } else if (shell->action_key[2]) {
        // right arrow
        model_yaw -= modifier;
        historyRefresh = true;
    }

    if (shell->action_key['w']) {
        model_z -= modifier;
        historyRefresh = true;
    } else if (shell->action_key['s']) {
        model_z += modifier;
        historyRefresh = true;
    }

    if (shell->action_key['a']) {
        model_x -= modifier;
        historyRefresh = true;
    } else if (shell->action_key['d']) {
        model_x += modifier;
        historyRefresh = true;
    }

    if (shell->action_key['q']) {
        model_y += modifier;
        historyRefresh = true;
    } else if (shell->action_key['e']) {
        model_y -= modifier;
        historyRefresh = true;
    }

    eyePitch = eyePitch & 2047;
    eyeYaw = eyeYaw & 2047;
    model_yaw = model_yaw & 2047;
}

void client_update(Client *c) {
    platform_poll_events(c);
    update_keys_pressed(c->shell);
    update_keys_held(c->shell);

    lastHistoryRefresh++;

    if (lastHistoryRefresh > 50) {
        if (historyRefresh) {
            // GameShell.setParameter('model', this.model.id.toString());

            historyRefresh = false;
        }

        lastHistoryRefresh = 0;
    }
}

void client_draw(Client *c) {
    pix2d_clear();
    pix2d_fill_rect(0, 0, 0x555555, c->shell->screen_width, c->shell->screen_height);

    // draw a model
    Model *model = model_from_id(model_id, false);
    model_calculate_normals(model, 64, 850, -30, -50, -30, true, false);
    model_draw(model, model_yaw, _Pix3D.sin_table[eyePitch], _Pix3D.cos_table[eyePitch], _Pix3D.sin_table[eyeYaw], _Pix3D.cos_table[eyeYaw], model_x - eyeX, model_y - eyeY, model_z - eyeZ, 0);
    model_free(model);

    // debug
    char buffer[8];
    sprintf(buffer, "FPS: %d", c->shell->fps);
    drawStringRight(c->font_bold12, c->shell->screen_width, c->font_bold12->height, buffer, YELLOW, true);

    // controls
    int leftY = c->font_bold12->height;
    char buffer2[16];
    sprintf(buffer2, "Model: %d", model_id);
    drawString(c->font_bold12, 0, leftY, buffer2, YELLOW);
    leftY += c->font_bold12->height;
    drawString(c->font_bold12, 0, leftY, "Controls:", YELLOW);
    leftY += c->font_bold12->height;
    drawString(c->font_bold12, 0, leftY, "r - reset camera and model rotation + movement speed", YELLOW);
    leftY += c->font_bold12->height;
    drawString(c->font_bold12, 0, leftY, "1 and 2 - change model", YELLOW);
    leftY += c->font_bold12->height;
    drawString(c->font_bold12, 0, leftY, "[ and ] - adjust movement speed", YELLOW);
    leftY += c->font_bold12->height;
    drawString(c->font_bold12, 0, leftY, "left and right - adjust model yaw", YELLOW);
    leftY += c->font_bold12->height;
    drawString(c->font_bold12, 0, leftY, "up and down - adjust model pitch", YELLOW);
    leftY += c->font_bold12->height;
    drawString(c->font_bold12, 0, leftY, ". and / - adjust model roll", YELLOW);
    leftY += c->font_bold12->height;
    drawString(c->font_bold12, 0, leftY, "w and s - move camera along z axis", YELLOW);
    leftY += c->font_bold12->height;
    drawString(c->font_bold12, 0, leftY, "a and d - move camera along x axis", YELLOW);
    leftY += c->font_bold12->height;
    drawString(c->font_bold12, 0, leftY, "q and e - move camera along y axis", YELLOW);

    pixmap_draw(c->shell->draw_area, 0, 0);
}

void client_unload(Client *c) {
    model_free_global();
    // animbase_free();
    // animframe_free();
    pix3d_free_global();
    gameshell_free(c->shell);
    client_free(c);
}

int main(int argc, char **argv) {
    // TODO
    (void)argc, (void)argv;
    srand(0);
    Client *c = client_new();
    model_init_global();
    pix3d_init_global();
    pixfont_init_global();
    packet_init_global();

    gameshell_init_application(c, SCREEN_WIDTH, SCREEN_HEIGHT);
    return 0;
}

void client_free(Client *c) {
    pixfont_free(c->font_plain11);
    pixfont_free(c->font_plain12);
    pixfont_free(c->font_bold12);
    pixfont_free(c->font_quill8);
    jagfile_free(c->archive_title);
    free(c);
}

Client *client_new(void) {
    Client *c = calloc(1, sizeof(Client));

    c->shell = gameshell_new();
    _Client.lowmem = false; // TODO init others?
    return c;
}

Jagfile *load_archive_simple(const char *name, int crc, const char *display_name, int progress) {
    // TODO
    (void)display_name, (void)progress;
    int8_t *data = NULL;
    int8_t *header = malloc(6);
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "cache/client/%s", name);
    rs2_log("Loading %s\n", filename);

    FILE *file = fopen(filename, "rb");
    if (!file) {
        rs2_error("Failed to open file\n", strerror(errno));
        free(header);
        return NULL;
    }

    if (fread(header, 1, 6, file) != 6) {
        rs2_error("Failed to read header\n", strerror(errno));
    }
    Packet *packet = packet_new(header, 6);
    packet->pos = 3;
    int file_size = g3(packet) + 6;
    int total_read = 6;
    data = malloc(file_size);
    memcpy(data, header, total_read); // or packet->data instead of header

    size_t remaining = file_size - total_read;
    if (fread(data + total_read, 1, remaining, file) != remaining) {
        rs2_error("Failed to read file\n", strerror(errno));
    }
    fclose(file);
    packet_free(packet);

    int crc_value = rs_crc32(data, file_size);
    if (crc_value != crc) {
        free(data);
        data = NULL;
    }

    return jagfile_new(data, file_size);
}
#endif
