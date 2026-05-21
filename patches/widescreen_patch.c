/*
 * Widescreen viewport fixes.
 *
 * RT64's automatic widescreen path expects full-screen viewports and scissors
 * to reach the 320x240 framebuffer edge. Normalize the game's full-screen
 * camera and UI bounds to that convention so title, race, menu, and cutscene
 * views consistently opt into RT64's widescreen handling. Smaller framed views
 * keep their authored bounds.
 */
#include "PR/ultratypes.h"

#define RECOMP_PATCH __attribute__((section(".recomp_patch")))

typedef struct ViewportNode {
    u8 _pad00[0x10];
    /* 0x10 */ struct ViewportNode *list3_next;
    u8 _pad14[0x84];
    /* 0x98 */ void *_displayListPtr;
    /* 0x9C */ void *_frameCallbackMsg;
    /* 0xA0 */ s16 originX;
    /* 0xA2 */ s16 originY;
    /* 0xA4 */ s16 viewportLeft;
    /* 0xA6 */ s16 viewportTop;
    /* 0xA8 */ s16 viewportRight;
    /* 0xAA */ s16 viewportBottom;
    /* 0xAC */ s16 offsetX;
    /* 0xAE */ s16 offsetY;
    /* 0xB0 */ s16 clipLeft;
    /* 0xB2 */ s16 clipTop;
    /* 0xB4 */ s16 clipRight;
    /* 0xB6 */ s16 clipBottom;
    u8 _padB8[0x10];
    /* 0xC8 */ s16 _viewportWidth;
    /* 0xCA */ s16 _viewportHeight;
    /* 0xCC */ s16 _unkCC;
    /* 0xCE */ s16 _unkCE;
    /* 0xD0 */ s16 unkD0;
    /* 0xD2 */ s16 unkD2;
} ViewportNode;

/* The original layout uses a doubly-linked sibling list at 0x08 for the
 * traversal we need; we don't dereference it through this struct. */
extern ViewportNode gRootViewport;

/* Match the layout used by graphics.c's updateViewportBounds traversal:
 *   node = &gRootViewport;
 *   childNode = node->unk0.next;        // PARENT pointer for non-roots
 *   ...
 *   node = node->unk8.list2_next;       // NEXT sibling/descendant
 * Both are at offsets 0x00 and 0x08. We walk via raw offsets to avoid
 * mirroring the union shape. */
static ViewportNode *vp_unk0_next(ViewportNode *n) {
    return *(ViewportNode **)((u8 *)n + 0x00);
}
static ViewportNode *vp_unk8_next(ViewportNode *n) {
    return *(ViewportNode **)((u8 *)n + 0x08);
}

RECOMP_PATCH void setModelCameraTransform(void *arg0, s16 originX, s16 originY,
                                          s16 left, s16 top, s16 right, s16 bottom) {
    ViewportNode *node = (ViewportNode *)arg0;

    if ((left == -0xA0 && top == -0x78 && right == 0x9F && bottom == 0x77) ||
        (left == -0x98 && top == -0x70 && right == 0x97 && bottom == 0x6F)) {
        left   = -0xA0;
        top    = -0x78;
        right  =  0xA0;
        bottom =  0x78;
    }

    node->originX        = originX;
    node->originY        = originY;
    node->viewportLeft   = left;
    node->viewportTop    = top;
    node->viewportRight  = right;
    node->viewportBottom = bottom;
}

/* Reimplementation of updateViewportBounds (graphics.c:1012, vram 0x8006F82C)
 * with the inheritedMaxX/Y defaults bumped from (0x13F, 0xEF) to (0x140, 0xF0)
 * so that full-screen viewports' clipRight/clipBottom can reach the actual
 * 320×240 framebuffer edge. See block comment at top of file for rationale. */
RECOMP_PATCH void updateViewportBounds(void) {
    ViewportNode *node;
    ViewportNode *childNode;
    u16 inheritedCenterX = 0xA0;
    u16 inheritedCenterY = 0x78;
    u16 inheritedMinX    = 0;
    u16 inheritedMinY    = 0;
    u16 inheritedMaxX    = 0x140;  /* was 0x13F */
    u16 inheritedMaxY    = 0xF0;   /* was 0xEF */
    s16 computedLeft;
    s16 computedTop;

    node = &gRootViewport;
    do {
        childNode = vp_unk0_next(node);
        if (childNode != NULL) {
            inheritedCenterX = (u16)childNode->offsetX;
            inheritedCenterY = (u16)childNode->offsetY;
            inheritedMinX    = (u16)childNode->clipLeft;
            inheritedMinY    = (u16)childNode->clipTop;
            inheritedMaxX    = (u16)childNode->clipRight;
            inheritedMaxY    = (u16)childNode->clipBottom;
        }
        node->offsetX = (s16)(inheritedCenterX + (u16)node->originX);
        node->offsetY = (s16)(inheritedCenterY + (u16)node->originY);
        node->unkD0   = (s16)(node->offsetX * 4);
        node->unkD2   = (s16)(node->offsetY * 4);
        node->clipLeft   = (s16)((u16)node->offsetX + (u16)node->viewportLeft);
        node->clipTop    = (s16)((u16)node->offsetY + (u16)node->viewportTop);
        node->clipRight  = (s16)((u16)node->offsetX + (u16)node->viewportRight);
        node->clipBottom = (s16)((u16)node->offsetY + (u16)node->viewportBottom);
        if (node->clipLeft   < (s16)inheritedMinX) node->clipLeft   = (s16)inheritedMinX;
        if (node->clipTop    < (s16)inheritedMinY) node->clipTop    = (s16)inheritedMinY;
        if ((s16)inheritedMaxX < node->clipRight)  node->clipRight  = (s16)inheritedMaxX;
        if ((s16)inheritedMaxY < node->clipBottom) node->clipBottom = (s16)inheritedMaxY;
        computedLeft = node->clipLeft;
        if (node->clipRight  < computedLeft) node->clipRight  = computedLeft;
        computedTop  = node->clipTop;
        if (node->clipBottom < computedTop)  node->clipBottom = computedTop;
        node = vp_unk8_next(node);
    } while (node != NULL);
}
