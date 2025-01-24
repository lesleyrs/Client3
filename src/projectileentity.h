#pragma once

#include "entity.h"
#include "spotanimtype.h"

typedef struct ProjectileEntity {
    Entity entity;
    SpotAnimType *spotanim;
    int level;
    int srcX;
    int srcZ;
    int srcY;
    int offsetY;
    int startCycle;
    int lastCycle;
    int peakPitch;
    int arc;
    int target;
    bool mobile; // = false;
    double x;
    double z;
    double y;
    double velocityX;
    double velocityZ;
    double velocity;
    double velocityY;
    double accelerationY;
    int yaw;
    int pitch;
    int seqFrame;
    int seqCycle;
} ProjectileEntity;

ProjectileEntity *projectileentity_new(int spotanim, int level, int srcX, int srcY, int srcZ, int startCycle, int lastCycle, int peakPitch, int arc, int target, int offsetY);
void projectileentity_update_velocity(ProjectileEntity *entity, int dstX, int dstY, int dstZ, int cycle);
void projectileentity_update(ProjectileEntity *entity, int delta);
Model *projectileentity_draw(ProjectileEntity *entity, int loopCycle);
