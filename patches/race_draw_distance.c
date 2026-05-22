#include "patches.h"
#include "PR/gu.h"

#define RACE_PLAYER_FAR_PLANE_SCALE 10.0f

RECOMP_PATCH s32 isObjectCulled(Vec3i* arg0) {
    return FALSE;
}

RECOMP_PATCH void setViewportPerspective(ViewportNode* node, f32 fov, f32 aspect, f32 near, f32 far) {
    f32 effectiveFar = far;

    // Race player viewports appear to use ids 100-103 with slots 0-3; paired slots 4-7 keep their original far plane.
    if (node->id >= 100 && node->id < 104 && node->slot_index < 4) {
        effectiveFar = far * RACE_PLAYER_FAR_PLANE_SCALE;
    }

    guPerspective(&node->perspectiveMatrix, &node->perspNorm, fov, aspect, near, effectiveFar, 1.0f);
}
