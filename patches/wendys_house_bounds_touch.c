#include "patches.h"

#include "graphics/displaylist.h"
#include "race/course.h"

/* 
 * Wendy's House can produce frames where the visible 3D geometry leads
 * RT64 to incorrectly assume draw bounds of 320x224 rather than 320x240.
 * This manifests as a black bar sometimes appearing at the bottom of the
 * screen.
 *
 * I do not know why this is happening. But we can fix it by drawing something
 * on the bottom of the screen. There are also some hacks in this file to ensure
 * the draw call only happens on Wendy's House.
 *
 * It's not pretty but it works.
 */

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

typedef struct {
    u8 _pad[0xB4];
    s16 skyType;
} ScheduledSkyTask;

extern Gfx* gDisplayListAllocPtr;
extern s16 gGraphicsMode;
extern void initSkyRenderTask(void*);
extern void renderCameraRelativeDisplayList(DisplayListObject* displayListObj);

static s32 sWendysHouseBoundsTouchEnabled = FALSE;

static void touch_wendys_house_bottom_bounds(void* unused) {
    Gfx* gfx = gDisplayListAllocPtr;

    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(gfx++, 0x00010001);
    gDPFillRectangle(gfx++, 0, SCREEN_HEIGHT - 1, 1, SCREEN_HEIGHT - 1);
    gDPPipeSync(gfx++);

    gDisplayListAllocPtr = gfx;
    gGraphicsMode = -1;
}

RECOMP_PATCH void scheduleSkyRenderTask(s32 skyType) {
    ScheduledSkyTask* task;

    sWendysHouseBoundsTouchEnabled = (skyType == WENDYS_HOUSE);

    task = scheduleTask(initSkyRenderTask, 0, 0, 0xD2);
    if (task != NULL) {
        task->skyType = skyType;
    }
}

RECOMP_PATCH void enqueueCameraRelativeDisplayList(s32 viewportSlot, DisplayListObject* arg1) {
    arg1->transformMatrix = NULL;
    enqueueCallbackBySlotIndex(viewportSlot, 0, &renderCameraRelativeDisplayList, arg1);

    if (sWendysHouseBoundsTouchEnabled && (viewportSlot >= 4) && (viewportSlot < 8)) {
        enqueueCallbackBySlotIndex(viewportSlot, 0, &touch_wendys_house_bottom_bounds, NULL);
    }
}
