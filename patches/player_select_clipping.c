#include "patches.h"

extern Gfx *gDisplayListAllocPtr;
extern Gfx gSpriteRDPSetupDL[];
extern s16 gTileTextureFlipTable[];
extern u16 gSpritePaletteModes[];
extern u16 gSpriteTextureFormats[];
extern SpriteFrameEntry *gCachedPaletteAddr;
extern s32 gCachedTextureAddr;
extern u16 gDefaultFontPalette[];

RECOMP_PATCH void renderScaledShadedSpriteFrame(ScaledSpriteArg *sprite) {
    s32 bottom;
    s16 scaleS;
    s16 scaleT;
    s32 left;
    s32 top;
    s32 right;
    s32 clipOffsetX;
    s32 clipOffsetY;
    SpriteFrameEntry *paletteBase;
    u16 paletteMode;
    u16 format;
    u16 paletteIndex;
    SpriteFrameEntry *frameEntry;
    s32 scaleW;
    s32 scaleH;
    s32 widthTimes4;
    s16 renderHeight;

    frameEntry = sprite->spriteData->frames;
    paletteBase = &frameEntry[sprite->spriteData->numFrames];
    frameEntry = &frameEntry[sprite->frameIndex];

    paletteMode = gSpritePaletteModes[frameEntry->paletteTableIndex];
    format = gSpriteTextureFormats[frameEntry->formatIndex];
    scaleS = gTileTextureFlipTable[sprite->tileMode * 2];
    scaleT = gTileTextureFlipTable[sprite->tileMode * 2 + 1];

    if (sprite->overridePaletteCount == 0) {
        paletteIndex = frameEntry->paletteIndex;
    } else {
        paletteIndex = sprite->overridePaletteCount - 1;
    }

    if (sprite->renderWidth > 0x7FFF || sprite->renderWidth == 0) {
        return;
    }
    if (sprite->renderHeight > 0x7FFF || sprite->renderHeight == 0) {
        return;
    }

    clipOffsetX = 0;

    sprite->tileMode = sprite->tileMode & 3;

    widthTimes4 = frameEntry->width << 2;
    scaleW = (frameEntry->width << 12) / sprite->renderWidth;
    scaleH = (frameEntry->height << 12) / sprite->renderHeight;

    left = (sprite->x * 4) - ((u32)scaleW >> 1) + (gTextClipAndOffsetData.offsetX * 4);
    top = (sprite->y * 4) - ((u32)scaleH >> 1) + (gTextClipAndOffsetData.offsetY * 4);
    renderHeight = sprite->renderHeight;
    if (renderHeight == 0x500) {
        // @recomp Align the scaled player-count portraits to whole pixels so RT64 does not skip the top texels.
        top &= ~3;
        // @recomp Keep sampling inside the 32px source frame so the top edge does not wrap onto the bottom.
        renderHeight = 0x4D8;
    }
    bottom = top + scaleH;
    right = left + scaleW;

    if (scaleS == -1) {
        clipOffsetX = widthTimes4 - 4;
    }

    if (left < gTextClipAndOffsetData.clipLeft * 4) {
        if (scaleS == -1) {
            clipOffsetX -= gTextClipAndOffsetData.clipLeft * 4 - left;
        } else {
            clipOffsetX = gTextClipAndOffsetData.clipLeft * 4 - left;
        }
        left = gTextClipAndOffsetData.clipLeft * 4;
    }

    clipOffsetY = 0;
    if (scaleT == -1) {
        clipOffsetY = frameEntry->height * 4 - 4;
    }

    if (top < gTextClipAndOffsetData.clipTop * 4) {
        if (scaleT == -1) {
            clipOffsetY -= gTextClipAndOffsetData.clipTop * 4 - top;
        } else {
            clipOffsetY = gTextClipAndOffsetData.clipTop * 4 - top;
        }
        top = gTextClipAndOffsetData.clipTop * 4;
    }

    if ((gTextClipAndOffsetData.clipRight * 4 >= left) && (!(gTextClipAndOffsetData.clipBottom * 4 < top)) &&
        (left < right) && (top < bottom)) {

        if (gGraphicsMode != 0x100) {
            gGraphicsMode = 0x100;
            gCachedPaletteAddr = NULL;
            gCachedTextureAddr = 0;
            gSPDisplayList(gDisplayListAllocPtr++, gSpriteRDPSetupDL);
        }

        {
            u32 combineCmd = 0xFC11E223;
            Gfx *gfx = gDisplayListAllocPtr;
            u32 combineArg = 0xFFC7FFFF;

            gDisplayListAllocPtr = (Gfx *)((s32)gfx + 8);
            __asm__ volatile("" : : "r"(gDisplayListAllocPtr) : "memory");
            gfx->words.w0 = 0xE7000000;
            gDisplayListAllocPtr = (Gfx *)((s32)gfx + 0x10);
            __asm__ volatile("" : : "r"(gDisplayListAllocPtr) : "memory");
            gDisplayListAllocPtr = (Gfx *)((s32)gfx + 0x18);

            gfx->words.w1 = 0;
            (gfx + 1)->words.w0 = combineCmd;
            (gfx + 1)->words.w1 = combineArg;
            (gfx + 2)->words.w0 = 0xFA000000;

            {
                u8 shade = sprite->shade;
                (gfx + 2)->words.w1 = (shade << 24) | (shade << 16) | (shade << 8) | 0xFF;
            }

            if (sprite->renderWidth != 0x400 || sprite->renderHeight != sprite->renderWidth) {
                gDisplayListAllocPtr = (Gfx *)((s32)gfx + 0x20);
                (gfx + 3)->words.w0 = 0xE200001C;
                (gfx + 3)->words.w1 = 0x0F0A7008;
            }
        }

        if ((s32)sprite->spriteData + frameEntry->textureOffset != gCachedTextureAddr) {
            gCachedTextureAddr = (s32)sprite->spriteData + frameEntry->textureOffset;
            loadSpriteTexture(
                (s32)sprite->spriteData + frameEntry->textureOffset,
                frameEntry->width,
                frameEntry->height,
                format,
                paletteMode
            );
        }

        {
            u32 palIdx = paletteIndex & 0xFFFF;
            if (palIdx == 0xFE) {
                if (gCachedPaletteAddr != (SpriteFrameEntry *)gDefaultFontPalette) {
                    gCachedPaletteAddr = (SpriteFrameEntry *)gDefaultFontPalette;
                    if (format == 0) {
                        gDPLoadTLUT_pal16(gDisplayListAllocPtr++, 0, gDefaultFontPalette);
                    } else {
                        gDPLoadTLUT_pal256(gDisplayListAllocPtr++, gDefaultFontPalette);
                    }
                }
            } else {
                SpriteFrameEntry *paletteAddr = &paletteBase[palIdx << 1];
                if (paletteAddr != gCachedPaletteAddr) {
                    gCachedPaletteAddr = paletteAddr;
                    if (format == 0) {
                        gDPLoadTLUT_pal16(gDisplayListAllocPtr++, 0, paletteAddr);
                    } else {
                        gDPLoadTLUT_pal256(gDisplayListAllocPtr++, paletteAddr);
                    }
                }
            }
        }

        gSPTextureRectangle(
            gDisplayListAllocPtr++,
            left,
            top,
            right,
            bottom,
            G_TX_RENDERTILE,
            clipOffsetX << 3,
            clipOffsetY << 3,
            (s16)scaleS * sprite->renderWidth,
            (s16)scaleT * renderHeight
        );

        gDPPipeSync(gDisplayListAllocPtr++);
        gDPSetCombineMode(gDisplayListAllocPtr++, G_CC_DECALRGBA, G_CC_DECALRGBA);

        if (sprite->renderWidth != 0x400 || sprite->renderHeight != sprite->renderWidth) {
            Gfx *_g2 = gDisplayListAllocPtr++;
            _g2->words.w0 = 0xE200001C;
            _g2->words.w1 = 0x503048;
        }
    }
}
