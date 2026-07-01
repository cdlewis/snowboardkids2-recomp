/*
 * Display list extension table maps DisplayListObject* to additional
 * information about the object, such as its player ID and interpolation
 * settings.
 */

#include "patches.h"
#include "transform_ids.h"
#include "system/memory_allocator.h"
#include "ui/level_preview_3d.h"

extern Gfx* gDisplayListAllocPtr;
extern Node* gDMAOverlay;
extern s32 cur_modelview_race_viewport_index;

#define MODELVIEW_RACE_PLAYER_BONE_ID_BASE 0xB1000000

// Table size of 2048 showed max probe of 2 in testing
#define DISPLAY_LIST_EXTENSION_TABLE_SIZE 2048
#define DISPLAY_LIST_EXTENSION_TABLE_MASK (DISPLAY_LIST_EXTENSION_TABLE_SIZE - 1)

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
    sDisplayListObjectExtensionSlots[index].extension.hasRacePlayerBoneOwner = FALSE;
    sDisplayListObjectExtensionSlots[index].extension.racePlayerIndex = 0;
}

s32 isDisplayListObjectExtensionEmpty(DisplayListObjectExtension* extension) {
    return (extension->matrixGroupId == 0) && !extension->skipInterpolation &&
           (extension->remainingInterpolationSkipPasses == 0) && !extension->hasRacePlayerBoneOwner;
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

static u32 getRacePlayerBoneMatrixGroupId(u8 playerIndex, s32 boneIndex) {
    return MODELVIEW_RACE_PLAYER_BONE_ID_BASE | ((cur_modelview_race_viewport_index & 0x3) << 20) |
           ((playerIndex & 0xF) << 16) | (boneIndex & 0xFF);
}

static u32 getMultiPartMatrixGroupId(DisplayListObject* displayObjects, s32 boneIndex) {
    DisplayListObjectExtension* extension;
    u32 id;

    extension = getDisplayListObjectExtension(displayObjects);
    if ((extension != NULL) && extension->hasRacePlayerBoneOwner && (cur_modelview_race_viewport_index >= 0)) {
        id = getRacePlayerBoneMatrixGroupId(extension->racePlayerIndex, boneIndex);
        return id;
    }

    id = (u32)&displayObjects[boneIndex];
    return id;
}

static s32 isRacePlayerMultiPartBone(DisplayListObject* displayObjects) {
    DisplayListObjectExtension* extension;

    extension = getDisplayListObjectExtension(displayObjects);
    return (extension != NULL) && extension->hasRacePlayerBoneOwner && (cur_modelview_race_viewport_index >= 0);
}

void pushObjectMatrixGroupForObject(DisplayListObject* object, u32 id) {
    if (consumeBoardSelectObjectInterpolationSkip(object)) {
        // Board-select marked this object as a teleport, so skip modelview interpolation for this group.
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

static void pushMultiPartMatrixGroup(DisplayListObject* displayObjects, s32 boneIndex) {
    u32 id;

    id = getMultiPartMatrixGroupId(displayObjects, boneIndex);
    if (isRacePlayerMultiPartBone(displayObjects)) {
        gEXMatrixGroupSimple(gDisplayListAllocPtr++, id, G_EX_PUSH, G_MTX_MODELVIEW,
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP,
                             G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO, G_EX_ORDER_AUTO, G_EX_EDIT_NONE,
                             G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO);
    } else {
        pushObjectMatrixGroupForObject(&displayObjects[boneIndex], id);
    }
}

void popObjectMatrixGroup() {
    gEXPopMatrixGroup(gDisplayListAllocPtr++, G_MTX_MODELVIEW);
}

// @recomp Modified to clean up extension data for de-allocated display list objects
RECOMP_PATCH void* decrementNodeRefCount(void* allocatedMemory) {
    MemoryAllocatorNode* node;

    if (allocatedMemory == NULL) {
        return NULL;
    }

    node = ((MemoryAllocatorNode*)allocatedMemory) - 1;
    if (node->refCount <= 1) {
        // @recomp Free the extension data for any display list objects in this memory range
        deleteDisplayListObjectExtensionsInRange(allocatedMemory, node->size - sizeof(MemoryAllocatorNode));
    }
    node->refCount--;
    node->cleanupTimestamp = gFrameCounter;

    return NULL;
}

// @recomp Modified to clean up extension data for de-allocated display list objects
RECOMP_PATCH void terminateCurrentTask(void) {
    u8 temp;

    // @recomp Free any extension data for display list objects at this address
    deleteTaskNodeDisplayListObjectExtensions(gDMAOverlay);

    temp = gDMAOverlay->unkE - 3;
    if (temp < 2) {
        gDMAOverlay->unkE = 4;
    } else {
        gDMAOverlay->unkE = 2;
    }
}

// @recomp Modified to clean up extension data for de-allocated display list objects
RECOMP_PATCH void terminateAllTasks(void) {
    Node* i;

    for (i = gActiveScheduler->activeList; i != NULL; i = i->next) {
        // @recomp Free any extension data for display list objects at this address
        deleteTaskNodeDisplayListObjectExtensions(i);
        if ((u32)(i->unkE - 3) < 2) {
            i->unkE = 4;
        } else {
            i->unkE = 2;
        }
    }
}

// @recomp Modified to clean up extension data for de-allocated display list objects
RECOMP_PATCH void terminateTasksByType(s32 taskType) {
    Node* i;

    for (i = gActiveScheduler->activeList; i != NULL; i = i->next) {
        if (i->unkC == (u8)taskType) {
            // @recomp Free any extension data for display list objects at this address
            deleteTaskNodeDisplayListObjectExtensions(i);
            if ((u32)(i->unkE - 3) < 2) {
                i->unkE = 4;
            } else {
                i->unkE = 2;
            }
        }
    }
}

// @recomp Modified to clean up extension data for de-allocated display list objects
RECOMP_PATCH void terminateTasksByTypeAndID(s32 taskType, s32 taskID) {
    Node* i;

    for (i = gActiveScheduler->activeList; i != NULL; i = i->next) {
        if (i->unkC == (u8)taskType && i->field_D == (u8)taskID) {
            // @recomp Free any extension data for display list objects at this address
            deleteTaskNodeDisplayListObjectExtensions(i);
            if ((u32)(i->unkE - 3) < 2) {
                i->unkE = 4;
            } else {
                i->unkE = 2;
            }
        }
    }
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
            pushMultiPartMatrixGroup(displayObjects, i);

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
                pushMultiPartMatrixGroup(displayObjects, i);

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
                pushMultiPartMatrixGroup(displayObjects, i);

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
                pushMultiPartMatrixGroup(displayObjects, i);

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
                pushMultiPartMatrixGroup(displayObjects, i);

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
                pushMultiPartMatrixGroup(displayObjects, i);

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
