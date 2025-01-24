#include <stdlib.h>

#include "animframe.h"
#include "jagfile.h"
#include "packet.h"
#include "platform.h"
#include "seqtype.h"

SeqTypeData _SeqType = {0};
extern AnimFrameData _AnimFrame;

static void seqtype_decode(SeqType *seq, Packet *dat);

static SeqType *seqtype_new(void) {
    SeqType *seq = calloc(1, sizeof(SeqType));
    seq->replayoff = -1;
    seq->priority = 5;
    seq->righthand = -1;
    seq->lefthand = -1;
    seq->replaycount = 99;
    return seq;
}

void seqtype_free_global(void) {
    for (int i = 0; i < _SeqType.count; i++) {
        free(_SeqType.instances[i]->frames);
        free(_SeqType.instances[i]->iframes);
        free(_SeqType.instances[i]->delay);
        free(_SeqType.instances[i]->walkmerge);
        free(_SeqType.instances[i]);
    }
    free(_SeqType.instances);
}

void seqtype_unpack(Jagfile *config) {
    Packet *dat = jagfile_to_packet(config, "seq.dat");
    _SeqType.count = g2(dat);

    if (!_SeqType.instances) {
        _SeqType.instances = calloc(_SeqType.count, sizeof(SeqType *));
    }

    for (int id = 0; id < _SeqType.count; id++) {
        if (!_SeqType.instances[id]) {
            _SeqType.instances[id] = seqtype_new();
        }

        seqtype_decode(_SeqType.instances[id], dat);
    }

    packet_free(dat);
}

static void seqtype_decode(SeqType *seq, Packet *dat) {
    while (true) {
        int code = g1(dat);
        if (code == 0) {
            break;
        }

        if (code == 1) {
            seq->frameCount = g1(dat);
            seq->frames = calloc(seq->frameCount, sizeof(int));
            seq->iframes = calloc(seq->frameCount, sizeof(int));
            seq->delay = calloc(seq->frameCount, sizeof(int));

            for (int i = 0; i < seq->frameCount; i++) {
                seq->frames[i] = g2(dat);

                seq->iframes[i] = g2(dat);
                if (seq->iframes[i] == 65535) {
                    seq->iframes[i] = -1;
                }

                seq->delay[i] = g2(dat);
                if (seq->delay[i] == 0) {
                    seq->delay[i] = _AnimFrame.instances[seq->frames[i]]->delay;
                }

                if (seq->delay[i] == 0) {
                    seq->delay[i] = 1;
                }
            }
        } else if (code == 2) {
            seq->replayoff = g2(dat);
        } else if (code == 3) {
            int count = g1(dat);
            seq->walkmerge = calloc(count + 1, sizeof(int));

            for (int i = 0; i < count; i++) {
                seq->walkmerge[i] = g1(dat);
            }

            seq->walkmerge[count] = 9999999;
        } else if (code == 4) {
            seq->stretches = true;
        } else if (code == 5) {
            seq->priority = g1(dat);
        } else if (code == 6) {
            // later RS (think RS3) seq->becomes mainhand
            seq->righthand = g2(dat);
        } else if (code == 7) {
            // later RS (think RS3) seq->becomes offhand
            seq->lefthand = g2(dat);
        } else if (code == 8) {
            seq->replaycount = g1(dat);
        } else {
            rs2_error("Error unrecognised seq config code: %d\n", code);
        }
    }

    if (seq->frameCount == 0) {
        seq->frameCount = 1;

        seq->frames = calloc(1, sizeof(int));
        seq->frames[0] = -1;

        seq->iframes = calloc(1, sizeof(int));
        seq->iframes[0] = -1;

        seq->delay = calloc(1, sizeof(int));
        seq->delay[0] = -1;
    }
}
