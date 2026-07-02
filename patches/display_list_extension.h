#pragma once

typedef struct DisplayListObjectExtension {
    u32 matrixGroupId;
    s32 skipInterpolation;
    s32 remainingInterpolationSkipPasses;
    u32 reserved;
} DisplayListObjectExtension;

DisplayListObjectExtension* getDisplayListObjectExtension(DisplayListObject* object);
DisplayListObjectExtension* getOrCreateDisplayListObjectExtension(DisplayListObject* object);
s32 isDisplayListObjectExtensionEmpty(DisplayListObjectExtension* extension);
s32 countBoardSelectObjectRenderPasses(DisplayListObject* object);
s32 consumeBoardSelectObjectInterpolationSkip(DisplayListObject* object);
void deleteDisplayListObjectExtension(DisplayListObject* object);
void deleteDisplayListObjectExtensionsInRange(void* start, u32 size);
void setBoardSelectObjectInterpolationSkip(void* object, s32 skip);
void setBoardSelectSceneModelInterpolationSkip(void* model, s32 skip);
void renderMultiPartOpaqueDisplayLists(DisplayListObject* displayObjects);
void renderMultiPartTransparentDisplayLists(DisplayListObject* displayObjects);
void renderMultiPartOverlayDisplayLists(DisplayListObject* displayObjects);
void renderMultiPartOpaqueDisplayListsWithLights(DisplayListObject* displayObjects);
void renderMultiPartTransparentDisplayListsWithLights(DisplayListObject* displayObjects);
void renderMultiPartOverlayDisplayListsWithLights(DisplayListObject* displayObjects);
