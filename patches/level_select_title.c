#include "patches.h"

extern Gfx D_8009A780_9B380[];
// @recomp Replace the original renderer directly to filter and interpolate the level-select title.
RECOMP_PATCH void renderRotatedBillboardSprite(RotatedBillboardSprite *state) {
    if ((u32)((gActiveViewport->viewTransform.translation.x - state->transform.translation.x) + 0x0FEA0000) >
        0x1FD40000U) {
        return;
    }
    if ((u32)((gActiveViewport->viewTransform.translation.z - state->transform.translation.z) + 0x0FEA0000) >
        0x1FD40000U) {
        return;
    }
    if ((u32)((gActiveViewport->viewTransform.translation.y - state->transform.translation.y) + 0x0FEA0000) >
        0x1FD40000U) {
        return;
    }

    if (state->matrix == NULL) {
        state->matrix = graphicsAlloc(sizeof(Mtx));
        if (state->matrix == NULL) {
            return;
        }
        transform3DToMtx(&state->transform, state->matrix);
    }

    if (gGraphicsMode != GRAPHICS_MODE_TEXTURED_BILLBOARD) {
        gGraphicsMode = GRAPHICS_MODE_TEXTURED_BILLBOARD;

        gSPDisplayList(gDisplayListAllocPtr++, D_8009A780_9B380);

        gDPLoadTextureBlock_4b(
            gDisplayListAllocPtr++,
            state->textureData,
            G_IM_FMT_CI,
            state->textureWidth,
            state->textureHeight,
            0,
            G_TX_CLAMP,
            G_TX_CLAMP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );

        gDPLoadTLUT_pal16(gDisplayListAllocPtr++, 0, state->paletteData);

        gCachedSpriteTextureData = (u32)state->textureData;
        gCachedSpritePaletteData = (u32)state->paletteData;
    } else {
        if (gCachedSpriteTextureData != (u32)state->textureData) {
            gDPLoadTextureBlock_4b(
                gDisplayListAllocPtr++,
                state->textureData,
                G_IM_FMT_CI,
                state->textureWidth,
                state->textureHeight,
                0,
                G_TX_CLAMP,
                G_TX_CLAMP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );

            gCachedSpriteTextureData = (u32)state->textureData;
        }

        if (gCachedSpritePaletteData != (u32)state->paletteData) {
            gDPLoadTLUT_pal16(gDisplayListAllocPtr++, 0, state->paletteData);

            gCachedSpritePaletteData = (u32)state->paletteData;
        }
    }

    // @recomp Synchronize the pipeline before changing texture filtering.
    gDPPipeSync(gDisplayListAllocPtr++);
    // @recomp Save the original other mode so the filtering override stays local to this draw.
    gEXPushOtherMode(gDisplayListAllocPtr++);
    // @recomp Keep the flipping title as crisp as its 1:1 rectangle version with nearest filtering.
    gDPSetTextureFilter(gDisplayListAllocPtr++, G_TF_POINT);

    // @recomp Match each persistent front/back quad independently through the flip.
    gEXMatrixGroupDecomposedNormal(gDisplayListAllocPtr++, (u32)state, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    gSPMatrix(gDisplayListAllocPtr++, state->matrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(gDisplayListAllocPtr++, gLookAtPtr, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gDisplayListAllocPtr++, state->vertices, 4, 0);
    gSP2Triangles(gDisplayListAllocPtr++, 0, 3, 2, 0, 2, 1, 0, 0);
    // @recomp End the title quad's interpolation group after its triangles.
    gEXPopMatrixGroup(gDisplayListAllocPtr++, G_MTX_MODELVIEW);
    // @recomp Synchronize the pipeline before restoring the original texture filtering.
    gDPPipeSync(gDisplayListAllocPtr++);
    // @recomp Restore the original other mode for subsequent draws.
    gEXPopOtherMode(gDisplayListAllocPtr++);
}
