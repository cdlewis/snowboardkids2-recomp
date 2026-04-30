#include "patches.h"
#include "PR/gu.h"

#define RACE_CAMERA_FAR_PLANE 10000.0f
#define RACE_CAMERA_CULL_DISTANCE_FIXED 0x27100000
#define RACE_FOG_MIN 500
#define RACE_FOG_MAX 32767

typedef struct {
    s32 x;
    s32 y;
    s32 z;
} RaceDrawDistanceVec3i;

static s32 is_short_race_far_plane(f32 far) {
    return far == 2000.0f || far == 3000.0f || far == 3800.0f;
}

static s32 is_race_player_camera_viewport(Node_70B00 *node) {
    Node_70B00 *parent;
    u16 playerIndex;

    if (node == NULL || node->slot_index > 3) {
        return 0;
    }

    playerIndex = node->slot_index;
    if (node->id != (u16)(0x64 + playerIndex)) {
        return 0;
    }

    parent = node->unk0.next;
    return parent != NULL && parent->slot_index == (u16)(playerIndex + 4) &&
           parent->id == node->id;
}

static s32 is_race_fog_viewport(Node_70B00 *node) {
    u16 playerIndex;

    if (node == NULL || node->id < 0x64 || node->id > 0x67) {
        return 0;
    }

    playerIndex = node->id - 0x64;
    return node->slot_index == playerIndex || node->slot_index == (u16)(playerIndex + 4);
}

RECOMP_PATCH void setViewportPerspective(Node_70B00 *node, f32 fov, f32 aspect, f32 near, f32 far) {
    if (is_race_player_camera_viewport(node) && is_short_race_far_plane(far)) {
        far = RACE_CAMERA_FAR_PLANE;
    }

    guPerspective(&node->perspectiveMatrix, &node->perspNorm, fov, aspect, near, far, 1.0f);
}

extern Node_70B00 *gActiveViewport;

static s32 race_camera_axis_is_culled(s32 cameraPos, s32 objectPos) {
    return (u32)((cameraPos - objectPos) + RACE_CAMERA_CULL_DISTANCE_FIXED) >
           (u32)(RACE_CAMERA_CULL_DISTANCE_FIXED * 2);
}

RECOMP_PATCH s32 isObjectCulled(RaceDrawDistanceVec3i *pos) {
    s32 cullDistance;
    s32 cullDistanceDouble;
    s32 cameraX;
    s32 cameraY;
    s32 cameraZ;

    if (is_race_player_camera_viewport(gActiveViewport)) {
        cameraX = *(s32 *)((u8 *)gActiveViewport + 0x134);
        cameraY = *(s32 *)((u8 *)gActiveViewport + 0x138);
        cameraZ = *(s32 *)((u8 *)gActiveViewport + 0x13C);

        return race_camera_axis_is_culled(cameraX, pos->x) ||
               race_camera_axis_is_culled(cameraY, pos->y) ||
               race_camera_axis_is_culled(cameraZ, pos->z);
    }

    cullDistance = 0x0FEA0000;
    cullDistanceDouble = 0x1FD40000;
    if ((u32)((*(s32 *)((u8 *)gActiveViewport + 0x134) - pos->x) + cullDistance) > (u32)cullDistanceDouble) {
        return 1;
    }

    if ((u32)((*(s32 *)((u8 *)gActiveViewport + 0x138) - pos->y) + cullDistance) > (u32)cullDistanceDouble) {
        return 1;
    }

    return (u32)((*(s32 *)((u8 *)gActiveViewport + 0x13C) - pos->z) + cullDistance) > (u32)cullDistanceDouble;
}

extern Node_70B00 gRootViewport;

RECOMP_PATCH void setViewportFogById(u16 viewportId, s16 fogMin, s16 fogMax, u8 fogR, u8 fogG, u8 fogB) {
    Node_70B00 *node = gRootViewport.unk8.list2_next;

    while (node != NULL) {
        if (node->id == viewportId) {
            if (is_race_fog_viewport(node)) {
                node->fogMin = RACE_FOG_MIN;
                node->fogMax = RACE_FOG_MAX;
            } else {
                node->fogMin = fogMin;
                node->fogMax = fogMax;
            }
            node->fogR = fogR;
            node->fogG = fogG;
            node->fogB = fogB;
        }

        node = node->unk8.list2_next;
    }
}
