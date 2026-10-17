#include "patches.h"
#include "ui/character_select_gfx.h"

extern Gfx gSpriteRDPSetupDL[];
extern s16 gTileTextureFlipTable[];
extern float recomp_get_target_aspect_ratio(float original);
static TileMapScrollRenderState *sCharSelectBackdrop;

// @recomp Register the scrolling backdrop without changing other callers of the shared tile renderer.
RECOMP_PATCH void setupCharSelectBackgroundEffect(CharSelectTiledBackgroundState *state) {
    initScrollingTileMapState(&state->renderState, state->tileMapAsset);
    // @recomp Identify this task's tilemap until its cleanup so foreground tilemaps retain native clipping.
    sCharSelectBackdrop = &state->renderState;
    setCallback(updateCharSelectBackgroundEffect);
}

// @recomp Forget the backdrop before task memory can be reused by another screen.
RECOMP_PATCH void cleanupCharSelectBackgroundEffect(CharSelectTiledBackgroundState *state) {
    // @recomp Prevent a later tilemap allocation at this address from inheriting backdrop expansion.
    sCharSelectBackdrop = NULL;
    state->tileMapAsset = freeNodeMemory(state->tileMapAsset);
}

// @recomp Expand only the registered character/board-select backdrop in this shared renderer.
RECOMP_PATCH void renderTiledTextureMap(TileMapScrollRenderState *state) {
    s16 xStart;
    s16 yStart;
    s16 clipLeft;
    s16 clipTop;
    s16 clipRight;
    s16 clipBottom;
    u16 colOffset;
    u16 rowOffset;
    s16 numCols;
    s16 numRows;
    s16 col;
    s16 row;
    u16 colStart;
    u16 rowStart;
    s16 screenLeft;
    s16 screenTop;
    s16 screenRight;
    s16 screenBottom;
    s16 texU;
    s16 texV;
    s16 textureIndex;
    s16 tileGridIndex;
    s16 lastTile;
    s16 colRem;
    s16 rowRem;
    s32 paletteIndex;
    u16 *tileIndexData;
    u8 *textureData;
    s16 flipX;
    s16 flipY;
    u16 tileMask;
    u16 tileShift;
    s32 padding1;
    s32 tileCount;
    Gfx *displayList;
    Gfx *displayListHead;
    TileMapEntry *tileEntries;
    PaletteChunk32 *paletteData;
    PaletteChunk32 *curPaletteData;

    tileIndexData = state->tileIndexData;
    textureData = state->textureData;
    tileEntries = state->tileEntries;
    paletteData = state->paletteData;
    curPaletteData = 0;
    tileCount = 0;
    lastTile = -1;
    clipLeft = state->clipX;
    xStart = state->x;

    if ((displayList = graphicsAlloc(0x1B30))) {
        displayListHead = displayList;

        if (clipLeft < gTextClipAndOffsetData.clipLeft) {
            clipLeft = gTextClipAndOffsetData.clipLeft;
            xStart = xStart + (clipLeft - state->clipX);
        }

        clipTop = state->clipY;
        yStart = state->y;

        if (clipTop < gTextClipAndOffsetData.clipTop) {
            clipTop = gTextClipAndOffsetData.clipTop;
            yStart = yStart + (clipTop - state->clipY);
        }

        clipRight = state->clipX + state->clipWidth;

        if (clipRight > gTextClipAndOffsetData.clipRight) {
            clipRight = gTextClipAndOffsetData.clipRight;
        }

        clipBottom = state->clipY + state->clipHeight;

        if (clipBottom > gTextClipAndOffsetData.clipBottom) {
            clipBottom = gTextClipAndOffsetData.clipBottom;
        }

        // @recomp Add proportional tile columns across the output, keeping the native scroll phase.
        if (state == sCharSelectBackdrop) {
            s32 halfWidth = (s32)(recomp_get_target_aspect_ratio(4.0f / 3.0f) * 120.0f + 0.999f);
            clipLeft = 160 - halfWidth;
            clipRight = 160 + halfWidth;
            clipTop = 0;
            clipBottom = 240;
            xStart = state->x + clipLeft - state->clipX;
            yStart = state->y - state->clipY;
        }

        tileMask = state->tileWidth - 1;

        if (state->tileWidth == 16) {
            tileShift = 4;
        } else {
            tileShift = 5;
        }

        colOffset = clipLeft - (xStart & tileMask);
        rowOffset = clipTop - (yStart & tileMask);

        numCols = (clipRight - clipLeft + (tileMask - 1)) >> tileShift;
        numRows = (clipBottom - clipTop + (tileMask - 1)) >> tileShift;

        if (xStart & tileMask) {
            ++numCols;
        }

        if (yStart & tileMask) {
            ++numRows;
        }

        rowStart = rowOffset;
        rowRem = (yStart >> tileShift) % state->tileGridHeight;
        if (rowRem < 0) {
            rowRem = rowRem + state->tileGridHeight;
        }

        gSPDisplayList(gDisplayListAllocPtr++, (u32)gSpriteRDPSetupDL);

        // @recomp Preserve tile proportions and open the scissor to both output edges for the backdrop.
        if (state == sCharSelectBackdrop) {
            gEXSetRectAspect(gDisplayListAllocPtr++, G_EX_ASPECT_ADJUST);
            gEXSetScissor(gDisplayListAllocPtr++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT,
                          0, 0, 0, 240);
        }

        for (row = 0; row < numRows; ++row) {
            colStart = colOffset;
            colRem = (xStart >> tileShift) % state->tileGridWidth;
            if (colRem < 0) {
                colRem = colRem + state->tileGridWidth;
            }

            for (col = 0; col < numCols; ++col) {
                textureIndex = colRem + rowRem * state->tilesPerRow;
                tileGridIndex = tileIndexData[textureIndex];
                paletteIndex = tileEntries[tileGridIndex].paletteIndex;
                textureIndex = tileEntries[tileGridIndex].textureIndex;

                if (tileGridIndex != 0) {
                    ++tileCount;
                    flipX = gTileTextureFlipTable[tileEntries[tileGridIndex].flipMode * 2];
                    flipY = gTileTextureFlipTable[tileEntries[tileGridIndex].flipMode * 2 + 1];

                    screenLeft = colStart;
                    screenTop = rowStart;
                    screenRight = colStart + state->tileWidth;
                    screenBottom = rowStart + state->tileHeight;
                    texU = 0;
                    texV = 0;

                    if (flipX == -1) {
                        texU = state->tileWidth - 1;
                    }

                    if (flipY == -1) {
                        texV = state->tileHeight - 1;
                    }

                    if (screenLeft < clipRight && screenTop < clipBottom && screenRight >= clipLeft &&
                        screenBottom >= clipTop) {
                        if (screenLeft < clipLeft) {
                            texU = clipLeft - screenLeft;
                            if (flipX == -1) {
                                texU = (state->tileWidth - 1) - texU;
                            }
                            screenLeft = clipLeft;
                        }

                        if (screenTop < clipTop) {
                            texV = clipTop - screenTop;
                            if (flipY == -1) {
                                texV = (state->tileHeight - 1) - texV;
                            }
                            screenTop = clipTop;
                        }

                        if (screenRight >= clipRight) {
                            // @recomp Include the final backdrop pixel at the exclusive rectangle endpoint.
                            screenRight = state == sCharSelectBackdrop ? clipRight : clipRight - 1;
                        }

                        if (screenBottom >= clipBottom) {
                            // @recomp Include the bottom backdrop pixel at the exclusive rectangle endpoint.
                            screenBottom = state == sCharSelectBackdrop ? clipBottom : clipBottom - 1;
                        }

                        if (state->ciMode == 0) {
                            if (&paletteData[paletteIndex] != curPaletteData) {
                                gDPLoadTLUT_pal16(gDisplayListAllocPtr++, 0, (s32)&paletteData[paletteIndex]);
                                curPaletteData = &paletteData[paletteIndex];
                            }

                            if ((textureIndex - 1) != lastTile) {
                                gDPLoadTextureTile_4b(
                                    gDisplayListAllocPtr++,
                                    /*timg*/ textureData +
                                        ((((textureIndex - 1) * state->tileWidth * state->tileHeight) / 4) * 2),
                                    G_IM_FMT_CI,
                                    /*width*/ (state->tileWidth),
                                    /*height*/ 0,
                                    /*uls*/ 0,
                                    /*ult*/ 0,
                                    /*lrs*/ (state->tileWidth - 1),
                                    /*lrt*/ (state->tileHeight - 1),
                                    /*pal*/ 0,
                                    /*cms*/ 2,
                                    /*cmt*/ 2,
                                    /*masks*/ 0,
                                    /*maskt*/ 0,
                                    /*shifts*/ 0,
                                    /*shiftt*/ 0
                                );

                                lastTile = textureIndex - 1;
                            }
                        } else {
                            if (&paletteData[paletteIndex] != curPaletteData) {
                                gDPLoadTLUT_pal256(gDisplayListAllocPtr++, (s32)&paletteData[paletteIndex]);

                                curPaletteData = &paletteData[paletteIndex];
                            }

                            if ((textureIndex - 1) != lastTile) {
                                gDPLoadTextureTile(
                                    gDisplayListAllocPtr++,
                                    /*timg*/
                                    (textureData +
                                     ((((textureIndex - 1) * state->tileWidth * state->tileHeight) / 2) * 2)),
                                    G_IM_FMT_CI,
                                    G_IM_SIZ_8b,
                                    /*width*/ (state->tileWidth),
                                    /*height*/ 0,
                                    /*uls*/ 0,
                                    /*ult*/ 0,
                                    /*lrs*/ (state->tileWidth - 1),
                                    /*lrt*/ (state->tileHeight - 1),
                                    /*pal*/ 0,
                                    /*cms*/ 2,
                                    /*cmt*/ 2,
                                    /*masks*/ 0,
                                    /*maskt*/ 0,
                                    /*shifts*/ 0,
                                    /*shiftt*/ 0
                                );
                                lastTile = textureIndex - 1;
                            }
                        }

                        // @recomp Signed rectangles cover the extra columns, one-pixel overlap prevents RT64 seams.
                        if (state == sCharSelectBackdrop) {
                            gEXTextureRectangle(
                                gDisplayListAllocPtr++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE,
                                screenLeft * 4, screenTop * 4, (screenRight + 1) * 4, screenBottom * 4,
                                0, texU << 5, texV << 5, flipX << 10, flipY << 10
                            );
                        } else {
                            gSPTextureRectangle(
                                gDisplayListAllocPtr++,
                                screenLeft << 2,
                                screenTop << 2,
                                screenRight << 2,
                                screenBottom << 2,
                                0,
                                texU << 5,
                                texV << 5,
                                flipX << 10,
                                flipY << 10
                            );
                        }
                    }
                }

                colStart = colStart + state->tileWidth;
                colRem = (colRem + 1) % state->tileGridWidth;
            }

            rowStart = rowStart + state->tileHeight;
            rowRem = (rowRem + 1) % state->tileGridHeight;
        }

        // @recomp Restore rectangle scaling and the original viewport scissor for subsequent callbacks.
        if (state == sCharSelectBackdrop) {
            gEXSetRectAspect(gDisplayListAllocPtr++, G_EX_ASPECT_AUTO);
            gDPSetScissor(gDisplayListAllocPtr++, G_SC_NON_INTERLACE,
                          gTextClipAndOffsetData.clipLeft, gTextClipAndOffsetData.clipTop,
                          gTextClipAndOffsetData.clipRight, gTextClipAndOffsetData.clipBottom);
        }

        gSPEndDisplayList(displayList++);

        gSPDisplayList(gDisplayListAllocPtr++, (u32)displayListHead);
    }
}
