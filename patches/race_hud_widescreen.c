/*
 * RT64 can stretch the race camera to widescreen, but the original HUD is drawn
 * with fixed 4:3 screen coordinates. Without explicit rect/scissor alignment,
 * corner HUD elements stay inset from the original 320x240 viewport instead of
 * tracking the widened screen edges. These hooks wrap each corner HUD draw with
 * temporary extended alignment state, then restore the normal text clip/scissor
 * so unrelated UI continues to use the game's original coordinates.
 */
 
#include "patches.h"

extern float recomp_get_target_aspect_ratio(float original);
extern const char sGoldFormatShort[];
extern const char sGoldFormatLong[];

#define ORIGINAL_ASPECT (4.0f / 3.0f)
#define HUD_ASPECT_LIMIT (16.0f / 9.0f)
#define HUD_SCREEN_WIDTH 320.0f
#define SCREEN_HEIGHT 240
#define HUD_CORNER_BASE_INSET 16
#define HUD_CORNER_INSET 24
#define HUD_CORNER_ALIGN_OFFSET ((HUD_CORNER_BASE_INSET * 2) - HUD_CORNER_INSET)

extern Gfx* gDisplayListAllocPtr;
extern TextClipAndOffsetData gTextClipAndOffsetData;

static s16 hud_widescreen_margin(void) {
    float aspect = recomp_get_target_aspect_ratio(ORIGINAL_ASPECT);
    float extraWidth;

    if (aspect > HUD_ASPECT_LIMIT) {
        aspect = HUD_ASPECT_LIMIT;
    }

    extraWidth = (HUD_SCREEN_WIDTH * (aspect / ORIGINAL_ASPECT)) - HUD_SCREEN_WIDTH;
    if (extraWidth <= 0.0f) {
        return 0;
    }

    return (s16) (extraWidth * 0.5f);
}

static void set_hud_widescreen_align(u16 leftOrigin, u16 rightOrigin, s16 xOffset, s16 yOffset) {
    Gfx* gfx = gDisplayListAllocPtr;
    
    gEXSetScissorAlign(gfx++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, -((s32) HUD_SCREEN_WIDTH), 0, 0, 0,
                       (s32) HUD_SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, (s32) HUD_SCREEN_WIDTH, SCREEN_HEIGHT);
    gEXSetRectAlign(gfx++, leftOrigin, rightOrigin, xOffset * 4, yOffset * 4, xOffset * 4, yOffset * 4);
    gDisplayListAllocPtr = gfx;
}

static void reset_hud_widescreen_align(void) {
    Gfx* gfx = gDisplayListAllocPtr;

    gEXSetRectAlign(gfx++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    gEXSetScissorAlign(gfx++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0, 0, 0, (s32) HUD_SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, gTextClipAndOffsetData.clipLeft, gTextClipAndOffsetData.clipTop,
                  gTextClipAndOffsetData.clipRight, gTextClipAndOffsetData.clipBottom);
    gDisplayListAllocPtr = gfx;
}

static void set_lap_counter_widescreen_align(void) {
    s16 margin = hud_widescreen_margin();

    if (margin <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, -HUD_CORNER_ALIGN_OFFSET, -HUD_CORNER_ALIGN_OFFSET);
}

static void setBottomLeftHudAlign(void* unused) {
    if (hud_widescreen_margin() <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, -HUD_CORNER_ALIGN_OFFSET, HUD_CORNER_ALIGN_OFFSET);
}

static void setBottomRightHudAlign(void* unused) {
    if (hud_widescreen_margin() <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT, -((s16) HUD_SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET),
                             HUD_CORNER_ALIGN_OFFSET);
}

static void setTopRightHudAlign(void* unused) {
    if (hud_widescreen_margin() <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT, -((s16) HUD_SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET),
                             -HUD_CORNER_ALIGN_OFFSET);
}

static void resetCornerHudAlign(void* unused) {
    if (hud_widescreen_margin() <= 0) {
        return;
    }

    reset_hud_widescreen_align();
}

static void renderLapCounterSpriteFrame(void* arg) {
    set_lap_counter_widescreen_align();
    renderSpriteFrame(arg);
    reset_hud_widescreen_align();
}

static void renderLapCounterSpriteFrameWithPalette(void* arg) {
    set_lap_counter_widescreen_align();
    renderSpriteFrameWithPalette(arg);
    reset_hud_widescreen_align();
}

// @recomp wrap calls to renderSpriteFrame(WithPalette) to adjust for widescreen
RECOMP_PATCH void updatePlayerLapCounterSinglePlayer(LapCounterSinglePlayerState* state) {
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderLapCounterSpriteFrame, state);
    state->currentLap = state->player->currentLap + 1;
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderLapCounterSpriteFrameWithPalette, &state->digitX1);
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderLapCounterSpriteFrame, &state->digitX2);
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderLapCounterSpriteFrameWithPalette, &state->digitX3);
}

// @recomp wrap call to renderSpriteFrame to adjust for widescreen
RECOMP_PATCH void updatePlayerFinishPositionDisplay(FinishPositionDisplayState* state) {
    state->spriteIndex = state->player->finishPosition;

    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, resetCornerHudAlign, NULL);
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderSpriteFrame, state);
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, setBottomLeftHudAlign, NULL);
}

RECOMP_PATCH void updatePlayerGoldDisplaySinglePlayer(PlayerGoldDisplayState* state) {
    s32 gold = state->player->raceCoins;

    if (gold < 100) {
        _Sprintf(state->goldTextBuffer, sGoldFormatShort, gold);
    } else {
        _Sprintf(state->goldTextBuffer, sGoldFormatLong, gold);
    }

    // @recomp wrap texture rendering call to adjust for widescreen
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, resetCornerHudAlign, NULL);

    drawNumericString(state->goldTextBuffer, state->x, state->y, 0xFF, state->digitsTexture, (u16) (state->playerIndex + 8), 0);

    state->animCounter++;
    if ((s16) state->animCounter >= 12) {
        state->animCounter = 0;
    }

    state->animFrame = (s16) state->animCounter >> 1;

    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderSpriteFrame, &state->iconX);

    // @recomp wrap texture rendering call to adjust for widescreen
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, setBottomRightHudAlign, NULL);
}

RECOMP_PATCH void updatePlayerRaceProgressIndicator(RaceProgressIndicatorState* state) {
    RaceProgressIndicatorAllocation* allocation;
    s32 i;
    u8 playerIndex;
    Player* playerData;
    RaceProgressIndicatorElement* elem;
    s32 targetPosition;
    s32 delta;
    s8 flashState;
    s16 currentPosition;
    s32 playerCount;
    u8 pad[0x8];

    allocation = getCurrentAllocation();

    // @recomp wrap race progress indicator render call to adjust for widescreen
    enqueueCallbackBySlotIndex(0xC, 0, resetCornerHudAlign, NULL);

    playerCount = allocation->numPlayers;
    i = 0;
    if (playerCount > 0) {
        do {
            playerIndex = allocation->playerIndices[i];
            playerData = (Player*) ((u8*) allocation->players + playerIndex * 0xBE8);

            targetPosition = (0x2000 - playerData->raceProgress) * 0x8C;
            elem = &state->elements[playerIndex];

            if (targetPosition < 0) {
                targetPosition += 0x1FFF;
            }

            currentPosition = elem->positionOffset;
            delta = (targetPosition >> 13) - currentPosition;

            if (delta < -4) {
                delta = -4;
            }
            if (delta >= 5) {
                delta = 4;
            }

            elem->positionOffset = currentPosition + delta;
            flashState = elem->flashState;

            switch (flashState) {
                case 0:
                    if (playerData->behaviorFlags & 0x10) {
                        elem->flashState = flashState + 1;
                        case 1:
                            elem->flashCounter++;
                            if ((s8) elem->flashCounter == 2) {
                                elem->flashState = elem->flashState + 1;
                            }
                    }
                    break;
                case 2:
                    if (!(playerData->behaviorFlags & 0x10)) {
                        elem->flashState = flashState + 1;
                        case 3:
                            elem->flashCounter--;
                            if ((elem->flashCounter << 24) == 0) {
                                elem->flashState = 0;
                            }
                    }
                    break;
            }

            elem->y = (u16) elem->positionOffset + state->baseY - 4;
            elem->spriteFrame = (s8) elem->flashCounter;

            if (playerData->slowdownLevel != 0) {
                elem->hasActiveEffect = 1;
            } else {
                elem->hasActiveEffect = 0;
            }

            enqueueCallbackBySlotIndex(0xC, 0, renderSpriteFrameWithPalette, elem);
            i++;
            playerCount = allocation->numPlayers;
        } while (i < playerCount);
    }

    enqueueCallbackBySlotIndex(0xC, 0, renderSpriteFrame, state);

    // @recomp wrap race progress indicator render call to adjust for widescreen
    enqueueCallbackBySlotIndex(0xC, 0, setTopRightHudAlign, NULL);
}
