#include "patches.h"

#include "ui/character_select_ui.h"
#include "ui/level_preview_3d.h"

static u16 sLastCharPreviewMenuState[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
static u16 sLastBoardPreviewMenuState[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };

static s32 isBoardSelectSkipTransition(u16 oldState, u16 val) {
    if (val == CHAR_SELECT_BOARD_FLASH) {
        return TRUE;
    }

    if (val == CHAR_SELECT_CHAR_CONFIRMED) {
        return TRUE;
    }

    if ((oldState == CHAR_SELECT_CHAR_CONFIRMED) && (val == CHAR_SELECT_MENU_NAV)) {
        return TRUE;
    }

    if (((oldState == CHAR_SELECT_CHAR_ROW_BROWSE) || (oldState == CHAR_SELECT_BOARD_BROWSE)) &&
        (val == CHAR_SELECT_MENU_NAV)) {
        return TRUE;
    }

    return FALSE;
}

s32 countBoardSelectObjectRenderPasses(DisplayListObject* object) {
    s32 count = 0;

    if ((object != NULL) && (object->displayLists != NULL)) {
        if (object->displayLists->opaqueDisplayList != NULL) {
            count++;
        }
        if (object->displayLists->transparentDisplayList != NULL) {
            count++;
        }
        if (object->displayLists->overlayDisplayList != NULL) {
            count++;
        }
    }

    if (count == 0) {
        count = 1;
    }

    return count;
}

s32 consumeBoardSelectObjectInterpolationSkip(DisplayListObject* object) {
    DisplayListObjectExtension* extension;

    extension = getDisplayListObjectExtension(object);
    if ((extension != NULL) && extension->skipInterpolation) {
        if (extension->remainingInterpolationSkipPasses <= 0) {
            extension->remainingInterpolationSkipPasses = countBoardSelectObjectRenderPasses(object);
        }

        extension->remainingInterpolationSkipPasses--;
        if (extension->remainingInterpolationSkipPasses <= 0) {
            extension->skipInterpolation = FALSE;
            if (isDisplayListObjectExtensionEmpty(extension)) {
                deleteDisplayListObjectExtension(object);
            }
        }
        return TRUE;
    }

    return FALSE;
}

void setBoardSelectObjectInterpolationSkip(void* object, s32 skip) {
    DisplayListObjectExtension* extension;

    if (object == NULL) {
        return;
    }

    if (skip) {
        extension = getOrCreateDisplayListObjectExtension((DisplayListObject*)object);
        if (extension != NULL) {
            extension->skipInterpolation = TRUE;
            extension->remainingInterpolationSkipPasses = 0;
        }
    } else {
        extension = getDisplayListObjectExtension((DisplayListObject*)object);
        if (extension != NULL) {
            extension->skipInterpolation = FALSE;
            extension->remainingInterpolationSkipPasses = 0;
            if (isDisplayListObjectExtensionEmpty(extension)) {
                deleteDisplayListObjectExtension((DisplayListObject*)object);
            }
        }
    }
}

void setBoardSelectSceneModelInterpolationSkip(void* model, s32 skip) {
    SceneModel* sceneModel;
    DisplayListObject* displayObjects;
    DisplayListObject* animationDisplayObject;
    s32 i;

    if (model == NULL) {
        return;
    }

    sceneModel = (SceneModel*)model;
    displayObjects = (DisplayListObject*)sceneModel->boneDisplayObjects;
    animationDisplayObject = (DisplayListObject*)sceneModel->specialAnimationDisplayObject;

    if (displayObjects != NULL) {
        for (i = 0; i < SCENE_MODEL_BONE_SLOT_COUNT; i++) {
            if ((displayObjects[i].displayLists != NULL) && (sceneModel->partDisplayFlags & (1 << i))) {
                setBoardSelectObjectInterpolationSkip(&displayObjects[i], skip);
            }
        }
    }

    if (animationDisplayObject != NULL) {
        setBoardSelectObjectInterpolationSkip(animationDisplayObject, skip);
    }
}

RECOMP_PATCH void updateCharSelectPreviewModel(CharSelectPreviewModel *arg0) {
    Transform3D sp10;
    GameState *state;
    u8 prevSelState;
    u8 newSelState;
    u8 charIndex;
    u8 paletteIndex;
    u8 assetIndex;
    u16 rotation;
    u16 val;

    state = (GameState *)getCurrentAllocation();

    newSelState = state->iconDisplayState[arg0->playerIndex];
    prevSelState = arg0->selectionState;

    if (prevSelState != newSelState) {
        arg0->selectionState = newSelState;
        updateCharSelectPreviewLighting(arg0, arg0->playerIndex);
    }

    charIndex = state->charSelectCharRow[arg0->playerIndex];
    paletteIndex = state->charSelectCharCol[arg0->playerIndex];
    assetIndex = paletteIndex + charIndex * 3;

    memcpy(&sp10, &identityMatrix, sizeof(Transform3D));
    memcpy(&sp10.translation, &arg0->positionMatrix.translation.x, sizeof(Vec3i));

    if (state->charSelectCursorIndices[arg0->playerIndex] == state->charSelectMaxMenuOption - 1) {
        val = state->charSelectMenuStates[arg0->playerIndex];
        if (val != 1) {
            rotation = state->charSelectPreviewAngles[arg0->playerIndex];
            createYRotationMatrix(&arg0->positionMatrix, rotation);
            goto after_rotation;
        }
    }

    rotation = state->charSelectCarouselAngles[arg0->playerIndex];
    createYRotationMatrix(&arg0->positionMatrix, (0x2000 - rotation) & 0xFFFF);

after_rotation:
    composeTransform3D(&arg0->rotationMatrix, &arg0->positionMatrix, &sp10);
    composeTransform3D(&sp10, &state->charSelectRotations[arg0->playerIndex], (Transform3D *)arg0);

    val = state->charSelectMenuStates[arg0->playerIndex];
    
    // @recomp Track board-select menu transitions that teleport this preview model.
    if (sLastCharPreviewMenuState[arg0->playerIndex] != val) {
        u16 oldState = sLastCharPreviewMenuState[arg0->playerIndex];
        sLastCharPreviewMenuState[arg0->playerIndex] = val;

        if (isBoardSelectSkipTransition(oldState, val)) {
            setBoardSelectObjectInterpolationSkip(arg0, TRUE);
        }
    }

    if (val == 4 || val == 9) {
        arg0->animationAsset = freeNodeMemory(arg0->animationAsset);
        arg0->skeletonAsset = freeNodeMemory(arg0->skeletonAsset);
        arg0->paletteAsset = freeNodeMemory(arg0->paletteAsset);
        setCallback(initCharSelectSlidePosition);
    } else {
        if (assetIndex != arg0->charPaletteIndex) {
            arg0->charPaletteIndex = assetIndex;
            arg0->animationAsset = freeNodeMemory(arg0->animationAsset);
            arg0->skeletonAsset = freeNodeMemory(arg0->skeletonAsset);
            arg0->paletteAsset = freeNodeMemory(arg0->paletteAsset);
            setCallback(reloadCharSelectPreviewAssets);
        } else {
            enqueueDisplayListObjectWithLights(arg0->playerIndex, (DisplayListObject *)arg0);
        }
    }
}

RECOMP_PATCH void updateCharSelectBoardPreview(CharSelectBoardPreview *arg0) {
    Transform3D localMatrix;
    Transform3D *localPtr;
    GameState *state;
    Transform3D *transformPtr;
    u16 rotation;
    u16 val;

    state = (GameState *)getCurrentAllocation();

    localPtr = &localMatrix;
    memcpy(localPtr, &identityMatrix, sizeof(Transform3D));

    transformPtr = &arg0->transform;
    rotation = state->charSelectCarouselAngles[arg0->playerIndex];
    createYRotationMatrix(transformPtr, 0x2000 - rotation);

    composeTransform3D(transformPtr, &state->charSelectRotations[arg0->playerIndex], localPtr);

    applyTransformToModel(arg0->model, localPtr);

    clearModelRotation(arg0->model);
    updateModelGeometry(arg0->model);

    val = state->charSelectMenuStates[arg0->playerIndex];
    
    // @recomp Track board-select menu transitions that teleport this preview model.
    if (sLastBoardPreviewMenuState[arg0->playerIndex] != val) {
        u16 oldState = sLastBoardPreviewMenuState[arg0->playerIndex];
        sLastBoardPreviewMenuState[arg0->playerIndex] = val;

        if (isBoardSelectSkipTransition(oldState, val)) {
            setBoardSelectSceneModelInterpolationSkip(arg0->model, TRUE);
        }
    }

    if (val == 0x10) {
        destroySceneModel(arg0->model);
        setCallback(recreateCharSelectBoardModelForSlideIn);
    }
}

RECOMP_PATCH void cleanupCharSelectBoardModel(CharSelectBoardPreview *preview) {
    // @recomp Clear any pending skip tags for this model before it is destroyed.
    setBoardSelectSceneModelInterpolationSkip(preview->model, FALSE);
    destroySceneModel(preview->model);
}

RECOMP_PATCH void cleanupCharSelectPreviewAssets(CharSelectPreviewModel *arg0) {
    // @recomp Clear any pending skip tag for this display object before its assets are released.
    setBoardSelectObjectInterpolationSkip(arg0, FALSE);
    arg0->animationAsset = freeNodeMemory(arg0->animationAsset);
    arg0->skeletonAsset = freeNodeMemory(arg0->skeletonAsset);
    arg0->paletteAsset = freeNodeMemory(arg0->paletteAsset);
}

RECOMP_PATCH SceneModel *cleanupSceneModelHolder(SceneModel **arg0) {
    // @recomp Clear any pending skip tags for this model before it is destroyed.
    setBoardSelectSceneModelInterpolationSkip(*arg0, FALSE);
    return destroySceneModel(*arg0);
}
