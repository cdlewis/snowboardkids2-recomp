#pragma once

#include "PR/ultratypes.h"

// Projections: 0x00001000 - 0x00001FFF
#define PROJECTION_RACE_PLAYER_TRANSFORM_ID_START 0x00001000
#define PROJECTION_RACE_PLAYER_PARENT_TRANSFORM_ID_START 0x00001005
#define PROJECTION_PILLARBOX_TRANSFORM_ID 0x00001009

extern s32 cur_perspective_projection_transform_id;
extern s32 skip_perspective_interpolation;

typedef struct {
    s32 valid;
    s32 skipInterpolation;
    s16 prevRot[3][3];
} ViewportCameraSkipState;

void resetProjectionIds(void);
void setViewportProjectionTransformId(ViewportNode *node, s32 projectionTransformId);
s32 getViewportProjectionTransformId(ViewportNode *node);
ViewportCameraSkipState *getViewportCameraSkipState(ViewportNode *node);
