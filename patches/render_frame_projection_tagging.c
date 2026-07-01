/*
 * Render frame projection tagging
 *
 * Leverage Viewport metadata to track which projection transforms
 * across renders, allowing us to control rt64's interpolation behaviour
 * and disable it where it would cause noticeable artifacts.
 */

#include "patches.h"
#include "transform_ids.h"

#define DEGREES_TO_GAME_ANGLE(degrees) (((degrees) * 0x2000 + 180) / 360)
#define CAMERA_ROT_CUT_DEGREES 20
#define CAMERA_ROT_CUT_ANGLE DEGREES_TO_GAME_ANGLE(CAMERA_ROT_CUT_DEGREES)
#define CAMERA_ROT_MATRIX_SCALE 0x2000

s32 cur_perspective_projection_transform_id = 0;
s32 skip_perspective_interpolation = FALSE;
s32 cur_modelview_race_viewport_index = -1;

extern void *gDramStack;
extern void *gOutputBuffer;
extern void *gYieldBuffer;
extern long long int rspbootTextStart[];
extern long long int aspMainTextStart[];
extern Gfx *gDisplayListAllocPtr;
extern s16 gTextureEnabled;
extern void *gLookAtPtr;

void resetProjectionIds(void) {
    cur_perspective_projection_transform_id = 0;
    skip_perspective_interpolation = FALSE;
    cur_modelview_race_viewport_index = -1;
}

static s32 getRacePlayerViewportIndex(ViewportNode *node) {
    s32 transformId;

    transformId = getViewportProjectionTransformId(node);
    if ((transformId >= PROJECTION_RACE_PLAYER_TRANSFORM_ID_START) &&
        (transformId < PROJECTION_RACE_PLAYER_TRANSFORM_ID_START + 4)) {
        return transformId - PROJECTION_RACE_PLAYER_TRANSFORM_ID_START;
    }

    return -1;
}

static s32 cameraRotationExceedsThreshold(s16 prevRot[3][3], s16 curRot[3][3]) {
    s64 dotSum = 0;
    s64 traceThreshold;
    s32 cosThreshold = approximateCos(CAMERA_ROT_CUT_ANGLE);
    s32 row;
    s32 col;

    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            dotSum += (s32)prevRot[row][col] * curRot[row][col];
        }
    }

    traceThreshold = (s64)(CAMERA_ROT_MATRIX_SCALE + cosThreshold * 2) * CAMERA_ROT_MATRIX_SCALE;
    return dotSum < traceThreshold;
}

static s32 updateCameraSkipState(ViewportNode *node) {
    ViewportCameraSkipState *cameraSkipState = getViewportCameraSkipState(node);
    s32 row;
    s32 col;
    s32 rotationCut;

    if (cameraSkipState == NULL) {
        skip_perspective_interpolation = FALSE;
        return FALSE;
    }

    if (!cameraSkipState->valid) {
        cameraSkipState->valid = TRUE;
        for (row = 0; row < 3; row++) {
            for (col = 0; col < 3; col++) {
                cameraSkipState->prevRot[row][col] = node->viewTransform.m[row][col];
            }
        }
        cameraSkipState->skipInterpolation = TRUE;
        skip_perspective_interpolation = TRUE;
        return TRUE;
    }

    rotationCut = cameraRotationExceedsThreshold(cameraSkipState->prevRot, node->viewTransform.m);
    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            cameraSkipState->prevRot[row][col] = node->viewTransform.m[row][col];
        }
    }
    cameraSkipState->skipInterpolation = rotationCut;
    skip_perspective_interpolation = cameraSkipState->skipInterpolation;
    return cameraSkipState->skipInterpolation;
}

static void tagPerspectiveProjection(ViewportNode *node) {
    s32 skipInterpolation;

    if (cur_perspective_projection_transform_id != 0) {
        skipInterpolation = updateCameraSkipState(node);
        if (skipInterpolation) {
            gEXMatrixGroupSkipAll(gDisplayListAllocPtr++, cur_perspective_projection_transform_id, G_EX_NOPUSH,
                                  G_MTX_PROJECTION, G_EX_EDIT_NONE);
        } else {
            gEXMatrixGroupSimpleNormal(gDisplayListAllocPtr++, cur_perspective_projection_transform_id, G_EX_NOPUSH,
                                       G_MTX_PROJECTION, G_EX_EDIT_NONE);
        }
    } else {
        skip_perspective_interpolation = FALSE;
        gEXMatrixGroupSimpleNormal(gDisplayListAllocPtr++, G_EX_ID_AUTO, G_EX_NOPUSH, G_MTX_PROJECTION,
                                   G_EX_EDIT_NONE);
    }
}

RECOMP_PATCH void renderFrame(u32 viScanline) {
    ViewportNode *node;
    ViewportNode *rootNode;
    ViewportNode **rootNodePtr;
    Gfx *displayListStart;
    Gfx *displayListEnd;
    s32 needsDisplayListSetup;
    CallbackEntry *callbackEntry;
    void *viewportAlloc;
    void *projectionAlloc;
    s32 *lookAtAlloc;
    Light *lightArray;
    s32 i;
    u32 storedViScanline;
    s32 temp;
    s32 pipeSyncW;
    s32 prevRaceViewportIndex;
    u8 padding[0x20];

    for (node = &gRootViewport; node != NULL; node = node->list3_next) {
        if (node->fadeMode != 0) {
            temp = node->fadeValue - node->prevFadeValue;
            temp /= node->fadeMode;
            node->prevFadeValue += temp;
            node->fadeMode--;
        }
    }

    if (gFrameBufferFlags[gCurrentDoubleBufferIndex] != 0 || gFrameSkipCounter != 0) {
        if (gFrameSkipCounter != 0) {
            gFrameSkipCounter = gFrameSkipCounter - 1;
        }

        for (node = &gRootViewport; node != NULL; node = node->list3_next) {
            initViewportCallbackPool(node);
        }

        resetLinearAllocator();

        // @recomp reset projection metadata
        resetProjectionIds();

        return;
    }

    selectGraphicsArena(gCurrentDoubleBufferIndex);
    linearAllocSelectRegion(gCurrentDoubleBufferIndex);
    gFrameSkipCounter = __additional_scanline_0;
    gFrameBufferCounters[gCurrentDoubleBufferIndex] = gFrameCounter;
    updateViewportBounds();

    rootNodePtr = &gRootViewport.list3_next;
    rootNode = *rootNodePtr;
    if (!rootNode) {
        rootNode = &gRootViewport;
    }

    node = rootNode;
    needsDisplayListSetup = TRUE;
    if (node != NULL) {
        pipeSyncW = 0xE7000000;
        storedViScanline = viScanline + 3;

        do {
            temp = node->viewportFlags;

            while (node->list3_next != NULL) {
                node->frameCallbackMsg = NULL;
                if (node->list3_next->viewportFlags != (u8)temp) {
                    break;
                }

                node = node->list3_next;
            }

            node->frameCallbackMsg = arenaAlloc16(0x50);
            displayListStart = gDisplayListAllocPtr;

            gSPSegment(gDisplayListAllocPtr++, 0, 0);
            node->displayListPtr = gDisplayListAllocPtr;
            {
                Gfx *_g = gDisplayListAllocPtr++;
                _g->words.w0 = pipeSyncW;
                _g->words.w1 = 0;
            }
            {
                Gfx *_g = gDisplayListAllocPtr++;
                _g->words.w0 = pipeSyncW;
                _g->words.w1 = 0;
            }
            gDPFullSync(gDisplayListAllocPtr++);
            gSPEndDisplayList(gDisplayListAllocPtr++);
            displayListEnd = gDisplayListAllocPtr;

            if (node->list3_next == NULL) {
                node->frameCallbackMsg->taskFlags = 1;
                node->frameCallbackMsg->msgQueue = &mainMessageQueue;
                callbackEntry = (CallbackEntry *)((u32)callbackEntry & 0xFFFF);
                callbackEntry = (CallbackEntry *)((u32)callbackEntry | (gCallbackEntrySegment << 16));
                node->frameCallbackMsg->msgData = (s32)callbackEntry;
            } else {
                node->frameCallbackMsg->taskFlags = 0;
            }

            node->frameCallbackMsg->auxBuffer =
                (void *)((u8 *)gAuxFrameBuffers +
                         ((gCurrentDisplayBufferIndex * 5 * 16 - gCurrentDisplayBufferIndex * 5) << 11));
            node->frameCallbackMsg->scanlineValue = storedViScanline + __additional_scanline_0 * 2;

            node->frameCallbackMsg->t.t.data_ptr = (u64 *)displayListStart;
            node->frameCallbackMsg->t.t.data_size = (s32)displayListEnd - (s32)displayListStart;
            node->frameCallbackMsg->t.t.type = M_GFXTASK;
            node->frameCallbackMsg->t.t.flags = 0;
            node->frameCallbackMsg->t.t.ucode_boot = (u64 *)rspbootTextStart;
            node->frameCallbackMsg->t.t.ucode_boot_size = (s32)aspMainTextStart - (s32)rspbootTextStart;
            node->frameCallbackMsg->t.t.ucode = microcodeGroups[temp].ucode;
            node->frameCallbackMsg->t.t.ucode_data = microcodeGroups[temp].ucode_data;
            node->frameCallbackMsg->t.t.ucode_data_size = 0x800;
            node->frameCallbackMsg->t.t.dram_stack = (u64 *)gDramStack;
            node->frameCallbackMsg->t.t.dram_stack_size = 0x400;
            node->frameCallbackMsg->t.t.output_buff = (u64 *)gOutputBuffer;
            node->frameCallbackMsg->t.t.output_buff_size = (u64 *)((s32)gOutputBuffer + BUFFER_SIZE);
            node->frameCallbackMsg->t.t.yield_data_ptr = (u64 *)gYieldBuffer;
            node->frameCallbackMsg->t.t.yield_data_size = 0xC00;
            node = node->list3_next;
        } while (node != NULL);
    }

    node = rootNode;
    needsDisplayListSetup = TRUE;
    if (node != NULL) {
        for (node = rootNode; node != NULL; node = node->list3_next) {
            gActiveViewport = (ActiveViewportState *)node;

            if (!isRegionAllocSpaceLow() && node->clipLeft < node->clipRight && node->clipTop < node->clipBottom) {

                if (needsDisplayListSetup) {
                    displayListStart = gDisplayListAllocPtr;
                    if (gNeedsDisplayListInit != 0) {
                        gNeedsDisplayListInit = 0;
                        gSPDisplayList(gDisplayListAllocPtr++, gInitDisplayList);
                    }

                    gSPDisplayList(gDisplayListAllocPtr++, gDefaultRenderDisplayList);
                } else {
                    gDPPipeSync(gDisplayListAllocPtr++);
                }

                gDPSetScissor(gDisplayListAllocPtr++, G_SC_NON_INTERLACE, node->clipLeft, node->clipTop,
                              node->clipRight, node->clipBottom);

                if (node->displayFlags & 0x2) {
                    gDPSetCycleType(gDisplayListAllocPtr++, G_CYC_FILL);
                    gDPSetRenderMode(gDisplayListAllocPtr++, G_RM_NOOP, G_RM_NOOP2);
                    gDPSetColorImage(gDisplayListAllocPtr++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, &gFrameBuffer);
                    gDPSetFillColor(gDisplayListAllocPtr++, 0xFFFCFFFC);
                    gDPFillRectangle(gDisplayListAllocPtr++, node->clipLeft, node->clipTop, node->clipRight,
                                     node->clipBottom);
                    needsDisplayListSetup = TRUE;
                }

                if (node->displayFlags & 0x1) {
                    gDPPipeSync(gDisplayListAllocPtr++);
                    gDPSetColorImage(gDisplayListAllocPtr++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                                     (void *)((u8 *)gAuxFrameBuffers +
                                              ((gCurrentDisplayBufferIndex * 5 * 16 -
                                                gCurrentDisplayBufferIndex * 5)
                                               << 11)));
                    gDPSetCycleType(gDisplayListAllocPtr++, G_CYC_1CYCLE);
                    gDPSetEnvColor(gDisplayListAllocPtr++, node->overlayR, node->overlayG, node->overlayB, 0xFF);
                    gDPSetCombineLERP(gDisplayListAllocPtr++, 1, 0, ENVIRONMENT, 0, 1, 0, ENVIRONMENT, 0, 1, 0,
                                      ENVIRONMENT, 0, 1, 0, ENVIRONMENT, 0);
                    gDPSetRenderMode(gDisplayListAllocPtr++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
                    gDPFillRectangle(gDisplayListAllocPtr++, node->clipLeft, node->clipTop, node->clipRight + 1,
                                     node->clipBottom + 1);
                    gDPPipeSync(gDisplayListAllocPtr++);
                    gDPSetDepthImage(gDisplayListAllocPtr++, &gFrameBuffer);
                    needsDisplayListSetup = FALSE;
                }

                if (needsDisplayListSetup) {
                    needsDisplayListSetup = FALSE;
                    gDPPipeSync(gDisplayListAllocPtr++);
                    gDPSetColorImage(gDisplayListAllocPtr++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                                     (void *)((u8 *)gAuxFrameBuffers +
                                              ((gCurrentDisplayBufferIndex * 5 * 16 -
                                                gCurrentDisplayBufferIndex * 5)
                                               << 11)));
                    gDPSetDepthImage(gDisplayListAllocPtr++, &gFrameBuffer);
                }

                gTextClipAndOffsetData.clipLeft = node->clipLeft;
                gTextClipAndOffsetData.clipTop = node->clipTop;
                gTextClipAndOffsetData.clipRight = node->clipRight;
                gTextClipAndOffsetData.clipBottom = node->clipBottom;
                gTextClipAndOffsetData.offsetX = node->offsetX;
                gTextClipAndOffsetData.offsetY = node->offsetY;

                gTextureEnabled = node->viewportFlags;
                gGraphicsMode = -1;

                if (node->viewportFlags == 0) {
                    gDPSetColorDither(gDisplayListAllocPtr++, G_CD_DISABLE);

                    prevRaceViewportIndex = cur_modelview_race_viewport_index;
                    cur_modelview_race_viewport_index = getRacePlayerViewportIndex(node);
                    for (callbackEntry = (CallbackEntry *)node->pool; callbackEntry != NULL;
                         callbackEntry = callbackEntry->next) {
                        if (callbackEntry->callback == NULL) {
                            continue;
                        }

                        if (isRegionAllocSpaceLow()) {
                            break;
                        }

                        gCurrentPoolIndex = callbackEntry->poolIndex;
                        ((void (*)(void *))callbackEntry->callback)(callbackEntry->callbackData);
                        gCallbackCounter++;
                    }
                    cur_modelview_race_viewport_index = prevRaceViewportIndex;

                    if (node->prevFadeValue != 0) {
                        gSPDisplayList(gDisplayListAllocPtr++, gFadeOverlayDisplayList);
                        gDPSetPrimColor(gDisplayListAllocPtr++, 0, 0, node->envR, node->envG, node->envB,
                                        node->prevFadeValue);
                        gSPTextureRectangle(gDisplayListAllocPtr++, node->clipLeft << 2, node->clipTop << 2,
                                            node->clipRight << 2, node->clipBottom << 2, 0, 0, 0, 0x400, 0x400);
                        gDPPipeSync(gDisplayListAllocPtr++);
                    }
                } else {
                    viewportAlloc = arenaAlloc16(sizeof(node->viewportWidth) * 8);
                    projectionAlloc = arenaAlloc16(sizeof(node->projectionMatrix));
                    lookAtAlloc = arenaAlloc16(48 * sizeof(s32));

                    if (node->numLights > 0) {
                        lightArray = arenaAlloc16((node->numLights + 1) * sizeof(Light));
                        if (lightArray != NULL) {
                            for (i = 0; i < node->numLights; i++) {
                                memcpy((Light *)(i * sizeof(Light) + (u32)lightArray),
                                       (u8 *)(i * sizeof(Light) + (u32)node) + 0x148, sizeof(Light));
                                gSPLight(gDisplayListAllocPtr++, (Light *)(i * sizeof(Light) + (u32)lightArray),
                                         i + 1);
                            }

                            memcpy((Light *)(i * sizeof(Light) + (u32)lightArray),
                                   (u8 *)(i * sizeof(Light) + (u32)node) + 0x148, sizeof(Light));
                            gSPLight(gDisplayListAllocPtr++, (Light *)(i * sizeof(Light) + (u32)lightArray), i + 1);

                            gSPNumLights(gDisplayListAllocPtr++, node->numLights);
                        } else {
                            goto bail;
                        }
                    }

                    if (lookAtAlloc != NULL) {
                        memcpy(viewportAlloc, &node->viewportWidth, 16);
                        memcpy(projectionAlloc, &node->projectionMatrix, sizeof(node->projectionMatrix));

                        lookAtAlloc[0] = ((node->viewTransform.m[0][0] << 3) & 0xFFFF0000) +
                                         (u16)(((u16)node->viewTransform.m[1][0] << 16) >> 29);
                        lookAtAlloc[1] = (node->viewTransform.m[2][0] << 3) & 0xFFFF0000;
                        lookAtAlloc[2] = ((node->viewTransform.m[0][1] << 3) & 0xFFFF0000) +
                                         (u16)(((u16)node->viewTransform.m[1][1] << 16) >> 29);
                        lookAtAlloc[3] = (node->viewTransform.m[2][1] << 3) & 0xFFFF0000;
                        lookAtAlloc[4] = ((node->viewTransform.m[0][2] << 3) & 0xFFFF0000) +
                                         (u16)(((u16)node->viewTransform.m[1][2] << 16) >> 29);
                        lookAtAlloc[5] = (node->viewTransform.m[2][2] << 3) & 0xFFFF0000;
                        lookAtAlloc[6] = 0;
                        lookAtAlloc[7] = 1;
                        lookAtAlloc[8] = ((node->viewTransform.m[0][0] << 19) & 0xFFFF0000) +
                                         ((node->viewTransform.m[1][0] << 3) & 0xFFFF);
                        lookAtAlloc[9] = (node->viewTransform.m[2][0] << 19) & 0xFFFF0000;
                        lookAtAlloc[10] = ((node->viewTransform.m[0][1] << 19) & 0xFFFF0000) +
                                          ((node->viewTransform.m[1][1] << 3) & 0xFFFF);
                        lookAtAlloc[11] = (node->viewTransform.m[2][1] << 19) & 0xFFFF0000;
                        lookAtAlloc[12] = ((node->viewTransform.m[0][2] << 19) & 0xFFFF0000) +
                                          ((node->viewTransform.m[1][2] << 3) & 0xFFFF);
                        lookAtAlloc[13] = (node->viewTransform.m[2][2] << 19) & 0xFFFF0000;
                        lookAtAlloc[14] = 0;
                        lookAtAlloc[15] = 0;

                        lookAtAlloc[16] = BUFFER_SIZE;
                        lookAtAlloc[17] = 0;
                        lookAtAlloc[18] = 1;
                        lookAtAlloc[19] = 0;
                        lookAtAlloc[20] = 0;
                        lookAtAlloc[21] = BUFFER_SIZE;
                        lookAtAlloc[22] = ((-node->viewTransform.translation.x) & 0xFFFF0000) +
                                          (u16)(((u32)(-node->viewTransform.translation.y)) >> 16);
                        lookAtAlloc[23] = ((-node->viewTransform.translation.z) & 0xFFFF0000) + 1;
                        lookAtAlloc[24] = 0;
                        lookAtAlloc[25] = 0;
                        lookAtAlloc[26] = 0;
                        lookAtAlloc[27] = 0;
                        lookAtAlloc[28] = 0;
                        lookAtAlloc[29] = 0;
                        lookAtAlloc[30] =
                            -(node->viewTransform.translation.x << 16) + (u16)(-node->viewTransform.translation.y);
                        lookAtAlloc[31] = (-node->viewTransform.translation.z) << 16;

                        lookAtAlloc[32] = ((node->viewTransform.m[0][0] << 3) & 0xFFFF0000) +
                                          (u16)(((u16)node->viewTransform.m[0][1] << 16) >> 29);
                        lookAtAlloc[33] = (node->viewTransform.m[0][2] << 3) & 0xFFFF0000;
                        lookAtAlloc[34] = ((node->viewTransform.m[1][0] << 3) & 0xFFFF0000) +
                                          (u16)(((u16)node->viewTransform.m[1][1] << 16) >> 29);
                        lookAtAlloc[35] = (node->viewTransform.m[1][2] << 3) & 0xFFFF0000;
                        lookAtAlloc[36] = ((node->viewTransform.m[2][0] << 3) & 0xFFFF0000) +
                                          (u16)(((u16)node->viewTransform.m[2][1] << 16) >> 29);
                        lookAtAlloc[37] = (node->viewTransform.m[2][2] << 3) & 0xFFFF0000;
                        lookAtAlloc[38] = 0;
                        lookAtAlloc[39] = 1;
                        lookAtAlloc[40] = ((node->viewTransform.m[0][0] << 19) & 0xFFFF0000) +
                                          ((node->viewTransform.m[0][1] << 3) & 0xFFFF);
                        lookAtAlloc[41] = (node->viewTransform.m[0][2] << 19) & 0xFFFF0000;
                        lookAtAlloc[42] = ((node->viewTransform.m[1][0] << 19) & 0xFFFF0000) +
                                          ((node->viewTransform.m[1][1] << 3) & 0xFFFF);
                        lookAtAlloc[43] = (node->viewTransform.m[1][2] << 19) & 0xFFFF0000;
                        lookAtAlloc[44] = ((node->viewTransform.m[2][0] << 19) & 0xFFFF0000) +
                                          ((node->viewTransform.m[2][1] << 3) & 0xFFFF);
                        lookAtAlloc[45] = (node->viewTransform.m[2][2] << 19) & 0xFFFF0000;
                        lookAtAlloc[46] = 0;
                        lookAtAlloc[47] = 0;

                        gLookAtPtr = (void *)&lookAtAlloc[32];

                        gSPViewport(gDisplayListAllocPtr++, viewportAlloc);
                        gSPPerspNormalize(gDisplayListAllocPtr++, node->perspNorm);
                        gSPMatrix(gDisplayListAllocPtr++, projectionAlloc,
                                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
                        gSPMatrix(gDisplayListAllocPtr++, lookAtAlloc, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
                        gSPMatrix(gDisplayListAllocPtr++, &lookAtAlloc[16],
                                  G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);

                        // @recomp tag the active projection
                        s32 prevProjectionTransformId = cur_perspective_projection_transform_id;
                        cur_perspective_projection_transform_id = getViewportProjectionTransformId(node);
                        tagPerspectiveProjection(node);
                        cur_perspective_projection_transform_id = prevProjectionTransformId;

                        gSPFogPosition(gDisplayListAllocPtr++, node->fogStartPermille, node->fogEndPermille);
                        gDPSetFogColor(gDisplayListAllocPtr++, node->fogR, node->fogG, node->fogB, node->fogA);
                    } else {
                        goto bail;
                    }

                    prevRaceViewportIndex = cur_modelview_race_viewport_index;
                    cur_modelview_race_viewport_index = getRacePlayerViewportIndex(node);
                    for (callbackEntry = (CallbackEntry *)node->pool; callbackEntry != NULL;
                         callbackEntry = callbackEntry->next) {
                        if (callbackEntry->callback == NULL) {
                            continue;
                        }

                        if (isRegionAllocSpaceLow() != 0) {
                            break;
                        }

                        gCurrentPoolIndex = callbackEntry->poolIndex;
                        ((void (*)(void *))callbackEntry->callback)(callbackEntry->callbackData);
                        gCallbackCounter++;
                    }
                    cur_modelview_race_viewport_index = prevRaceViewportIndex;

                bail:
                    if (node->prevFadeValue != 0) {
                        gSPDisplayList(gDisplayListAllocPtr++, gFadeOverlayDisplayList);
                        gDPSetPrimColor(gDisplayListAllocPtr++, 0, 0, node->envR, node->envG, node->envB,
                                        node->prevFadeValue);
                        gSPTextureRectangle(gDisplayListAllocPtr++, node->clipLeft << 2, node->clipTop << 2,
                                            node->clipRight << 2, node->clipBottom << 2, 0, 0, 0, 0x400, 0x400);
                        gDPPipeSync(gDisplayListAllocPtr++);
                        gDPSetColorDither(gDisplayListAllocPtr++, G_CD_MAGICSQ);
                    }
                }
            }

            if (node->frameCallbackMsg != NULL) {
                if (!needsDisplayListSetup) {
                    if (gRootViewport.prevFadeValue != 0 && node->list3_next == 0) {
                        BorderData *bd = (BorderData *)&gRootViewport.clipLeft;

                        gSPDisplayList(gDisplayListAllocPtr++, gFadeOverlayDisplayList);

                        gDPSetScissor(gDisplayListAllocPtr++, G_SC_NON_INTERLACE, bd->clipLeft, bd->clipTop,
                                      bd->clipRight, bd->clipBottom);

                        gDPSetPrimColor(gDisplayListAllocPtr++, 0, 0, bd->envR, bd->envG, bd->envB, bd->envA);

                        gSPTextureRectangle(gDisplayListAllocPtr++, bd->clipLeft << 2, bd->clipTop << 2,
                                            bd->clipRight << 2, bd->clipBottom << 2, 0, 0, 0, 0x400, 0x400);
                    }

                    gSPEndDisplayList(gDisplayListAllocPtr++);
                    gSPDisplayList(node->displayListPtr, displayListStart);
                }
                needsDisplayListSetup = TRUE;
            }

            initViewportCallbackPool(node);
        }
    }

    if (gFrameBufferFlags[(gCurrentDoubleBufferIndex + 1) & 1] != 0) {
        gDisplayFramePending = TRUE;
    } else {
        processDisplayFrameUpdate();
    }

    resetLinearAllocator();

    // @recomp reset projection metadata
    resetProjectionIds();
}
