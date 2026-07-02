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

#define RACE_SPLITSCREEN_INSET_QUADRANT_CENTER_X 0x49
#define RACE_SPLITSCREEN_INSET_HALF_CENTER_Y 0x35
#define RACE_SPLITSCREEN_INSET_QUADRANT_HALF_WIDTH 0x48
#define RACE_SPLITSCREEN_INSET_HALF_HEIGHT 0x34

#define RACE_SPLITSCREEN_EDGE_QUADRANT_CENTER_X 0x50
#define RACE_SPLITSCREEN_EDGE_HALF_CENTER_Y 0x3C
#define RACE_SPLITSCREEN_EDGE_QUADRANT_HALF_WIDTH 0x50
#define RACE_SPLITSCREEN_EDGE_HALF_HEIGHT 0x3C

static s32 isRacePlayerViewport(ViewportNode* node) {
    s32 transformId;

    transformId = getViewportProjectionTransformId(node);
    return ((transformId >= PROJECTION_RACE_PLAYER_TRANSFORM_ID_START) &&
            (transformId < PROJECTION_RACE_PLAYER_TRANSFORM_ID_START + 4)) ||
           ((transformId >= PROJECTION_RACE_PLAYER_PARENT_TRANSFORM_ID_START) &&
            (transformId < PROJECTION_RACE_PLAYER_PARENT_TRANSFORM_ID_START + 4));
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
    if (!isRacePlayerViewport(node)) {
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

    // @recomp adjust viewport bounds for RT64 widescreen handling
    normalizeRaceSplitScreenViewportBounds(node, &originX, &originY, &left, &top, &right, &bottom);

    node->originX = originX;
    node->originY = originY;
    node->viewportLeft = left;
    node->viewportTop = top;
    node->viewportRight = right;
    node->viewportBottom = bottom;
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
