#include "patches.h"

extern float recomp_get_target_aspect_ratio(float original);

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

static void set_bottom_left_hud_align(void* unused) {
    if (hud_widescreen_margin() <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, -HUD_CORNER_ALIGN_OFFSET, HUD_CORNER_ALIGN_OFFSET);
}

static void set_top_right_hud_align(void* unused) {
    if (hud_widescreen_margin() <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT, -((s16) HUD_SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET),
                             -HUD_CORNER_ALIGN_OFFSET);
}

static void set_bottom_right_hud_align(void* unused) {
    if (hud_widescreen_margin() <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT, -((s16) HUD_SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET),
                             HUD_CORNER_ALIGN_OFFSET);
}

static void reset_corner_hud_align(void* unused) {
    if (hud_widescreen_margin() <= 0) {
        return;
    }

    reset_hud_widescreen_align();
}

static void render_lap_counter_sprite(void* arg) {
    set_lap_counter_widescreen_align();
    renderSpriteFrame(arg);
    reset_hud_widescreen_align();
}

static void render_lap_counter_sprite_with_palette(void* arg) {
    set_lap_counter_widescreen_align();
    renderSpriteFrameWithPalette(arg);
    reset_hud_widescreen_align();
}

static void format_gold_counter(char* buffer, s32 gold) {
    s32 divisor = 10000;
    s32 started = 0;
    s32 digit;
    s32 i;

    buffer[0] = (gold < 100) ? 1 : 2;

    for (i = 0; i < 5; i++) {
        digit = gold / divisor;
        gold -= digit * divisor;
        divisor /= 10;

        if ((digit != 0) || started || (i == 4)) {
            buffer[i + 1] = (char) ('0' + digit);
            started = 1;
        } else {
            buffer[i + 1] = ' ';
        }
    }

    buffer[6] = '\0';
}

RECOMP_PATCH void updatePlayerLapCounterSinglePlayer(LapCounterSinglePlayerState* state) {
    u16 callbackIndex = (u16) (state->playerIndex + 8);

    enqueueCallbackBySlotIndex(callbackIndex, 0, render_lap_counter_sprite, state);
    state->currentLap = state->player->currentLap + 1;
    enqueueCallbackBySlotIndex(callbackIndex, 0, render_lap_counter_sprite_with_palette, &state->digitX1);
    enqueueCallbackBySlotIndex(callbackIndex, 0, render_lap_counter_sprite, &state->digitX2);
    enqueueCallbackBySlotIndex(callbackIndex, 0, render_lap_counter_sprite_with_palette, &state->digitX3);
}

RECOMP_PATCH void updatePlayerFinishPositionDisplay(FinishPositionDisplayState* state) {
    u16 callbackIndex = (u16) (state->playerIndex + 8);

    state->spriteIndex = state->player->finishPosition;

    enqueueCallbackBySlotIndex(callbackIndex, 0, reset_corner_hud_align, NULL);
    enqueueCallbackBySlotIndex(callbackIndex, 0, renderSpriteFrame, state);
    enqueueCallbackBySlotIndex(callbackIndex, 0, set_bottom_left_hud_align, NULL);
}

RECOMP_PATCH void updatePlayerGoldDisplaySinglePlayer(PlayerGoldDisplayState* state) {
    s32 gold = state->player->raceCoins;
    u16 callbackIndex = (u16) (state->playerIndex + 8);

    format_gold_counter(state->goldTextBuffer, gold);

    enqueueCallbackBySlotIndex(callbackIndex, 0, reset_corner_hud_align, NULL);
    drawNumericString(state->goldTextBuffer, state->x, state->y, 0xFF, state->digitsTexture, callbackIndex, 0);

    state->animCounter++;
    if ((s16) state->animCounter >= 12) {
        state->animCounter = 0;
    }

    state->animFrame = (s16) state->animCounter >> 1;

    enqueueCallbackBySlotIndex(callbackIndex, 0, renderSpriteFrame, &state->iconX);
    enqueueCallbackBySlotIndex(callbackIndex, 0, set_bottom_right_hud_align, NULL);
}

RECOMP_PATCH void updatePlayerRaceProgressIndicator(RaceProgressIndicatorState* state) {
    RaceProgressIndicatorAllocation* allocation;
    s32 i;
    u8 playerIndex;
    Player* playerData;
    RaceProgressIndicatorElement* elem;
    s32 targetPosition;
    s32 delta;
    volatile s8 flashState;
    s16 currentPosition;
    s32 playerCount;

    allocation = getCurrentAllocation();
    enqueueCallbackBySlotIndex(0xC, 0, reset_corner_hud_align, NULL);

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

            if (flashState == 0) {
                if (playerData->behaviorFlags & 0x10) {
                    elem->flashState = 1;
                    flashState = 1;
                }
            }
            if (flashState == 1) {
                elem->flashCounter++;
                if ((s8) elem->flashCounter == 2) {
                    elem->flashState = 2;
                }
            } else if (flashState == 2) {
                if (!(playerData->behaviorFlags & 0x10)) {
                    elem->flashState = 3;
                    flashState = 3;
                }
            }
            if (flashState == 3) {
                elem->flashCounter--;
                if ((elem->flashCounter << 24) == 0) {
                    elem->flashState = 0;
                }
            }

            elem->y = (u16) elem->positionOffset + state->baseY - 4;
            elem->spriteFrame = (s8) elem->flashCounter;
            elem->hasActiveEffect = playerData->slowdownLevel != 0;

            enqueueCallbackBySlotIndex(0xC, 0, renderSpriteFrameWithPalette, elem);
            i++;
            playerCount = allocation->numPlayers;
        } while (i < playerCount);
    }

    enqueueCallbackBySlotIndex(0xC, 0, renderSpriteFrame, state);
    enqueueCallbackBySlotIndex(0xC, 0, set_top_right_hud_align, NULL);
}
