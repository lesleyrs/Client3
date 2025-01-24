#pragma once

#include "defines.h"
#include "entity.h"

typedef struct {
    Entity entity;
    int x;
    int z;
    int yaw;
    bool seqStretches;   // = false;
    int size;            // = 1;
    int seqStandId;      // = -1;
    int seqTurnId;       // = -1;
    int seqWalkId;       // = -1;
    int seqTurnAroundId; // = -1;
    int seqTurnLeftId;   // = -1;
    int seqTurnRightId;  // = -1;
    int seqRunId;        // = -1;
    char chat[CHAT_LENGTH + 1];
    int chatTimer; // = 100;
    int chatColor;
    int chatStyle;
    int damage;
    int damageType;
    int combatCycle; // = -1000;
    int health;
    int totalHealth;
    int targetId; // = -1;
    int targetTileX;
    int targetTileZ;
    int secondarySeqId; // = -1;
    int secondarySeqFrame;
    int secondarySeqCycle;
    int primarySeqId; // = -1;
    int primarySeqFrame;
    int primarySeqCycle;
    int primarySeqDelay;
    int primarySeqLoop;
    int spotanimId; // = -1;
    int spotanimFrame;
    int spotanimCycle;
    int spotanimLastCycle;
    int spotanimOffset;
    int forceMoveStartSceneTileX;
    int forceMoveEndSceneTileX;
    int forceMoveStartSceneTileZ;
    int forceMoveEndSceneTileZ;
    int forceMoveEndCycle;
    int forceMoveStartCycle;
    int forceMoveFaceDirection;
    int cycle;
    int height;
    int dstYaw;
    int pathLength;
    int pathTileX[10];
    int pathTileZ[10];
    bool pathRunning[10];
    int seqTrigger;

    int lastMask;      // = -1;
    int lastMaskCycle; // = -1;
    int lastFaceX;     // = -1;
    int lastFaceZ;     // = -1;
} PathingEntity;

void pathingentity_teleport(PathingEntity *entity, bool jump, int x, int z);
void pathingentity_movealongroute(PathingEntity *entity, bool running, int direction);
bool pathingentity_is_visible(PathingEntity *entity);
PathingEntity pathingentity_new(const char *type);
