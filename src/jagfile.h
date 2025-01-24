#pragma once

#include "packet.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int8_t *data;
    int file_count;
    int *file_hash;
    int *file_unpacked_size;
    int *file_packed_size;
    int *file_offset;
    bool is_compressed_whole;
} Jagfile;

Jagfile *jagfile_new(int8_t *src, int length);
void jagfile_free(Jagfile *jagfile);
Packet *jagfile_to_packet(Jagfile *jagfile, const char *name);
// NOTE can't make those static as pix24 uses them directly with stb
int8_t *jagfile_read_index(Jagfile *jagfile, int id);
int jagfile_read(Jagfile *jagfile, const char *name);
