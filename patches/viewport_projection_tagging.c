/*
 * View projection tagging
 *
 * This patch adds the ability to associate additional metadata with
 * a viewport node, which can be used to determine which projection
 * transform to use when rendering that viewport.
 */

#include "patches.h"
#include "transform_ids.h"

extern s32 recomp_get_vertical_2p_split_screen_enabled(void);

s32 gRaceUsesVerticalTwoPlayerSplit = FALSE;

typedef struct {
    ViewportNode *node;
    s32 projectionTransformId;
    u8 usesCenteredFixedAspect;
    ViewportCameraSkipState cameraSkipState;
} ViewportProjectionTag;

// Viewports are registered in gViewportCallbackPools, which can have at most 0x10 entries.
// But we're feeling generous so double to 0x20 just to be safe.
static ViewportProjectionTag viewportProjectionTags[0x20];

extern ViewportNode *gViewportCallbackPools[];

static void registerViewportProjectionSlot(ViewportNode *node) {
    u16 slot = node->callbackSlotIndex;

    viewportProjectionTags[slot].node = node;
    viewportProjectionTags[slot].projectionTransformId = 0;
    viewportProjectionTags[slot].usesCenteredFixedAspect = FALSE;
    viewportProjectionTags[slot].cameraSkipState.valid = FALSE;
}

static void clearViewportProjectionSlot(ViewportNode *node) {
    u16 slot = node->callbackSlotIndex;

    if (viewportProjectionTags[slot].node == node) {
        viewportProjectionTags[slot].node = NULL;
        viewportProjectionTags[slot].projectionTransformId = 0;
        viewportProjectionTags[slot].usesCenteredFixedAspect = FALSE;
        viewportProjectionTags[slot].cameraSkipState.valid = FALSE;
    }
}

static s32 isRacePlayerParentProjectionTransformId(s32 projectionTransformId) {
    return (projectionTransformId >= PROJECTION_RACE_PLAYER_PARENT_TRANSFORM_ID_START) &&
           (projectionTransformId < PROJECTION_RACE_PLAYER_PARENT_TRANSFORM_ID_START + 4);
}

void setViewportProjectionTransformId(ViewportNode *node, s32 projectionTransformId) {
    u16 slot = node->callbackSlotIndex;

    if (viewportProjectionTags[slot].node == node) {
        viewportProjectionTags[slot].projectionTransformId = projectionTransformId;
    }
}

s32 getViewportProjectionTransformId(ViewportNode *node) {
    u16 slot = node->callbackSlotIndex;
    s32 projectionTransformId = 0;

    if (viewportProjectionTags[slot].node == node) {
        projectionTransformId = viewportProjectionTags[slot].projectionTransformId;
    }

    return projectionTransformId;
}

void setViewportProjectionCenteredFixedAspectBySlot(u16 slot) {
    if (viewportProjectionTags[slot].node != NULL) {
        viewportProjectionTags[slot].usesCenteredFixedAspect = TRUE;
    }
}

s32 viewportProjectionUsesCenteredFixedAspect(ViewportNode *node) {
    u16 slot = node->callbackSlotIndex;
    s32 usesCenteredFixedAspect = FALSE;

    if (viewportProjectionTags[slot].node == node) {
        usesCenteredFixedAspect = viewportProjectionTags[slot].usesCenteredFixedAspect;
    }

    return usesCenteredFixedAspect;
}

ViewportCameraSkipState *getViewportCameraSkipState(ViewportNode *node) {
    u16 slot = node->callbackSlotIndex;
    ViewportCameraSkipState *cameraSkipState = NULL;

    if (viewportProjectionTags[slot].node == node) {
        cameraSkipState = &viewportProjectionTags[slot].cameraSkipState;
    }

    return cameraSkipState;
}

RECOMP_PATCH void initViewportNode(ViewportNode *arg0, ViewportNode *arg1, s32 arg2, s32 arg3, s32 arg4) {
    ViewportNode *temp_v0;
    ViewportNode *var_a0;
    u8 arg4_byte = (u8)arg4;

    gViewportCallbackPools[arg2 & 0xFFFF] = arg0;

    if (arg1 == NULL) {
        arg0->parent = &gRootViewport;
        arg0->hierarchyPrev = &gRootViewport;
        temp_v0 = gRootViewport.nextSibling;
        arg0->nextSibling = temp_v0;
        if (temp_v0 != NULL) {
            temp_v0->hierarchyPrev = arg0;
        }
        gRootViewport.nextSibling = arg0;
    } else {
        arg0->parent = arg1;
        arg0->hierarchyPrev = arg1;
        temp_v0 = arg1->nextSibling;
        arg0->nextSibling = temp_v0;
        if (temp_v0 != NULL) {
            temp_v0->hierarchyPrev = arg0;
        }
        arg1->nextSibling = arg0;
    }

    var_a0 = &gRootViewport;
    if (gRootViewport.renderNext != NULL) {
        do {
            ViewportNode *temp_v1 = var_a0->renderNext;
            if ((u8)arg3 < (u8)temp_v1->renderOrder) {
                break;
            }
            var_a0 = temp_v1;
        } while (var_a0->renderNext != NULL);
    }

    arg0->renderPrev = var_a0;
    arg0->renderNext = var_a0->renderNext;
    var_a0->renderNext = arg0;
    temp_v0 = arg0->renderNext;
    if (temp_v0 != NULL) {
        temp_v0->renderPrev = arg0;
    }

    arg0->renderOrder = (s8)arg3;
    arg0->callbackSlotIndex = (u16)arg2;
    arg0->uses3DRendering = (s8)arg4_byte;
    arg0->displayFlags = 0;
    arg0->viewportId = 0;
    arg0->numLights = 0;
    arg0->viewport.vp.vscale[0] = 0x280;
    arg0->viewport.vp.vscale[1] = 0x1E0;
    arg0->viewport.vp.vscale[2] = 0x1FF;
    arg0->viewport.vp.vscale[3] = 0;
    arg0->viewport.vp.vtrans[0] = 0x280;
    arg0->viewport.vp.vtrans[1] = 0x1E0;
    arg0->viewport.vp.vtrans[2] = 0x1FF;
    arg0->viewport.vp.vtrans[3] = 0;
    memcpy(&arg0->viewTransform, &identityMatrix, sizeof(Transform3D));
    guPerspective(&arg0->projectionMatrix, &arg0->perspNorm, 30.0f, 1.3333334f, 20.0f, 2000.0f, 1.0f);
    arg0->fogA = 0xFF;
    arg0->fogStartPermille = 0x3DE;
    arg0->fogB = 0;
    arg0->fogG = 0;
    arg0->fogR = 0;
    arg0->fogEndPermille = 0x3E6;
    arg0->envR = 0;
    arg0->envG = 0;
    arg0->envB = 0;
    arg0->prevFadeValue = 0;
    arg0->fadeMode = 0;
    arg0->scaleY = 1.0f;
    initViewportCallbackPool(arg0);

    // @recomp tag viewport node with projection id
    registerViewportProjectionSlot(arg0);
    if (arg1 == NULL && (u32)(arg2 - 4) < 4 && arg3 == 5 && arg4_byte == 1) {
        setViewportProjectionTransformId(arg0, PROJECTION_RACE_PLAYER_PARENT_TRANSFORM_ID_START + arg2 - 4);
        if (arg2 == 4) {
            // @recomp latch the split direction when the first player viewport for a new race is initialized
            gRaceUsesVerticalTwoPlayerSplit = recomp_get_vertical_2p_split_screen_enabled();
        }
    } else if (arg1 != NULL && (u32)arg2 < 4 && arg3 == 0xA && arg4_byte == 1 &&
               isRacePlayerParentProjectionTransformId(getViewportProjectionTransformId(arg1))) {
        setViewportProjectionTransformId(arg0, PROJECTION_RACE_PLAYER_TRANSFORM_ID_START + arg2);
    }
}

RECOMP_PATCH void unlinkNode(ViewportNode *node) {
    ViewportNode *current;
    ViewportNode *next;

    current = &gRootViewport;
    gViewportCallbackPools[node->callbackSlotIndex] = NULL;

    next = gRootViewport.nextSibling;
    while (next != 0) {
        if (current->parent == node) {
            current->parent = node->parent;
        }

        current = current->nextSibling;
        next = current->nextSibling;
    }

    if (node->nextSibling != 0) {
        node->nextSibling->hierarchyPrev = node->hierarchyPrev;
    }

    node->hierarchyPrev->nextSibling = node->nextSibling;
    if (node->renderNext != 0) {
        node->renderNext->renderPrev = node->renderPrev;
    }

    node->renderPrev->renderNext = node->renderNext;

    // @recomp clear projection metadata
    clearViewportProjectionSlot(node);
}
