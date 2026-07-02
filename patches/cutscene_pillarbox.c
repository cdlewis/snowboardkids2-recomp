#include "patches.h"

// Pillarbox projection and RT64 matrix-group setup based on BanjoRecomp's pillarbox patch:
// https://github.com/BanjoRecomp/BanjoRecomp/blob/main/patches/pillarbox_patches.c

#include "animation/animation_loop.h"
#include "animation/slot_animation.h"
#include "cutscene/1DD170.h"
#include "cutscene/cutscene_manager.h"
#include "transform_ids.h"

#define ORIGINAL_ASPECT (4.0f / 3.0f)
#define CUTSCENE_SCREEN_WIDTH 320.0f
#define CUTSCENE_SCREEN_HEIGHT 240.0f
#define PILLARBOX_RECTANGLE_TRANSFORM_ID_START 0x31000000

extern float recomp_get_target_aspect_ratio(float original);
extern Gfx* gDisplayListAllocPtr;
extern char gDebugFrameFormatString[];

extern void guMtxF2L(float mf[4][4], Mtx* m);

static float pillarboxIdentityMatrix[4][4] = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
};

static float cutscenePillarboxWidth(void) {
    float curAspect = recomp_get_target_aspect_ratio(ORIGINAL_ASPECT);
    float targetWidth;
    float wideWidth;
    float pillarWidth;

    if (curAspect <= ORIGINAL_ASPECT) {
        return 0;
    }

    targetWidth = CUTSCENE_SCREEN_HEIGHT * ORIGINAL_ASPECT;
    wideWidth = CUTSCENE_SCREEN_HEIGHT * curAspect;
    pillarWidth = wideWidth - targetWidth;

    if (pillarWidth <= 0.0f) {
        return 0;
    }

    return pillarWidth;
}

static void setIdentityMatrix(float mf[4][4]) {
    s32 row;
    s32 col;

    for (row = 0; row < 4; row++) {
        for (col = 0; col < 4; col++) {
            mf[row][col] = (row == col) ? 1.0f : 0.0f;
        }
    }
}

static void setOrthoMatrix(float mf[4][4], float left, float right, float bottom, float top, float near, float far) {
    setIdentityMatrix(mf);

    mf[0][0] = 2.0f / (right - left);
    mf[1][1] = 2.0f / (top - bottom);
    mf[2][2] = -2.0f / (far - near);
    mf[3][0] = -(right + left) / (right - left);
    mf[3][1] = -(top + bottom) / (top - bottom);
    mf[3][2] = -(far + near) / (far - near);
}

static void setTranslateMatrix(Mtx* mtx, float x, float y, float z) {
    float mf[4][4];

    setIdentityMatrix(mf);
    mf[3][0] = x;
    mf[3][1] = y;
    mf[3][2] = z;
    guMtxF2L(mf, mtx);
}

static void drawCutscenePillarbox(void* unused) {
    float pillarWidth = cutscenePillarboxWidth();
    float curAspect = recomp_get_target_aspect_ratio(ORIGINAL_ASPECT);
    float wideWidth = CUTSCENE_SCREEN_HEIGHT * curAspect;
    float(*projection)[4];
    Mtx* modelview;
    Vtx* verts;
    Gfx* gfx = gDisplayListAllocPtr;
    u32 id;
    s32 ix;
    s32 iy;

    if (pillarWidth <= 0.0f) {
        return;
    }

    projection = arenaAlloc16(sizeof(float) * 4 * 4);
    modelview = arenaAlloc16(sizeof(Mtx) * 2);
    verts = arenaAlloc16(sizeof(Vtx) * 4);

    if (projection == NULL || modelview == NULL || verts == NULL) {
        return;
    }

    setOrthoMatrix(projection, -CUTSCENE_SCREEN_WIDTH * 2.0f, CUTSCENE_SCREEN_WIDTH * 2.0f,
                   -CUTSCENE_SCREEN_HEIGHT * 2.0f, CUTSCENE_SCREEN_HEIGHT * 2.0f, 1.0f, 20.0f);

    for (ix = -1; ix < 2; ix += 2) {
        for (iy = 0; iy < 2; iy++) {
            verts->v.ob[0] = (s16)(ix * pillarWidth * 2.0f);
            verts->v.ob[1] = (s16)(iy * CUTSCENE_SCREEN_HEIGHT * 4.0f - CUTSCENE_SCREEN_HEIGHT * 2.0f);
            verts->v.ob[2] = -0x14;
            verts++;
        }
    }
    verts -= 4;

    gEXPushOtherMode(gfx++);
    gEXPushCombineMode(gfx++);
    gEXPushGeometryMode(gfx++);

    gEXPushScissor(gfx++);
    gEXSetScissorAlign(gfx++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, -((s32) CUTSCENE_SCREEN_WIDTH), 0, 0, 0,
                       (s32) CUTSCENE_SCREEN_WIDTH, (s32) CUTSCENE_SCREEN_HEIGHT);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, (s32) CUTSCENE_SCREEN_WIDTH, (s32) CUTSCENE_SCREEN_HEIGHT);

    gSPClearGeometryMode(gfx++, G_CULL_BOTH);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetCombineLERP(gfx++, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    gEXMatrixFloat(gfx++, projection, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gEXMatrixGroupSimpleNormal(gfx++, PROJECTION_PILLARBOX_TRANSFORM_ID, G_EX_NOPUSH, G_MTX_PROJECTION,
                               G_EX_EDIT_NONE);
    gEXSetViewMatrixFloat(gfx++, pillarboxIdentityMatrix);

    id = PILLARBOX_RECTANGLE_TRANSFORM_ID_START;
    for (ix = -1; ix < 2; ix += 2) {
        setTranslateMatrix(modelview, ix * wideWidth * 2.0f, 0.0f, 0.0f);
        gSPMatrix(gfx++, modelview, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        modelview++;

        gEXMatrixGroupSimpleVerts(gfx++, id++, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
        gSPVertex(gfx++, verts, 4, 0);
        gSP1Quadrangle(gfx++, 0, 1, 3, 2, 0);
        gEXPopMatrixGroup(gfx++, G_MTX_MODELVIEW);
    }

    gEXPopScissor(gfx++);
    gEXPopGeometryMode(gfx++);
    gEXPopCombineMode(gfx++);
    gEXPopOtherMode(gfx++);

    gDisplayListAllocPtr = gfx;
}

RECOMP_PATCH s32 processCutsceneFrame(CutsceneManager* cutsceneManager) {
    s32 slotBitmask;
    SceneModel* currentModel;
    s32 i;
    CutsceneSlotData* slotData;
    s16 cameraOffsetX;
    s32 cameraScaleZ;
    StateEntry* eventEntry;
    s32 shouldInitializeSlot;
    u16 commandType;
    s32 curtainPos;
    u16 eventIndex;
    u8 eventType;

    if (cutsceneManager->showDebugInfo) {
        _Sprintf((char*) cutsceneManager->debugText, gDebugFrameFormatString, cutsceneManager->currentFrame);
        enqueueCallbackBySlotIndex(cutsceneManager->uiResource->callbackSlotIndex, 6, &renderTextPalette,
                                   &cutsceneManager->textX);
    }

    while (cutsceneManager->currentFrame <= cutsceneManager->maxFrame && !cutsceneManager->skipAnimation) {
        for (i = 0, slotBitmask = 0; i < getCutsceneSlotCount(); i++) {
            currentModel = cutsceneManager->slots[i].model;
            slotData = &cutsceneManager->slots[i].slotData;

            eventIndex = findEventAtFrame(i, cutsceneManager->currentFrame);
            if ((eventIndex & 0xFFFF) != 0xFFFF) {
                eventEntry = getStateEntry(eventIndex);
                shouldInitializeSlot = 1;

                if (cutsceneManager->currentFrame < cutsceneManager->maxFrame) {
                    eventType = eventEntry->commandCategory - 4;

                    if (eventType < 2) {
                        shouldInitializeSlot = 0;
                    } else {
                        commandType = *(u16*) &eventEntry->commandCategory;
                        if (commandType == 0x801) {
                            commandType = 0;
                            shouldInitializeSlot = commandType;
                        }
                    }
                }

                if (shouldInitializeSlot) {
                    initializeSlotState(eventEntry, cutsceneManager, slotBitmask >> 24);
                }
            }

            updateSlotData(cutsceneManager, slotBitmask >> 24);
            syncModelFromSlot(slotData, currentModel);

            slotBitmask += 0x1000000;
        }

        cutsceneManager->currentFrame++;
        advanceSceneManager(cutsceneManager->sceneContext);
    }

    finalizeAnimationLoop(cutsceneManager->sceneContext);

    if (!cutsceneManager->skipAnimation) {
        cutsceneManager->maxFrame++;
    }

    for (i = 0; i < getCutsceneSlotCount(); i++) {
        currentModel = cutsceneManager->slots[i].model;
        slotData = &cutsceneManager->slots[i].slotData;

        if (currentModel) {
            setupSlotTransform(slotData);
            applyTransformToModel(currentModel, &slotData->transform);
            if (cutsceneManager->enableTransparency) {
                setModelVisibility(currentModel, 1);
            } else {
                setModelVisibility(currentModel, 0);
            }

            updateModelGeometry(currentModel);

            if (slotData->angle != 0) {
                setModelRotation(currentModel, slotData->angle);
            } else {
                clearModelRotation(currentModel);
            }
        }
    }

    curtainPos = cutsceneManager->curtainPosition;
    cameraOffsetX = -((curtainPos * 120) >> 16);
    cameraScaleZ = (curtainPos * 119) >> 16;

    setModelRenderMode(&cutsceneManager->unk10.renderModeArg, cutsceneManager->enableTransparency);
    setModelCameraTransform(cutsceneManager->uiResource, 0, 0, -0xA0, cameraOffsetX, 0x9F, cameraScaleZ);
    setModelCameraTransform(cutsceneManager->shadowModel, 0, 0, -0xA0, cameraOffsetX, 0x9F, cameraScaleZ);
    setModelCameraTransform(cutsceneManager->reflectionModel, 0, 0, -0xA0, cameraOffsetX, 0x9F, cameraScaleZ);

    // @recomp draw pillarbox on top of existing frame
    enqueueCallbackBySlotIndex(cutsceneManager->uiResource->callbackSlotIndex, 7, drawCutscenePillarbox, NULL);

    return (cutsceneManager->currentFrame <= cutsceneManager->endFrame) ? 1 : 0;
}
