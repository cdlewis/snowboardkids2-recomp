#include "patches.h"
#include "transform_ids.h"
#include "system/memory_allocator.h"
#include "ui/level_preview_3d.h"

extern Gfx* gDisplayListAllocPtr;
extern Gfx gPlayerShadowRenderSetupDl[];
void setDisplayListObjectRacePlayerBones(DisplayListObject* object, u8 playerIndex);

// Display list extension table maps DisplayListObject* to additional
// information about the object, such as its matrix group ID and
// interpolation settings.
//
// Table size of 2048 showed max probe of 2 in testing
#define DISPLAY_LIST_EXTENSION_TABLE_SIZE 2048
#define DISPLAY_LIST_EXTENSION_TABLE_MASK (DISPLAY_LIST_EXTENSION_TABLE_SIZE - 1)

extern Node* gDMAOverlay;

typedef struct {
    DisplayListObject* key;
    DisplayListObjectExtension extension;
} DisplayListObjectExtensionSlot;

static DisplayListObjectExtensionSlot sDisplayListObjectExtensionSlots[DISPLAY_LIST_EXTENSION_TABLE_SIZE];

static u32 hashDisplayListObjectExtensionKey(DisplayListObject* object) {
    // 'Golden ratio' hash function
    return ((u32)object >> 2) * 0x9E3779B1;
}

static s32 findDisplayListObjectExtensionSlot(DisplayListObject* object, s32* firstEmpty) {
    u32 index;
    u32 probes;

    index = hashDisplayListObjectExtensionKey(object) & DISPLAY_LIST_EXTENSION_TABLE_MASK;
    for (probes = 0; probes < DISPLAY_LIST_EXTENSION_TABLE_SIZE; probes++) {
        if (sDisplayListObjectExtensionSlots[index].key == object) {
            return index;
        }
        if (sDisplayListObjectExtensionSlots[index].key == NULL) {
            if (firstEmpty != NULL) {
                *firstEmpty = index;
            }
            return -1;
        }
        index = (index + 1) & DISPLAY_LIST_EXTENSION_TABLE_MASK;
    }

    if (firstEmpty != NULL) {
        *firstEmpty = -1;
    }
    return -1;
}

static void clearDisplayListObjectExtensionSlot(s32 index) {
    sDisplayListObjectExtensionSlots[index].key = NULL;
    sDisplayListObjectExtensionSlots[index].extension.matrixGroupId = 0;
    sDisplayListObjectExtensionSlots[index].extension.skipInterpolation = FALSE;
    sDisplayListObjectExtensionSlots[index].extension.remainingInterpolationSkipPasses = 0;
    sDisplayListObjectExtensionSlots[index].extension.reserved = 0;
}

s32 isDisplayListObjectExtensionEmpty(DisplayListObjectExtension* extension) {
    return (extension->matrixGroupId == 0) && !extension->skipInterpolation &&
           (extension->remainingInterpolationSkipPasses == 0) && (extension->reserved == 0);
}

static void deleteDisplayListObjectExtensionSlot(s32 index) {
    s32 hole;
    s32 current;
    s32 ideal;

    hole = index;
    clearDisplayListObjectExtensionSlot(hole);

    current = (hole + 1) & DISPLAY_LIST_EXTENSION_TABLE_MASK;
    while (sDisplayListObjectExtensionSlots[current].key != NULL) {
        ideal = hashDisplayListObjectExtensionKey(sDisplayListObjectExtensionSlots[current].key) &
                DISPLAY_LIST_EXTENSION_TABLE_MASK;

        if (((current - ideal) & DISPLAY_LIST_EXTENSION_TABLE_MASK) >
            ((hole - ideal) & DISPLAY_LIST_EXTENSION_TABLE_MASK)) {
            sDisplayListObjectExtensionSlots[hole] = sDisplayListObjectExtensionSlots[current];
            hole = current;
            clearDisplayListObjectExtensionSlot(hole);
        }
        current = (current + 1) & DISPLAY_LIST_EXTENSION_TABLE_MASK;
    }
}

DisplayListObjectExtension* getDisplayListObjectExtension(DisplayListObject* object) {
    s32 index;

    if (object == NULL) {
        return NULL;
    }

    index = findDisplayListObjectExtensionSlot(object, NULL);
    if (index < 0) {
        return NULL;
    }

    return &sDisplayListObjectExtensionSlots[index].extension;
}

DisplayListObjectExtension* getOrCreateDisplayListObjectExtension(DisplayListObject* object) {
    s32 index;
    s32 firstEmpty;

    if (object == NULL) {
        return NULL;
    }

    firstEmpty = -1;
    index = findDisplayListObjectExtensionSlot(object, &firstEmpty);
    if (index >= 0) {
        return &sDisplayListObjectExtensionSlots[index].extension;
    }
    if (firstEmpty < 0) {
        return NULL;
    }

    sDisplayListObjectExtensionSlots[firstEmpty].key = object;
    return &sDisplayListObjectExtensionSlots[firstEmpty].extension;
}

void deleteDisplayListObjectExtension(DisplayListObject* object) {
    s32 index;

    if (object == NULL) {
        return;
    }

    index = findDisplayListObjectExtensionSlot(object, NULL);
    if (index >= 0) {
        deleteDisplayListObjectExtensionSlot(index);
    }
}

void deleteDisplayListObjectExtensionsInRange(void* start, u32 size) {
    u32 startAddr;
    u32 endAddr;
    s32 i;

    if ((start == NULL) || (size == 0)) {
        return;
    }

    startAddr = (u32)start;
    endAddr = startAddr + size;
    i = 0;
    while (i < DISPLAY_LIST_EXTENSION_TABLE_SIZE) {
        if ((sDisplayListObjectExtensionSlots[i].key != NULL) &&
            ((u32)sDisplayListObjectExtensionSlots[i].key >= startAddr) &&
            ((u32)sDisplayListObjectExtensionSlots[i].key < endAddr)) {
            deleteDisplayListObjectExtensionSlot(i);
        } else {
            i++;
        }
    }
}

static void deleteTaskNodeDisplayListObjectExtensions(Node* node) {
    if (node != NULL) {
        deleteDisplayListObjectExtensionsInRange(&node->payload, TASK_NODE_INLINE_PAYLOAD_SIZE);
    }
}

RECOMP_PATCH void* decrementNodeRefCount(void* allocatedMemory) {
    MemoryAllocatorNode* node;

    if (allocatedMemory == NULL) {
        return NULL;
    }

    node = ((MemoryAllocatorNode*)allocatedMemory) - 1;
    if (node->refCount <= 1) {
        deleteDisplayListObjectExtensionsInRange(allocatedMemory, node->size - sizeof(MemoryAllocatorNode));
    }
    node->refCount--;
    node->cleanupTimestamp = gFrameCounter;

    return NULL;
}

RECOMP_PATCH void terminateCurrentTask(void) {
    u8 temp;

    deleteTaskNodeDisplayListObjectExtensions(gDMAOverlay);

    temp = gDMAOverlay->unkE - 3;
    if (temp < 2) {
        gDMAOverlay->unkE = 4;
    } else {
        gDMAOverlay->unkE = 2;
    }
}

RECOMP_PATCH void terminateAllTasks(void) {
    Node* i;

    for (i = gActiveScheduler->activeList; i != NULL; i = i->next) {
        deleteTaskNodeDisplayListObjectExtensions(i);
        if ((u32)(i->unkE - 3) < 2) {
            i->unkE = 4;
        } else {
            i->unkE = 2;
        }
    }
}

RECOMP_PATCH void terminateTasksByType(s32 taskType) {
    Node* i;

    for (i = gActiveScheduler->activeList; i != NULL; i = i->next) {
        if (i->unkC == (u8)taskType) {
            deleteTaskNodeDisplayListObjectExtensions(i);
            if ((u32)(i->unkE - 3) < 2) {
                i->unkE = 4;
            } else {
                i->unkE = 2;
            }
        }
    }
}

RECOMP_PATCH void terminateTasksByTypeAndID(s32 taskType, s32 taskID) {
    Node* i;

    for (i = gActiveScheduler->activeList; i != NULL; i = i->next) {
        if (i->unkC == (u8)taskType && i->field_D == (u8)taskID) {
            deleteTaskNodeDisplayListObjectExtensions(i);
            if ((u32)(i->unkE - 3) < 2) {
                i->unkE = 4;
            } else {
                i->unkE = 2;
            }
        }
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

// @recomp Functions for tagging matrix the display list objects with their corresponding matrix group.
// This can be expanded with additional parameters to control whether interpolation is skipped,
// decomposed interpolation is used, etc.
//
// TODO: This needs to be replaced with a better ID system that pulls the ID associated to the object
// and also combines it with the current viewport ID. Pointer alone shouldn't be relied on if possible.
// This extra structure could also store additional information to control interpolation in greater detail.

void pushObjectMatrixGroupForObject(DisplayListObject* object, u32 id) {
    if (consumeBoardSelectObjectInterpolationSkip(object)) {
        // @recomp Board-select marked this object as a teleport, so skip modelview interpolation for this group.
        gEXMatrixGroupSkipAll(gDisplayListAllocPtr++, id, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    } else {
        gEXMatrixGroupSimple(gDisplayListAllocPtr++, id, G_EX_PUSH, G_MTX_MODELVIEW,
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP,
                             G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO, G_EX_ORDER_LINEAR, G_EX_EDIT_NONE,
                             G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO);
    }
}

void pushObjectMatrixGroupId(u32 id) {
    pushObjectMatrixGroupForObject((DisplayListObject*)id, id);
}

void pushObjectMatrixGroup(DisplayListObject* displayObjects) {
    pushObjectMatrixGroupForObject(displayObjects, (u32)displayObjects);
}

void popObjectMatrixGroup() {
    gEXPopMatrixGroup(gDisplayListAllocPtr++, G_MTX_MODELVIEW);
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderMultiPartOpaqueDisplayLists(DisplayListObject* displayObjects) {
    s32 i;
    DisplayListObject* currentObject;
    Gfx* displayListCmd;
    volatile u8 padding[0x40];

    initializeMultiPartDisplayListObjects(displayObjects);

    for (i = 0; i < displayObjects->numParts; i++) {
        currentObject = &displayObjects[i];
        if (currentObject->displayLists->opaqueDisplayList != NULL) {
            // @recomp Push the matrix group for this part.
            pushObjectMatrixGroupForObject(&displayObjects[i], (u32)displayObjects + i);

            setupMultiPartObjectRenderState(displayObjects, i);
            displayListCmd = gDisplayListAllocPtr;
            displayListCmd->words.w0 = 0xDE000000;
            displayListCmd->words.w1 = (u32) currentObject->displayLists->opaqueDisplayList;
            gDisplayListAllocPtr = displayListCmd + 1;

            // @recomp Pop the matrix group.
            popObjectMatrixGroup();
        }
    }
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderMultiPartTransparentDisplayLists(DisplayListObject* displayObjects) {
    DisplayListObject* currentObject;
    s32 i;
    Gfx* displayListCmd;
    s32 objectCount;
    u8 pad[0x48];

    initializeMultiPartDisplayListObjects(displayObjects);

    objectCount = displayObjects->numParts;
    if (objectCount > 0) {
        currentObject = displayObjects;
        i = 0;
        do {
            if (currentObject[i].displayLists->transparentDisplayList != NULL) {
                // @recomp Push the matrix group for this part.
                pushObjectMatrixGroupForObject(&displayObjects[i], (u32)displayObjects + i);

                setupMultiPartObjectRenderState(displayObjects, i);
                displayListCmd = gDisplayListAllocPtr;
                displayListCmd->words.w0 = 0xDE000000;
                displayListCmd->words.w1 = (u32) currentObject[i].displayLists->transparentDisplayList;
                gDisplayListAllocPtr = displayListCmd + 1;

                // @recomp Pop the matrix group.
                popObjectMatrixGroup();
            }
            i++;
            objectCount = displayObjects->numParts;
        } while (i < objectCount);
    }
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderMultiPartOverlayDisplayLists(DisplayListObject* displayObjects) {
    DisplayListObject* currentObject;
    s32 i;
    Gfx* displayListCmd;
    s32 objectCount;
    u8 pad[0x48];

    initializeMultiPartDisplayListObjects(displayObjects);

    objectCount = displayObjects->numParts;
    if (objectCount > 0) {
        currentObject = displayObjects;
        i = 0;
        do {
            if (currentObject[i].displayLists->overlayDisplayList != NULL) {
                // @recomp Push the matrix group for this part.
                pushObjectMatrixGroupForObject(&displayObjects[i], (u32)displayObjects + i);

                setupMultiPartObjectRenderState(displayObjects, i);
                displayListCmd = gDisplayListAllocPtr;
                displayListCmd->words.w0 = 0xDE000000;
                displayListCmd->words.w1 = (u32) currentObject[i].displayLists->overlayDisplayList;
                gDisplayListAllocPtr = displayListCmd + 1;

                // @recomp Pop the matrix group.
                popObjectMatrixGroup();
            }
            i++;
            objectCount = displayObjects->numParts;
        } while (i < objectCount);
    }
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderMultiPartOpaqueDisplayListsWithLights(DisplayListObject* displayObjects) {
    DisplayListObject* currentObject;
    s32 i;
    Gfx* displayListCmd;
    s32 objectCount;
    u8 pad[0x44];

    initializeMultiPartDisplayListObjects(displayObjects);

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_1,
                  displayObjects->light1R << 0x18 | displayObjects->light1G << 0x10 | displayObjects->light1B << 8);

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_2,
                  displayObjects->light2R << 0x18 | displayObjects->light2G << 0x10 | displayObjects->light2B << 8);

    i = 0;
    objectCount = displayObjects->numParts;
    if (objectCount > 0) {
        currentObject = displayObjects;
        do {
            if (currentObject[i].displayLists->opaqueDisplayList != NULL) {
                // @recomp Push the matrix group for this part.
                pushObjectMatrixGroupForObject(&displayObjects[i], (u32)displayObjects + i);

                setupMultiPartObjectRenderState(displayObjects, i);
                displayListCmd = gDisplayListAllocPtr;
                displayListCmd->words.w0 = 0xDE000000;
                displayListCmd->words.w1 = (u32) currentObject[i].displayLists->opaqueDisplayList;
                gDisplayListAllocPtr = displayListCmd + 1;

                // @recomp Pop the matrix group.
                popObjectMatrixGroup();
            }
            i += 1;
        } while (i < (s32) displayObjects->numParts);
    }

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_1,
                  gActiveViewport->defaultLight1R << 0x18 | gActiveViewport->defaultLight1G << 0x10 |
                      gActiveViewport->defaultLight1B << 8);

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_2,
                  gActiveViewport->defaultLight2R << 0x18 | gActiveViewport->defaultLight2G << 0x10 |
                      gActiveViewport->defaultLight2B << 8);
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderMultiPartTransparentDisplayListsWithLights(DisplayListObject* displayObjects) {
    DisplayListObject* currentObject;
    s32 i;
    Gfx* displayListCmd;
    s32 objectCount;
    u8 pad[0x44];

    initializeMultiPartDisplayListObjects(displayObjects);

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_1,
                  displayObjects->light1R << 0x18 | displayObjects->light1G << 0x10 | displayObjects->light1B << 8);

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_2,
                  displayObjects->light2R << 0x18 | displayObjects->light2G << 0x10 | displayObjects->light2B << 8);

    i = 0;
    objectCount = displayObjects->numParts;
    if (objectCount > 0) {
        currentObject = displayObjects;
        do {
            if (currentObject[i].displayLists->transparentDisplayList != NULL) {
                // @recomp Push the matrix group for this part.
                pushObjectMatrixGroupForObject(&displayObjects[i], (u32)displayObjects + i);

                setupMultiPartObjectRenderState(displayObjects, i);
                displayListCmd = gDisplayListAllocPtr;
                displayListCmd->words.w0 = 0xDE000000;
                displayListCmd->words.w1 = (u32) currentObject[i].displayLists->transparentDisplayList;
                gDisplayListAllocPtr = displayListCmd + 1;

                // @recomp Pop the matrix group.
                popObjectMatrixGroup();
            }
            i += 1;
        } while (i < (s32) displayObjects->numParts);
    }

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_1,
                  gActiveViewport->defaultLight1R << 0x18 | gActiveViewport->defaultLight1G << 0x10 |
                      gActiveViewport->defaultLight1B << 8);

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_2,
                  gActiveViewport->defaultLight2R << 0x18 | gActiveViewport->defaultLight2G << 0x10 |
                      gActiveViewport->defaultLight2B << 8);
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderMultiPartOverlayDisplayListsWithLights(DisplayListObject* displayObjects) {
    DisplayListObject* currentObject;
    s32 i;
    Gfx* displayListCmd;
    s32 objectCount;
    u8 pad[0x44];

    initializeMultiPartDisplayListObjects(displayObjects);

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_1,
                  displayObjects->light1R << 0x18 | displayObjects->light1G << 0x10 | displayObjects->light1B << 8);

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_2,
                  displayObjects->light2R << 0x18 | displayObjects->light2G << 0x10 | displayObjects->light2B << 8);

    i = 0;
    objectCount = displayObjects->numParts;
    if (objectCount > 0) {
        currentObject = displayObjects;
        do {
            if (currentObject[i].displayLists->overlayDisplayList != NULL) {
                // @recomp Push the matrix group for this part.
                pushObjectMatrixGroupForObject(&displayObjects[i], (u32)displayObjects + i);

                setupMultiPartObjectRenderState(displayObjects, i);
                displayListCmd = gDisplayListAllocPtr;
                displayListCmd->words.w0 = 0xDE000000;
                displayListCmd->words.w1 = (u32) currentObject[i].displayLists->overlayDisplayList;
                gDisplayListAllocPtr = displayListCmd + 1;

                // @recomp Pop the matrix group.
                popObjectMatrixGroup();
            }
            i += 1;
        } while (i < (s32) displayObjects->numParts);
    }

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_1,
                  gActiveViewport->defaultLight1R << 0x18 | gActiveViewport->defaultLight1G << 0x10 |
                      gActiveViewport->defaultLight1B << 8);

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_2,
                  gActiveViewport->defaultLight2R << 0x18 | gActiveViewport->defaultLight2G << 0x10 |
                      gActiveViewport->defaultLight2B << 8);
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderOpaqueDisplayList(DisplayListObject* arg0) {
    // @recomp Push the matrix group for this object.
    pushObjectMatrixGroup(arg0);

    prepareDisplayListRenderState(arg0);
    gSPDisplayList(gDisplayListAllocPtr++, arg0->displayLists->opaqueDisplayList);

    // @recomp Pop the matrix group.
    popObjectMatrixGroup();
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderTransparentDisplayList(DisplayListObject* arg0) {
    // @recomp Push the matrix group for this object.
    pushObjectMatrixGroup(arg0);

    prepareDisplayListRenderState(arg0);
    gSPDisplayList(gDisplayListAllocPtr++, arg0->displayLists->transparentDisplayList);

    // @recomp Pop the matrix group.
    popObjectMatrixGroup();
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderOverlayDisplayList(DisplayListObject* arg0) {
    // @recomp Push the matrix group for this object.
    pushObjectMatrixGroup(arg0);

    prepareDisplayListRenderState(arg0);
    gSPDisplayList(gDisplayListAllocPtr++, arg0->displayLists->overlayDisplayList);

    // @recomp Pop the matrix group.
    popObjectMatrixGroup();
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderOpaqueDisplayListCallback(DisplayListObject* obj) {
    // @recomp Push the matrix group for this object.
    pushObjectMatrixGroup(obj);

    setupDisplayListMatrix(obj);
    gSPDisplayList(gDisplayListAllocPtr++, obj->displayLists->opaqueDisplayList);

    // @recomp Pop the matrix group.
    popObjectMatrixGroup();
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderTransparentDisplayListCallback(DisplayListObject* obj) {
    // @recomp Push the matrix group for this object.
    pushObjectMatrixGroup(obj);

    setupDisplayListMatrix(obj);
    gSPDisplayList(gDisplayListAllocPtr++, obj->displayLists->transparentDisplayList);

    // @recomp Pop the matrix group.
    popObjectMatrixGroup();
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderOverlayDisplayListCallback(DisplayListObject* obj) {
    // @recomp Push the matrix group for this object.
    pushObjectMatrixGroup(obj);

    setupDisplayListMatrix(obj);
    gSPDisplayList(gDisplayListAllocPtr++, obj->displayLists->overlayDisplayList);

    // @recomp Pop the matrix group.
    popObjectMatrixGroup();
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderOpaqueDisplayListWithFrustumCull(DisplayListObject* arg0) {
    if (!isObjectCulled(&arg0->transform.translation)) {
        // @recomp Push the matrix group for this object.
        pushObjectMatrixGroup(arg0);

        setupDisplayListMatrix(arg0);
        gSPDisplayList(gDisplayListAllocPtr++, arg0->displayLists->opaqueDisplayList);

        // @recomp Pop the matrix group.
        popObjectMatrixGroup();
    }
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderTransparentDisplayListWithFrustumCull(DisplayListObject* arg0) {
    if (!isObjectCulled(&arg0->transform.translation)) {
        // @recomp Push the matrix group for this object.
        pushObjectMatrixGroup(arg0);

        setupDisplayListMatrix(arg0);
        gSPDisplayList(gDisplayListAllocPtr++, arg0->displayLists->transparentDisplayList);

        // @recomp Pop the matrix group.
        popObjectMatrixGroup();
    }
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderOverlayDisplayListWithFrustumCull(DisplayListObject* arg0) {
    s32* temp_v1;

    if (!isObjectCulled(&arg0->transform.translation)) {
        // @recomp Push the matrix group for this object.
        pushObjectMatrixGroup(arg0);

        setupDisplayListMatrix(arg0);
        gSPDisplayList(gDisplayListAllocPtr++, arg0->displayLists->overlayDisplayList);

        // @recomp Pop the matrix group.
        popObjectMatrixGroup();
    }
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderBillboardedOpaqueDisplayList(DisplayListObject* arg0) {
    if (!isObjectCulled(&arg0->transform.translation)) {
        // @recomp Push the matrix group for this object.
        pushObjectMatrixGroup(arg0);

        setupBillboardDisplayListMatrix(arg0);
        gSPDisplayList(gDisplayListAllocPtr++, arg0->displayLists->opaqueDisplayList);

        // @recomp Pop the matrix group.
        popObjectMatrixGroup();
    }
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderBillboardedTransparentDisplayList(DisplayListObject* arg0) {
    if (!isObjectCulled(&arg0->transform.translation)) {
        // @recomp Push the matrix group for this object.
        pushObjectMatrixGroup(arg0);

        setupBillboardDisplayListMatrix(arg0);
        gSPDisplayList(gDisplayListAllocPtr++, arg0->displayLists->transparentDisplayList);

        // @recomp Pop the matrix group.
        popObjectMatrixGroup();
    }
}

// @recomp Modified to insert matrix groups around the display list.
RECOMP_PATCH void renderBillboardedOverlayDisplayList(DisplayListObject* arg0) {
    if (!isObjectCulled(&arg0->transform.translation)) {
        // @recomp Push the matrix group for this object.
        pushObjectMatrixGroup(arg0);

        setupBillboardDisplayListMatrix(arg0);
        gSPDisplayList(gDisplayListAllocPtr++, arg0->displayLists->overlayDisplayList);

        // @recomp Pop the matrix group.
        popObjectMatrixGroup();
    }
}

// @recomp Modified to insert matrix groups around lit display lists.
RECOMP_PATCH void renderOpaqueDisplayListWithLights(DisplayListObject *arg0) {
    pushObjectMatrixGroup(arg0);

    prepareDisplayListRenderStateWithLights(arg0);
    gSPDisplayList(gDisplayListAllocPtr++, arg0->displayLists->opaqueDisplayList);

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_1,
                  gActiveViewport->defaultLight1R << 0x18 | gActiveViewport->defaultLight1G << 0x10 |
                      gActiveViewport->defaultLight1B << 8);
    gSPLightColor(gDisplayListAllocPtr++, LIGHT_2,
                  gActiveViewport->defaultLight2R << 0x18 | gActiveViewport->defaultLight2G << 0x10 |
                      gActiveViewport->defaultLight2B << 8);

    popObjectMatrixGroup();
}

// @recomp Modified to insert matrix groups around lit display lists.
RECOMP_PATCH void renderTransparentDisplayListWithLights(DisplayListObject *arg0) {
    pushObjectMatrixGroup(arg0);

    prepareDisplayListRenderStateWithLights(arg0);
    gSPDisplayList(gDisplayListAllocPtr++, arg0->displayLists->transparentDisplayList);

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_1,
                  gActiveViewport->defaultLight1R << 0x18 | gActiveViewport->defaultLight1G << 0x10 |
                      gActiveViewport->defaultLight1B << 8);
    gSPLightColor(gDisplayListAllocPtr++, LIGHT_2,
                  gActiveViewport->defaultLight2R << 0x18 | gActiveViewport->defaultLight2G << 0x10 |
                      gActiveViewport->defaultLight2B << 8);

    popObjectMatrixGroup();
}

// @recomp Modified to insert matrix groups around lit display lists.
RECOMP_PATCH void renderOverlayDisplayListWithLights(DisplayListObject *arg0) {
    pushObjectMatrixGroup(arg0);

    prepareDisplayListRenderStateWithLights(arg0);
    gSPDisplayList(gDisplayListAllocPtr++, arg0->displayLists->overlayDisplayList);

    gSPLightColor(gDisplayListAllocPtr++, LIGHT_1,
                  gActiveViewport->defaultLight1R << 0x18 | gActiveViewport->defaultLight1G << 0x10 |
                      gActiveViewport->defaultLight1B << 8);
    gSPLightColor(gDisplayListAllocPtr++, LIGHT_2,
                  gActiveViewport->defaultLight2R << 0x18 | gActiveViewport->defaultLight2G << 0x10 |
                      gActiveViewport->defaultLight2B << 8);

    popObjectMatrixGroup();
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
        // @recomp Push the matrix group for this player. Use the address of the player's shadow matrix pointer as its ID.
        // TODO: This can be replaced with a better ID system by extending Player data as well.
        gEXMatrixGroupSimple(gDisplayListAllocPtr++, (u32)(&player->shadowMatrix), G_EX_PUSH, G_MTX_MODELVIEW,
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP,
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_AUTO, G_EX_ORDER_LINEAR, G_EX_EDIT_NONE,
                             G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO);

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
