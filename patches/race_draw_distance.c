#include "patches.h"
#include "PR/gu.h"

#define RACE_CAMERA_FAR_PLANE 10000.0f
#define RACE_CAMERA_CULL_DISTANCE_FIXED 0x27100000

typedef struct {
    u8 padding[0x134];
    u32 cameraX;
    u32 cameraY;
    u32 cameraZ;
    u8 pad14C[4];
    u8 defaultLight1R;
    u8 defaultLight1G;
    u8 defaultLight1B;
    u8 pad14B;
    u8 pad14C_2[3];
    u8 defaultLight2R;
    u8 defaultLight2G;
    u8 defaultLight2B;
} ActiveViewportOverlay;

extern ViewportNode gRootViewport;
extern ActiveViewportOverlay* gActiveViewport;

RECOMP_PATCH void setViewportPerspective(ViewportNode* node, f32 fov, f32 aspect, f32 near, f32 far) {
    guPerspective(&node->perspectiveMatrix, &node->perspNorm, fov, aspect, near, far, 1.0f);
}

RECOMP_PATCH s32 isObjectCulled(Vec3i* arg0) {
    return FALSE;
}

RECOMP_PATCH void setViewportFogById(u16 viewportId, s16 fogMin, s16 fogMax, u8 fogR, u8 fogG, u8 fogB) {
    ViewportNode* node = gRootViewport.unk8.list2_next;

    while (node != NULL) {
        if (node->id == viewportId) {
            node->fogMin = fogMin + 10;
            node->fogMax = fogMax + 10;

            node->fogR = fogR;
            node->fogG = fogG;
            node->fogB = fogB;
        }

        node = node->unk8.list2_next;
    }
}
