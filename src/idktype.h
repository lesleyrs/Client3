#pragma once

#include "jagfile.h"
#include "model.h"

// name taken from rs3
typedef struct {
    int type; // = -1;
    int *models;
    int recol_s[6]; // = new int[6];
    int recol_d[6]; // = new int[6];
    int heads[5];   // = new int[] { -1, -1, -1, -1, -1 };
    bool disable;   // = false;

    int models_count;
} IdkType;

typedef struct {
    int count;
    IdkType **instances;
} IdkTypeData;

void idktype_unpack(Jagfile *config);
void idktype_free_global(void);
Model *idktype_get_model(IdkType *idk);
Model *idktype_get_headmodel(IdkType *idk);
