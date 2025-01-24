#include <stdlib.h>

#include "animframe.h"
#include "packet.h"

extern AnimBaseData _AnimBase;
AnimFrameData _AnimFrame = {0};

void animframe_free_global(void) {
    for (int i = 0; i < _AnimFrame.count; i++) {
        if (_AnimFrame.instances[i]) {
            free(_AnimFrame.instances[i]->groups);
            free(_AnimFrame.instances[i]->x);
            free(_AnimFrame.instances[i]->y);
            free(_AnimFrame.instances[i]->z);
            free(_AnimFrame.instances[i]);
        }
    }
    free(_AnimFrame.instances);
}

void animframe_unpack(Jagfile *models) {
    Packet *head = jagfile_to_packet(models, "frame_head.dat");
    Packet *tran1 = jagfile_to_packet(models, "frame_tran1.dat");
    Packet *tran2 = jagfile_to_packet(models, "frame_tran2.dat");
    Packet *del = jagfile_to_packet(models, "frame_del.dat");

    const int total = g2(head);
    const int count = g2(head);
    _AnimFrame.count = count + 1;
    _AnimFrame.instances = calloc(_AnimFrame.count, sizeof(AnimFrame *));
    int *labels = calloc(500, sizeof(int));
    int *x = calloc(500, sizeof(int));
    int *y = calloc(500, sizeof(int));
    int *z = calloc(500, sizeof(int));
    for (int i = 0; i < total; i++) {
        int id = g2(head);
        AnimFrame *frame = _AnimFrame.instances[id] = calloc(1, sizeof(AnimFrame));
        frame->delay = g1(del);
        int baseId = g2(head);
        AnimBase *base = _AnimBase.instances[baseId];
        frame->base = base;
        int length = g1(head);
        int last_group = -1;
        int current = 0;
        int flags;
        for (int j = 0; j < length; j++) {
            flags = g1(tran1);
            if (flags > 0) {
                if (base->types[j] != OP_BASE) {
                    for (int group = j - 1; group > last_group; group--) {
                        if (base->types[group] == OP_BASE) {
                            labels[current] = group;
                            x[current] = 0;
                            y[current] = 0;
                            z[current] = 0;
                            current++;
                            break;
                        }
                    }
                }
                labels[current] = j;
                int default_value = 0;
                if (base->types[labels[current]] == OP_SCALE) {
                    default_value = 128;
                }
                if ((flags & 0x1) == 0) {
                    x[current] = default_value;
                } else {
                    x[current] = gsmart(tran2);
                }
                if ((flags & 0x2) == 0) {
                    y[current] = default_value;
                } else {
                    y[current] = gsmart(tran2);
                }
                if ((flags & 0x4) == 0) {
                    z[current] = default_value;
                } else {
                    z[current] = gsmart(tran2);
                }
                last_group = j;
                current++;
            }
        }
        frame->length = current;
        frame->groups = calloc(current, sizeof(int));
        frame->x = calloc(current, sizeof(int));
        frame->y = calloc(current, sizeof(int));
        frame->z = calloc(current, sizeof(int));
        for (flags = 0; flags < current; flags++) {
            frame->groups[flags] = labels[flags];
            frame->x[flags] = x[flags];
            frame->y[flags] = y[flags];
            frame->z[flags] = z[flags];
        }
    }

    free(labels);
    free(x);
    free(y);
    free(z);
    packet_free(head);
    packet_free(tran1);
    packet_free(tran2);
    packet_free(del);
}
