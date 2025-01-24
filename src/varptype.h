#pragma once

#include "jagfile.h"

// name derived from other types + varp.dat (rs3 has this as VarPlayerType)
typedef struct {
    const char *code10;
    int code1;
    int code2;
    bool hasCode3; // = false;
    bool code4;    // = true;
    int clientcode;
    int code7;
    bool code6; // = false;
    bool code8; // = false;
} VarpType;

typedef struct {
    int count;
    VarpType **instances;
    int code3Count;
    int *code3;
} VarpTypeData;

void varptype_unpack(Jagfile *config);
void varptype_free_global(void);
