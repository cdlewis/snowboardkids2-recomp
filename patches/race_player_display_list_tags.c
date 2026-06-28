#include "patches.h"

#define MODELVIEW_RACE_PLAYER_SHADOW_ID_BASE 0xB2000000

extern Gfx* gDisplayListAllocPtr;
extern Gfx gPlayerShadowRenderSetupDl[];
extern s32 cur_modelview_race_viewport_index;

static void setDisplayListObjectRacePlayerBones(DisplayListObject* object, u8 playerIndex) {
    DisplayListObjectExtension* extension;

    extension = getOrCreateDisplayListObjectExtension(object);
    if (extension != NULL) {
        extension->hasRacePlayerBoneOwner = TRUE;
        extension->racePlayerIndex = playerIndex;
    }
}

static void tagRacePlayerMultipartBones(DisplayListObject* displayObjects) {
    GameState* gameState;
    Player* player;
    s32 i;

    gameState = (GameState*)getCurrentAllocation();
    if ((displayObjects == NULL) || (gameState == NULL) || (gameState->players == NULL) ||
        (gameState->numPlayers > 16)) {
        return;
    }

    for (i = 0; i < gameState->numPlayers; i++) {
        player = &gameState->players[i];
        if ((displayObjects == player->boneDisplayObjects) && (player->flyingAttackState == 0)) {
            setDisplayListObjectRacePlayerBones(displayObjects, player->playerIndex);
            return;
        }
    }
}

static u32 getRacePlayerShadowMatrixGroupId(u8 playerIndex) {
    return MODELVIEW_RACE_PLAYER_SHADOW_ID_BASE | ((cur_modelview_race_viewport_index & 0x3) << 20) |
           ((playerIndex & 0xF) << 16);
}

static s32 isRacePlayerShadow(Player* player) {
    return (cur_modelview_race_viewport_index >= 0) && (player->playerIndex < 4);
}

static void pushRacerProjectedShadowMatrixGroup(Player* player) {
    u32 id;

    if (isRacePlayerShadow(player)) {
        id = getRacePlayerShadowMatrixGroupId(player->playerIndex);
        gEXMatrixGroupSimple(gDisplayListAllocPtr++, id, G_EX_PUSH, G_MTX_MODELVIEW,
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP,
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_AUTO, G_EX_ORDER_AUTO, G_EX_EDIT_NONE,
                             G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO);
    } else {
        gEXMatrixGroupSimple(gDisplayListAllocPtr++, (u32)(&player->shadowMatrix), G_EX_PUSH, G_MTX_MODELVIEW,
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP,
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_AUTO, G_EX_ORDER_LINEAR, G_EX_EDIT_NONE,
                             G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO);
    }
}

// @recomp modified to associate the display list object with a particular player id
RECOMP_PATCH void enqueuePreLitMultiPartDisplayList(s32 arg0, DisplayListObject* arg1, s32 arg2) {
    DisplayListObject* new_var;
    s32 i;
    s32 renderFlags;
    DisplayLists* dlPtrs;
    DisplayListObject* currentPart;
    volatile u8 padding[0x1];

    // @recomp Tag the display list object with its associated player id
    tagRacePlayerMultipartBones(arg1);

    i = 0;
    renderFlags = 0;
    arg1->numParts = arg2;

    if (arg2 > 0) {
        currentPart = arg1;
        do {
            dlPtrs = currentPart->displayLists;
            (new_var = currentPart)->transformMatrix = 0;
            if (dlPtrs->opaqueDisplayList != 0) {
                renderFlags |= 1;
            }
            if (dlPtrs->transparentDisplayList != 0) {
                renderFlags |= 2;
            }
            if (dlPtrs->overlayDisplayList != 0) {
                renderFlags |= 4;
            }
            i += 1;
            currentPart++;
        } while (i < arg2);
    }

    if (renderFlags & 1) {
        enqueueCallbackBySlotIndex(arg0 & 0xFFFF, 1, &renderMultiPartOpaqueDisplayLists, arg1);
    }
    new_var = arg1;
    if (renderFlags & 2) {
        enqueueCallbackBySlotIndex((arg0 & 0xFFFF) ^ 0, 3, &renderMultiPartTransparentDisplayLists, new_var);
    }
    if (renderFlags & 4) {
        enqueueCallbackBySlotIndex(arg0 & 0xFFFF, 5, &renderMultiPartOverlayDisplayLists, arg1);
    }
}

// @recomp modified to associate the display list object with a particular player id
RECOMP_PATCH void enqueueMultiPartDisplayList(s32 arg0, DisplayListObject* arg1, s32 arg2) {
    DisplayListObject* new_var;
    s32 i;
    s32 renderFlags;
    DisplayLists* dlPtrs;
    DisplayListObject* currentPart;
    volatile u8 padding[0x1];

    // @recomp Tag the display list object with its associated player id
    tagRacePlayerMultipartBones(arg1);

    i = 0;
    renderFlags = 0;
    arg1->numParts = arg2;
    if (arg2 > 0) {
        currentPart = arg1;
        do {
            dlPtrs = currentPart->displayLists;
            (new_var = currentPart)->transformMatrix = 0;
            if (dlPtrs->opaqueDisplayList != 0) {
                renderFlags |= 1;
            }
            if (dlPtrs->transparentDisplayList != 0) {
                renderFlags |= 2;
            }
            if (dlPtrs->overlayDisplayList != 0) {
                renderFlags |= 4;
            }
            i += 1;
            currentPart++;
        } while (i < arg2);
    }
    if (renderFlags & 1) {
        enqueueCallbackBySlotIndex(arg0 & 0xFFFF, 1, &renderMultiPartOpaqueDisplayListsWithLights, arg1);
    }
    new_var = arg1;
    if (renderFlags & 2) {
        enqueueCallbackBySlotIndex((arg0 & 0xFFFF) ^ 0, 3, &renderMultiPartTransparentDisplayListsWithLights,
                                   new_var);
    }
    if (renderFlags & 4) {
        enqueueCallbackBySlotIndex(arg0 & 0xFFFF, 5, &renderMultiPartOverlayDisplayListsWithLights, arg1);
    }
}

// @recomp Modified to insert a matrix group around the shadow.
RECOMP_PATCH void renderRacerProjectedShadow(Player* player) {
    OutputStruct_19E80 shadowTexture;
    s32 shadowAlpha;
    s32 sampleIndex;

    if (player->shadowMeshNeedsUpdate != 0) {
        player->shadowVertices = arenaAlloc16(0x90);
        if (player->shadowVertices != NULL) {
            sampleIndex = 0;
            shadowAlpha = 0x50;
            do {
                s32 vertexByteOffset = sampleIndex << 4;
                *(s16*) (vertexByteOffset + (s32) player->shadowVertices) =
                    (s16) ((*(volatile s32*) &player->shadowSamplePositions[sampleIndex].x -
                            player->shadowSamplePositions[0].x) >>
                           14);
                *(s16*) (vertexByteOffset + (s32) player->shadowVertices + 2) =
                    (s16) ((*(volatile s32*) &player->shadowSamplePositions[sampleIndex].y -
                            player->shadowSamplePositions[0].y) >>
                           14);
                *(s16*) (vertexByteOffset + (s32) player->shadowVertices + 4) =
                    (s16) ((*(volatile s32*) &player->shadowSamplePositions[sampleIndex].z -
                            player->shadowSamplePositions[0].z) >>
                           14);
                *(u8*) (vertexByteOffset + (s32) player->shadowVertices + 0xC) = 0;
                *(u8*) (vertexByteOffset + (s32) player->shadowVertices + 0xD) = 0;
                *(u8*) (vertexByteOffset + (s32) player->shadowVertices + 0xE) = 0;
                *(u8*) (vertexByteOffset + (s32) player->shadowVertices + 0xF) = shadowAlpha;
                sampleIndex++;
            } while (sampleIndex < 9);

            player->shadowVertices[0].v.tc[0] = -0x20;
            player->shadowVertices[0].v.tc[1] = -0x20;
            player->shadowVertices[1].v.tc[0] = 0x3E0;
            player->shadowVertices[1].v.tc[1] = -0x20;
            player->shadowVertices[2].v.tc[0] = 0x7E0;
            player->shadowVertices[2].v.tc[1] = -0x20;
            player->shadowVertices[3].v.tc[0] = -0x20;
            player->shadowVertices[3].v.tc[1] = 0x3E0;
            player->shadowVertices[4].v.tc[0] = 0x3E0;
            player->shadowVertices[4].v.tc[1] = 0x3E0;
            player->shadowVertices[5].v.tc[0] = 0x7E0;
            player->shadowVertices[5].v.tc[1] = 0x3E0;
            player->shadowVertices[6].v.tc[0] = -0x20;
            player->shadowVertices[6].v.tc[1] = 0x7E0;
            player->shadowVertices[7].v.tc[0] = 0x3E0;
            player->shadowVertices[7].v.tc[1] = 0x7E0;
            player->shadowVertices[8].v.tc[0] = 0x7E0;
            player->shadowVertices[8].v.tc[1] = 0x7E0;
        }

        player->shadowMatrix = arenaAlloc16(0x40);
        if (player->shadowMatrix != NULL) {
            memcpy(&gScaleMatrix.translation, &player->shadowSamplePositions[0], sizeof(Vec3i));
            transform3DToN64Mtx(&gScaleMatrix, player->shadowMatrix);
        }
        player->shadowMeshNeedsUpdate = 0;
    }

    if (player->shadowVertices != NULL && player->shadowMatrix != NULL) {
        // @recomp Push the matrix group for this shadow.
        pushRacerProjectedShadowMatrixGroup(player);

        gSPMatrix(gDisplayListAllocPtr++, player->shadowMatrix, (G_MTX_NOPUSH | G_MTX_LOAD) | G_MTX_MODELVIEW);
        gGraphicsMode = -1;
        gSPDisplayList(gDisplayListAllocPtr++, gPlayerShadowRenderSetupDl);
        getTableEntryByU16Index(player->assetTable, 0, &shadowTexture);

        gDPSetTextureImage(gDisplayListAllocPtr++, G_IM_FMT_I, G_IM_SIZ_16b, 1, shadowTexture.data_ptr);
        gDPSetTile(gDisplayListAllocPtr++, G_IM_FMT_I, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, G_TX_CLAMP, 5, G_TX_NOLOD,
                   G_TX_CLAMP, 5, G_TX_NOLOD);
        gDPLoadSync(gDisplayListAllocPtr++);
        gDPLoadBlock(gDisplayListAllocPtr++, G_TX_LOADTILE, 0, 0, 255, 1024);
        gDPPipeSync(gDisplayListAllocPtr++);
        gDPSetTile(gDisplayListAllocPtr++, G_IM_FMT_I, G_IM_SIZ_4b, 2, 0, G_TX_RENDERTILE, 0, G_TX_CLAMP, 5, G_TX_NOLOD,
                   G_TX_CLAMP, 5, G_TX_NOLOD);
        gDPSetTileSize(gDisplayListAllocPtr++, G_TX_RENDERTILE, 0, 0, 31 << 2, 31 << 2);

        gSPVertex(gDisplayListAllocPtr++, player->shadowVertices, 9, 0);
        gSP2Triangles(gDisplayListAllocPtr++, 0, 1, 3, 0, 1, 4, 3, 0);
        gSP2Triangles(gDisplayListAllocPtr++, 1, 2, 5, 0, 5, 4, 1, 0);
        gSP2Triangles(gDisplayListAllocPtr++, 3, 4, 7, 0, 7, 6, 3, 0);
        gSP2Triangles(gDisplayListAllocPtr++, 4, 5, 7, 0, 5, 8, 7, 0);

        // @recomp Pop the matrix group.
        gEXPopMatrixGroup(gDisplayListAllocPtr++, G_MTX_MODELVIEW);
    }
}
