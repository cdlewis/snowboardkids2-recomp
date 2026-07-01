#include "patches.h"

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

RECOMP_PATCH void enqueuePreLitMultiPartDisplayList(s32 arg0, DisplayListObject* arg1, s32 arg2) {
    DisplayListObject* new_var;
    s32 i;
    s32 renderFlags;
    DisplayLists* dlPtrs;
    DisplayListObject* currentPart;
    volatile u8 padding[0x1];

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

RECOMP_PATCH void enqueueMultiPartDisplayList(s32 arg0, DisplayListObject* arg1, s32 arg2) {
    DisplayListObject* new_var;
    s32 i;
    s32 renderFlags;
    DisplayLists* dlPtrs;
    DisplayListObject* currentPart;
    volatile u8 padding[0x1];

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
