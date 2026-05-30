/*
 * Fix player-select portrait clipping seen in recomp and some cycle-accurate
 * emulators, but not on real hardware (?).
 */

#include "patches.h"
#include "ui/player_select_sprites.h"

typedef struct {
    u8 pad0[0x1E0];
    u16 unk1E0;
    u16 unk1E2;
    u8 pad1[2];
    u8 unk1E6;
} PlayerSelectAllocationPatch;

extern u16 D_800B09A0_1DC080[];

RECOMP_PATCH void updatePlayerSelectAnim(PlayerSelectState* state) {
    PlayerSelectAllocationPatch* allocation;
    s32 i;
    volatile PlayerSelectSprite* vsprite;

    allocation = getCurrentAllocation();

    switch (state->unk2C) {
        case 0:
            for (i = 0; i < 2; i++) {
                state->sprites[i].y += 0x10;
            }
            if (state->sprites[0].y == -0x1C) {
                state->unk2C = 1;
            }
            break;

        case 1:
            if (getViewportFadeMode((ViewportNode*) allocation) == 0) {
                state->unk2C = 2;
            }
            break;

        case 2:
            state->unk2D++;
            if ((state->unk2D & 0xFF) == 3) {
                state->unk2C = 3;
            }
            break;

        case 3:
            if (allocation->unk1E2 != state->playerIndex) {
                state->playerIndex = allocation->unk1E2;
                state->animState = 0;
                state->animCounter = 0;
            } else {
                state->animCounter = (state->animCounter + 1) & 3;
                if (state->animCounter == 0) {
                    state->animState = (state->animState + 1) & 3;
                }
            }

            i = 0;
            if (allocation->unk1E2 != state->slotIndex) {
                s16 scaleConst = 0x4E0;
                s16 alphaConst = 0x80;
                s32 divConst = 0x8000;
                vsprite = (volatile PlayerSelectSprite*) state;
                do {
                    s16 scale;
                    u8 slotIdx;
                    s16 frame;
                    s32 yPos;

                    vsprite->scaleY = scaleConst;
                    scale = divConst / (s32) (u16) vsprite->scaleY;
                    vsprite->scaleX = scaleConst;
                    vsprite->alpha = alphaConst;
                    yPos = i * scale;
                    yPos -= 0xC;
                    vsprite->y = yPos - scale / 2;
                    slotIdx = state->slotIndex;
                    vsprite->x = (slotIdx << 6) - 0x60;
                    frame = i + 8;
                    i++;
                    frame += slotIdx * 6;
                    vsprite->frameIndex = frame;
                    vsprite++;
                } while (i < 2);
            } else {
                s16 scaleXConst = 0x420;
                s16 scaleYConst = 0x400;
                s16 alphaConst = 0xFF;
                s32 divConst = 0x8000;
                u16* animTable = D_800B09A0_1DC080;
                vsprite = (volatile PlayerSelectSprite*) state;
                do {
                    s16 scale;
                    u8 slotIdx;
                    s32 frame;
                    s32 yPos;

                    vsprite->scaleY = scaleYConst;
                    scale = divConst / (s32) (u16) vsprite->scaleY;
                    vsprite->scaleX = scaleXConst;
                    vsprite->alpha = alphaConst;
                    yPos = i * scale;
                    yPos -= 0xC;
                    vsprite->y = yPos - scale / 2;
                    slotIdx = state->slotIndex;
                    vsprite->x = (slotIdx << 6) - 0x60;
                    frame = i + 8;
                    frame += slotIdx * 6;
                    vsprite->frameIndex = frame;
                    i++;
                    vsprite->frameIndex = frame + animTable[state->animState] * 2;
                    vsprite++;
                } while (i < 2);
            }

            if (allocation->unk1E6 == 1) {
                s32 slot;
                slot = state->slotIndex;
                state->unk2C = 4;
                if (slot == (allocation->unk1E2 & 0xFF)) {
                    PlayerSelectSprite* sprite;
                    s32 frameBase;
                    s32 offset;
                    sprite = &state->sprites[i];
                    frameBase = i + 8;
                    offset = slot * 6;
                    sprite->frameIndex = frameBase + offset;
                    state->animState = 0;
                    state->animCounter = 0;
                }
            }
            break;

        default:
            break;

        case 4:
            if (state->slotIndex == allocation->unk1E2) {
                i = 0;
                do {
                    if (allocation->unk1E0 & 1) {
                        state->sprites[i].unk13 = 0xFF;
                    } else {
                        state->sprites[i].unk13 = 0;
                    }
                    i++;
                } while (i < 2);
            }
            if (allocation->unk1E6 == 0) {
                state->unk2C = 3;
            }
            break;
    }

    i = 0;
    do {
        enqueueCallbackBySlotIndex(8, 0, renderScaledShadedSpriteFrame, &state->sprites[i]);
        i++;
    } while (i < 2);
}
