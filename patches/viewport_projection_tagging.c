/*
 * View projection tagging
 *
 * This patch adds the ability to associate additional metadata with
 * a viewport node, which can be used to determine which projection
 * transform to use when rendering that viewport.
 */

#include "patches.h"
#include "transform_ids.h"

typedef struct {
    ViewportNode *node;
    s32 projectionTransformId;
    ViewportCameraSkipState cameraSkipState;
} ViewportProjectionTag;

// Viewports are registered in gViewportCallbackPools, which can have at most 0x10 entries.
// But we're feeling generous so double to 0x20 just to be safe.
static ViewportProjectionTag viewportProjectionTags[0x20];

extern CallbackPoolSlot *gViewportCallbackPools[];

static void registerViewportProjectionSlot(ViewportNode *node) {
    u16 slot = node->callbackSlotIndex;

    viewportProjectionTags[slot].node = node;
    viewportProjectionTags[slot].projectionTransformId = 0;
    viewportProjectionTags[slot].cameraSkipState.valid = FALSE;
}

static void clearViewportProjectionSlot(ViewportNode *node) {
    u16 slot = node->callbackSlotIndex;

    if (viewportProjectionTags[slot].node == node) {
        viewportProjectionTags[slot].node = NULL;
        viewportProjectionTags[slot].projectionTransformId = 0;
        viewportProjectionTags[slot].cameraSkipState.valid = FALSE;
    }
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

    gViewportCallbackPools[arg2 & 0xFFFF] = (CallbackPoolSlot *)arg0;

    if (arg1 == NULL) {
        arg0->unk0.next = &gRootViewport;
        arg0->prev = &gRootViewport;
        temp_v0 = gRootViewport.unk8.list2_next;
        arg0->unk8.list2_next = temp_v0;
        if (temp_v0 != NULL) {
            temp_v0->prev = arg0;
        }
        gRootViewport.unk8.list2_next = arg0;
    } else {
        arg0->unk0.next = arg1;
        arg0->prev = arg1;
        temp_v0 = arg1->unk8.list2_next;
        arg0->unk8.list2_next = temp_v0;
        if (temp_v0 != NULL) {
            temp_v0->prev = arg0;
        }
        arg1->unk8.list2_next = arg0;
    }

    var_a0 = &gRootViewport;
    if (gRootViewport.list3_next != NULL) {
        do {
            ViewportNode *temp_v1 = var_a0->list3_next;
            if ((u8)arg3 < (u8)temp_v1->renderOrder) {
                break;
            }
            var_a0 = temp_v1;
        } while (var_a0->list3_next != NULL);
    }

    arg0->list2_prev = var_a0;
    arg0->list3_next = var_a0->list3_next;
    var_a0->list3_next = arg0;
    temp_v0 = arg0->list3_next;
    if (temp_v0 != NULL) {
        temp_v0->list2_prev = arg0;
    }

    arg0->renderOrder = (s8)arg3;
    arg0->callbackSlotIndex = (u16)arg2;
    arg0->viewportFlags = (s8)arg4_byte;
    arg0->displayFlags = 0;
    arg0->viewportId = 0;
    arg0->numLights = 0;
    arg0->viewportWidth = 0x280;
    arg0->viewportHeight = 0x1E0;
    arg0->unkCC = 0x1FF;
    arg0->unkCE = 0;
    arg0->unkD0 = 0x280;
    arg0->unkD2 = 0x1E0;
    arg0->unkD4 = 0x1FF;
    arg0->unkD6 = 0;
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
    } else if (arg1 != NULL && (u32)arg2 < 4 && arg3 == 0xA && arg4_byte == 1) {
        setViewportProjectionTransformId(arg0, PROJECTION_RACE_PLAYER_TRANSFORM_ID_START + arg2);
    }
}

RECOMP_PATCH void unlinkNode(ViewportNode *node) {
    ViewportNode *current;
    ViewportNode *next;

    current = &gRootViewport;
    gViewportCallbackPools[node->callbackSlotIndex] = NULL;

    next = gRootViewport.unk8.list2_next;
    while (next != 0) {
        if (current->unk0.next == node) {
            current->unk0.next = node->unk0.next;
        }

        current = current->unk8.list2_next;
        next = current->unk8.list2_next;
    }

    if (node->unk8.list2_next != 0) {
        node->unk8.list2_next->prev = node->prev;
    }

    node->prev->unk8.list2_next = node->unk8.list2_next;
    if (node->list3_next != 0) {
        node->list3_next->list2_prev = node->list2_prev;
    }

    node->list2_prev->list3_next = node->list3_next;

    // @recomp clear projection metadata
    clearViewportProjectionSlot(node);
}
