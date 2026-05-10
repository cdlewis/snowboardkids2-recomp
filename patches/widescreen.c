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

extern float recomp_get_target_aspect_ratio(float original);


RECOMP_PATCH void setModelCameraTransform(void* arg0, s16 originX, s16 originY, s16 left, s16 top, s16 right,
                                          s16 bottom) {
    ViewportNode* node = (ViewportNode*) arg0;

    if ((left == -0xA0 && top == -0x78 && right == 0x9F && bottom == 0x77) ||
        (left == -0x98 && top == -0x70 && right == 0x97 && bottom == 0x6F)) {
        left = -0xA0;
        top = -0x78;
        right = 0xA0;
        bottom = 0x78;
    }

    node->originX = originX;
    node->originY = originY;
    node->viewportLeft = left;
    node->viewportTop = top;
    node->viewportRight = right;
    node->viewportBottom = bottom;
}

RECOMP_PATCH void updateViewportBounds(void) {
    ViewportNode* node;
    ViewportNode* childNode;
    u16 inheritedCenterX = 0xA0;
    u16 inheritedCenterY = 0x78;
    u16 inheritedMinX = 0;
    u16 inheritedMinY = 0;
    u16 inheritedMaxX = 0x140;
    u16 inheritedMaxY = 0xF0;
    s16 computedLeft;
    s16 computedTop;

    node = &gRootViewport;
    do {
        childNode = node->unk0.next;
        if (childNode != NULL) {
            inheritedCenterX = (u16) childNode->offsetX;
            inheritedCenterY = (u16) childNode->offsetY;
            inheritedMinX = (u16) childNode->clipLeft;
            inheritedMinY = (u16) childNode->clipTop;
            inheritedMaxX = (u16) childNode->clipRight;
            inheritedMaxY = (u16) childNode->clipBottom;
        }
        node->offsetX = (s16) (inheritedCenterX + (u16) node->originX);
        node->offsetY = (s16) (inheritedCenterY + (u16) node->originY);
        node->unkD0 = (s16) (node->offsetX * 4);
        node->unkD2 = (s16) (node->offsetY * 4);
        node->clipLeft = (s16) ((u16) node->offsetX + (u16) node->viewportLeft);
        node->clipTop = (s16) ((u16) node->offsetY + (u16) node->viewportTop);
        node->clipRight = (s16) ((u16) node->offsetX + (u16) node->viewportRight);
        node->clipBottom = (s16) ((u16) node->offsetY + (u16) node->viewportBottom);
        if (node->clipLeft < (s16) inheritedMinX)
            node->clipLeft = (s16) inheritedMinX;
        if (node->clipTop < (s16) inheritedMinY)
            node->clipTop = (s16) inheritedMinY;
        if ((s16) inheritedMaxX < node->clipRight)
            node->clipRight = (s16) inheritedMaxX;
        if ((s16) inheritedMaxY < node->clipBottom)
            node->clipBottom = (s16) inheritedMaxY;
        computedLeft = node->clipLeft;
        if (node->clipRight < computedLeft)
            node->clipRight = computedLeft;
        computedTop = node->clipTop;
        if (node->clipBottom < computedTop)
            node->clipBottom = computedTop;
        node = node->unk8.list2_next;
    } while (node != NULL);
}
