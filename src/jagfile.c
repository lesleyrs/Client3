#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "jagfile.h"
#include "thirdparty/bzip.h"

static Jagfile *jagfile_parse(int8_t *src, int length);
static int8_t *jagfile_read_index(Jagfile *jagfile, int id);
static int jagfile_read(Jagfile *jagfile, const char *name);

Jagfile *jagfile_new(int8_t *src, int length) {
    return jagfile_parse(src, length);
}

void jagfile_free(Jagfile *jagfile) {
    free(jagfile->data);
    free(jagfile->file_hash);
    free(jagfile->file_unpacked_size);
    free(jagfile->file_packed_size);
    free(jagfile->file_offset);
    free(jagfile);
}

Packet *jagfile_to_packet(Jagfile *jagfile, const char *name) {
    const int id = jagfile_read(jagfile, name);
    return packet_new(jagfile_read_index(jagfile, id), jagfile->file_unpacked_size[id]);
}

uint8_t *jagfile_to_bytes(Jagfile *jagfile, const char* name, int *length) {
    const int id = jagfile_read(jagfile, name);
    if (length) {
        *length = jagfile->file_unpacked_size[id];
    }
    return (uint8_t*) jagfile_read_index(jagfile, id);
}

static Jagfile *jagfile_parse(int8_t *src, int length) {
    Packet *packet = packet_new(src, length);
    int unpacked_size = g3(packet);
    int packed_size = g3(packet);

    Jagfile *jagfile = calloc(1, sizeof(Jagfile));

    if (packed_size == unpacked_size) {
        jagfile->data = src;
        jagfile->is_compressed_whole = false;
    } else {
        int8_t *data = calloc(unpacked_size, sizeof(int8_t));
        bzip_decompress(data, src, packed_size, 6);
        jagfile->data = data;
        packet_free(packet);
        packet = packet_new(jagfile->data, unpacked_size);
        jagfile->is_compressed_whole = true;
    }
    jagfile->file_count = g2(packet);
    jagfile->file_hash = calloc(jagfile->file_count, sizeof(int));
    jagfile->file_unpacked_size = calloc(jagfile->file_count, sizeof(int));
    jagfile->file_packed_size = calloc(jagfile->file_count, sizeof(int));
    jagfile->file_offset = calloc(jagfile->file_count, sizeof(int));
    int offset = packet->pos + jagfile->file_count * 10;
    for (int i = 0; i < jagfile->file_count; i++) {
        jagfile->file_hash[i] = g4(packet);
        jagfile->file_unpacked_size[i] = g3(packet);
        jagfile->file_packed_size[i] = g3(packet);
        jagfile->file_offset[i] = offset;
        offset += jagfile->file_packed_size[i];
    }
    free(packet);
    return jagfile;
}

static int jagfile_read(Jagfile *jagfile, const char *name) {
    int hash = 0;
    for (size_t i = 0; i < strlen(name); i++) {
        hash = hash * 61 + toupper(name[i]) - 32;
    }
    for (int id = 0; id < jagfile->file_count; id++) {
        if (jagfile->file_hash[id] == hash) {
            return id;
        }
    }
    // NOTE custom, need index again when creating packet/jpeg to get the length
    return -1;
}

static int8_t *jagfile_read_index(Jagfile *jagfile, int id) {
    int8_t *dest = malloc(jagfile->file_unpacked_size[id]);
    if (jagfile->is_compressed_whole) {
        memcpy(dest, jagfile->data + jagfile->file_offset[id], jagfile->file_unpacked_size[id]);
    } else {
        bzip_decompress(dest, jagfile->data, jagfile->file_packed_size[id], jagfile->file_offset[id]);
    }
    return dest;
}
