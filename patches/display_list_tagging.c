#include "patches.h"

extern Gfx* gDisplayListAllocPtr;
extern ActiveViewportState* gActiveViewport;

extern void prepareDisplayListRenderState(DisplayListObject*);
extern void setupDisplayListMatrix(DisplayListObject*);
extern void setupBillboardDisplayListMatrix(DisplayListObject*);
extern void initializeMultiPartDisplayListObjects(DisplayListObject* arg0);
extern void setupMultiPartObjectRenderState(DisplayListObject* arg0, s32 arg1);

// @recomp Functions for tagging matrix the display list objects with their corresponding matrix group.
// This can be expanded with additional parameters to control whether interpolation is skipped,
// decomposed interpolation is used, etc.
//
// TODO: This needs to be replaced with a better ID system that pulls the ID associated to the object
// and also combines it with the current viewport ID. Pointer alone shouldn't be relied on if possible.
// This extra structure could also store additional information to control interpolation in greater detail.

void pushObjectMatrixGroup(DisplayListObject* displayObjects) {
    gEXMatrixGroupSimple(gDisplayListAllocPtr++, (u32) (displayObjects), G_EX_PUSH, G_MTX_MODELVIEW,
                         G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP,
                         G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO, G_EX_ORDER_LINEAR, G_EX_EDIT_NONE,
                         G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO);
}

void pushMultiPartObjectMatrixGroup(DisplayListObject* displayObjects, s32 i) {
    gEXMatrixGroupSimple(gDisplayListAllocPtr++, (u32) (displayObjects) + i, G_EX_PUSH, G_MTX_MODELVIEW,
                         G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP,
                         G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO, G_EX_ORDER_LINEAR, G_EX_EDIT_NONE,
                         G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO);
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
            pushMultiPartObjectMatrixGroup(displayObjects, i);

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
                pushMultiPartObjectMatrixGroup(displayObjects, i);

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
                pushMultiPartObjectMatrixGroup(displayObjects, i);

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
                pushMultiPartObjectMatrixGroup(displayObjects, i);

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
                pushMultiPartObjectMatrixGroup(displayObjects, i);

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
                pushMultiPartObjectMatrixGroup(displayObjects, i);

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