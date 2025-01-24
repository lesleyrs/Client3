#include <stdlib.h>

#include "animbase.h"
#include "jagfile.h"
#include "packet.h"

AnimBaseData _AnimBase = {0};

void animbase_free_global(void) {
    for (int i = 0; i < _AnimBase.count; i++) {
        if (_AnimBase.instances[i]) {
            free(_AnimBase.instances[i]->types);
            for (int j = 0; j < _AnimBase.instances[i]->length; j++) {
                free(_AnimBase.instances[i]->labels[j]);
            }
            free(_AnimBase.instances[i]->labels);
            free(_AnimBase.instances[i]->labels_count);
            free(_AnimBase.instances[i]);
        }
    }
    free(_AnimBase.instances);
}

void animbase_unpack(Jagfile *models) {
    Packet *head = jagfile_to_packet(models, "base_head.dat");
    Packet *type = jagfile_to_packet(models, "base_type.dat");
    Packet *label = jagfile_to_packet(models, "base_label.dat");

    const int total = g2(head);
    const int count = g2(head);
    _AnimBase.count = count + 1;
    _AnimBase.instances = calloc(_AnimBase.count, sizeof(AnimBase *));
    for (int i = 0; i < total; i++) {
        int id = g2(head);
        int length = g1(head);
        int *transform_types = calloc(length, sizeof(int));
        int **group_labels = calloc(length, sizeof(int *));
        int *labels_count = calloc(length, sizeof(int));
        for (int j = 0; j < length; j++) {
            transform_types[j] = g1(type);
            const int group_count = g1(label);
            labels_count[j] = group_count;
            group_labels[j] = calloc(group_count, sizeof(int));
            for (int k = 0; k < group_count; k++) {
                group_labels[j][k] = g1(label);
            }
        }
        _AnimBase.instances[id] = calloc(1, sizeof(AnimBase));
        _AnimBase.instances[id]->length = length;
        _AnimBase.instances[id]->types = transform_types;
        _AnimBase.instances[id]->labels = group_labels;
        _AnimBase.instances[id]->labels_count = labels_count;
    }

    packet_free(head);
    packet_free(type);
    packet_free(label);
}
