/*
 * Widescreen viewport fixes.
 *
 * RT64's automatic widescreen path expects full-screen viewports and scissors
 * to reach the 320x240 framebuffer edge. Normalize the game's full-screen
 * camera and UI bounds to that convention so title, race, menu, and cutscene
 * views consistently opt into RT64's widescreen handling. Smaller framed views
 * keep their authored bounds.
 */

#include "patches.h"
#include "transform_ids.h"

extern float recomp_get_target_aspect_ratio(float original);
extern s32 recomp_get_vertical_2p_split_screen_enabled(void);

#define RACE_SPLITSCREEN_INSET_QUADRANT_CENTER_X 0x49
#define RACE_SPLITSCREEN_INSET_HALF_CENTER_Y 0x35
#define RACE_SPLITSCREEN_INSET_QUADRANT_HALF_WIDTH 0x48
#define RACE_SPLITSCREEN_INSET_HALF_HEIGHT 0x34

#define RACE_SPLITSCREEN_EDGE_QUADRANT_CENTER_X 0x50
#define RACE_SPLITSCREEN_EDGE_HALF_CENTER_Y 0x3C
#define RACE_SPLITSCREEN_EDGE_QUADRANT_HALF_WIDTH 0x50
#define RACE_SPLITSCREEN_EDGE_HALF_HEIGHT 0x3C

static s32 getRacePlayerViewportIndex(ViewportNode* node) {
    s32 transformId;

    transformId = getViewportProjectionTransformId(node);
    if ((transformId >= PROJECTION_RACE_PLAYER_TRANSFORM_ID_START) &&
        (transformId < PROJECTION_RACE_PLAYER_TRANSFORM_ID_START + 4)) {
        return transformId - PROJECTION_RACE_PLAYER_TRANSFORM_ID_START;
    }
    if ((transformId >= PROJECTION_RACE_PLAYER_PARENT_TRANSFORM_ID_START) &&
        (transformId < PROJECTION_RACE_PLAYER_PARENT_TRANSFORM_ID_START + 4)) {
        return transformId - PROJECTION_RACE_PLAYER_PARENT_TRANSFORM_ID_START;
    }

    if (node != &gRootViewport && node->unk0.next != NULL) {
        return getRacePlayerViewportIndex(node->unk0.next);
    }

    return -1;
}

static s32 usesVerticalTwoPlayerSplit(ViewportNode* node) {
    s32 playerIndex = getRacePlayerViewportIndex(node);

    return recomp_get_vertical_2p_split_screen_enabled() && playerIndex >= 0 && playerIndex < 2;
}

static void convertTwoPlayerViewportToVertical(
    ViewportNode* node,
    s16* originX,
    s16* originY,
    s16* left,
    s16* top,
    s16* right,
    s16* bottom
) {
    s32 playerIndex;

    if (!usesVerticalTwoPlayerSplit(node) || *left != -0xA0 || *right != 0xA0 || *top != -0x34 ||
        *bottom != 0x34) {
        return;
    }

    playerIndex = getRacePlayerViewportIndex(node);
    if (*originY == -0x35 || *originY == 0x35) {
        *originX = (playerIndex == 0) ? -RACE_SPLITSCREEN_INSET_QUADRANT_CENTER_X
                                    : RACE_SPLITSCREEN_INSET_QUADRANT_CENTER_X;
    } else if (*originX != 0 || *originY != 0) {
        return;
    }

    *originY = 0;
    *left = -RACE_SPLITSCREEN_INSET_QUADRANT_HALF_WIDTH;
    *top = -0x78;
    *right = RACE_SPLITSCREEN_INSET_QUADRANT_HALF_WIDTH;
    *bottom = 0x78;
}

static void normalizeRaceSplitScreenViewportBounds(
    ViewportNode* node,
    s16* originX,
    s16* originY,
    s16* left,
    s16* top,
    s16* right,
    s16* bottom
) {
    if (getRacePlayerViewportIndex(node) < 0) {
        return;
    }

    /*
     * The original race split-screen bounds are slightly inset from the
     * framebuffer edge. Snap them to exact halves/quadrants so RT64 recognizes
     * them as edge-aligned viewports. 2P only matches the Y half values, while
     * 3P/4P also match the quadrant X and width values.
     */
    if (*originX == -RACE_SPLITSCREEN_INSET_QUADRANT_CENTER_X) {
        *originX = -RACE_SPLITSCREEN_EDGE_QUADRANT_CENTER_X;
    } else if (*originX == RACE_SPLITSCREEN_INSET_QUADRANT_CENTER_X) {
        *originX = RACE_SPLITSCREEN_EDGE_QUADRANT_CENTER_X;
    }

    if (*originY == -RACE_SPLITSCREEN_INSET_HALF_CENTER_Y) {
        *originY = -RACE_SPLITSCREEN_EDGE_HALF_CENTER_Y;
    } else if (*originY == RACE_SPLITSCREEN_INSET_HALF_CENTER_Y) {
        *originY = RACE_SPLITSCREEN_EDGE_HALF_CENTER_Y;
    }

    if (*left == -RACE_SPLITSCREEN_INSET_QUADRANT_HALF_WIDTH &&
        *right == RACE_SPLITSCREEN_INSET_QUADRANT_HALF_WIDTH) {
        *left = -RACE_SPLITSCREEN_EDGE_QUADRANT_HALF_WIDTH;
        *right = RACE_SPLITSCREEN_EDGE_QUADRANT_HALF_WIDTH;
    }

    if (*top == -RACE_SPLITSCREEN_INSET_HALF_HEIGHT &&
        *bottom == RACE_SPLITSCREEN_INSET_HALF_HEIGHT) {
        *top = -RACE_SPLITSCREEN_EDGE_HALF_HEIGHT;
        *bottom = RACE_SPLITSCREEN_EDGE_HALF_HEIGHT;
    }
}

RECOMP_PATCH void setModelCameraTransform(ViewportNode* node, s16 originX, s16 originY, s16 left, s16 top, s16 right,
                                          s16 bottom) {
    if ((left == -0xA0 && top == -0x78 && right == 0x9F && bottom == 0x77) ||
        (left == -0x98 && top == -0x70 && right == 0x97 && bottom == 0x6F)) {
        left = -0xA0;
        top = -0x78;
        right = 0xA0;
        bottom = 0x78;
    }

    // @recomp arrange 2P race viewports as left/right columns when configured
    convertTwoPlayerViewportToVertical(node, &originX, &originY, &left, &top, &right, &bottom);

    // @recomp adjust viewport bounds for RT64 widescreen handling
    normalizeRaceSplitScreenViewportBounds(node, &originX, &originY, &left, &top, &right, &bottom);

    node->originX = originX;
    node->originY = originY;
    node->viewportLeft = left;
    node->viewportTop = top;
    node->viewportRight = right;
    node->viewportBottom = bottom;
}

RECOMP_PATCH void setViewportScale(ViewportNode *arg0, f32 scaleX, f32 scaleY) {
    // @recomp convert the original full-width, half-height 2P scale to half-width, full-height for a vertical split
    if (usesVerticalTwoPlayerSplit(arg0) && scaleX == 1.0f && scaleY == 0.5f) {
        scaleX = 0.5f;
        scaleY = 1.0f;
    }

    arg0->scaleY = scaleY;
    arg0->viewportWidth = (s16)(scaleX * 640.0f);
    arg0->viewportHeight = (s16)(scaleY * 480.0f);
}

RECOMP_PATCH void setViewportPerspective(ViewportNode *node, f32 fov, f32 aspect, f32 near, f32 far) {
    // @recomp replace the original 8:3 2P projection with a 2:3 projection suited to the vertical split while
    // preserving roughly the standard 4:3 camera's horizontal field of view
    if (usesVerticalTwoPlayerSplit(node) && aspect == (4.0f / 3.0f) * 2.0f) {
        fov = 110.0f;
        aspect = (4.0f / 3.0f) * 0.5f;
    }

    guPerspective(&node->projectionMatrix, &node->perspNorm, fov, aspect, near, far, 1.0f);
}

RECOMP_PATCH void updateViewportBounds(void) {
    ViewportNode *childNode;
    ViewportNode *node;
    u16 inheritedCenterX;
    u16 inheritedCenterY;
    u16 inheritedMinX;
    u16 inheritedMinY;
    u16 inheritedMaxX;
    u16 inheritedMaxY;
    s16 computedLeft;
    s16 computedTop;

    inheritedCenterX = 0xA0; // screen width / 2 (320 / 2)
    inheritedCenterY = 0x78; // screen height / 2 (240 / 2)
    inheritedMinX = 0;
    inheritedMinY = 0;
    // @recomp use 320 instead of 319 so the widescreen viewports remain edge-aligned
    inheritedMaxX = 0x140;
    node = &gRootViewport;
    // @recomp use 240 instead of 239 so the widescreen viewports remain edge-aligned
    inheritedMaxY = 0xF0;

    if (node != NULL) {
        do {
            childNode = node->unk0.next;
            if (childNode != NULL) {
                inheritedCenterX = childNode->offsetX;
                inheritedCenterY = childNode->offsetY;
                inheritedMinX = childNode->clipLeft;
                inheritedMinY = childNode->clipTop;
                inheritedMaxX = childNode->clipRight;
                inheritedMaxY = childNode->clipBottom;
            }
            node->offsetX = inheritedCenterX + (u16)node->originX;
            node->offsetY = inheritedCenterY + (u16)node->originY;
            node->unkD0 = node->offsetX * 4;
            node->unkD2 = node->offsetY * 4;
            node->clipLeft = (u16)node->offsetX + (u16)node->viewportLeft;
            node->clipTop = (u16)node->offsetY + (u16)node->viewportTop;
            node->clipRight = (u16)node->offsetX + (u16)node->viewportRight;
            node->clipBottom = (u16)node->offsetY + (u16)node->viewportBottom;
            if (node->clipLeft < (s16)inheritedMinX) {
                node->clipLeft = inheritedMinX;
            }
            if (node->clipTop < (s16)inheritedMinY) {
                node->clipTop = inheritedMinY;
            }
            if ((s16)inheritedMaxX < node->clipRight) {
                node->clipRight = inheritedMaxX;
            }
            if ((s16)inheritedMaxY < node->clipBottom) {
                node->clipBottom = inheritedMaxY;
            }
            computedLeft = node->clipLeft;
            if (node->clipRight < computedLeft) {
                node->clipRight = computedLeft;
            }
            computedTop = node->clipTop;
            if (node->clipBottom < computedTop) {
                node->clipBottom = computedTop;
            }
            node = node->unk8.list2_next;
        } while (node != NULL);
    }
}
