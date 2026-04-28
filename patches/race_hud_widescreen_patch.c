#include "patches.h"

#define ORIGINAL_ASPECT (4.0f / 3.0f)
#define HUD_ASPECT_LIMIT (16.0f / 9.0f)
#define SCREEN_WIDTH 320.0f
#define SCREEN_HEIGHT 240
#define HUD_CORNER_BASE_INSET 16
#define HUD_CORNER_INSET 24
#define HUD_CORNER_ALIGN_OFFSET ((HUD_CORNER_BASE_INSET * 2) - HUD_CORNER_INSET)

typedef struct {
    u8 pad0[0xB6C];
    s32 raceCoins;
    s32 skillPoints;
    u8 padB74[0x50];
    u8 finishPosition;
    u8 currentLap;
} RaceHudPlayer;

typedef struct {
    s16 clipLeft;
    s16 clipTop;
    s16 clipRight;
    s16 clipBottom;
    s16 offsetX;
    s16 offsetY;
} HudClipAndOffsetData;

typedef struct {
    s16 x;
    s16 y;
    void* lapIconAsset;
    s16 spriteIndex;
    u8 padA[0x2];
    s16 digitX1;
    u8 padE[0x2];
    s16 digitY1;
    u8 pad12[0x2];
    s16 currentLap;
    u8 pad16[0x2];
    s16 digitX2;
    u8 pad1A[0xA];
    s16 digitX3;
    u8 pad26[0x1E];
    RaceHudPlayer* player;
    s32 playerIndex;
} LapCounterSinglePlayerState;

typedef struct {
    s16 x;
    s16 y;
    void* asset;
    s16 spriteIndex;
    u8 padA[0x2];
    RaceHudPlayer* player;
    s32 playerIndex;
} FinishPositionDisplayState;

typedef struct {
    void* digitsTexture;
    s16 x;
    s16 y;
    s16 iconX;
    s16 iconY;
    void* iconAsset;
    s16 animFrame;
    u8 pad12[0xE];
    char goldTextBuffer[8];
    RaceHudPlayer* player;
    u16 playerIndex;
    u16 animCounter;
} PlayerGoldDisplayState;

typedef struct {
    u8 pad0[0x10];
    void* players;
    u8 pad14[0x4A];
    u8 numPlayers;
    u8 pad5F[0x5];
    u8 playerIndices[4];
} RaceProgressIndicatorAllocation;

typedef struct {
    u8 pad0[0xB88];
    s32 playerStateFlags;
    u8 padB8C[0xC];
    s16 raceProgress;
    u8 padB9A[0x35];
    u8 activeEffectCount;
} RaceProgressPlayerData;

typedef struct {
    s16 x;
    s16 y;
    u8 pad4[0x4];
    s16 spriteFrame;
    u8 hasActiveEffect;
    u8 unkB;
    u8 flashCounter;
    s8 flashState;
    u8 padE[0x2];
    s16 positionOffset;
    u8 pad12[0x2];
} RaceProgressIndicatorElement;

typedef struct {
    u8 pad0[0x2];
    s16 baseY;
    u8 pad4[0x8];
    RaceProgressIndicatorElement elements[4];
} RaceProgressIndicatorState;

extern Gfx* gRegionAllocPtr;
extern HudClipAndOffsetData gTextClipAndOffsetData;

void debugEnqueueCallback(u16 index, u8 slotIndex, void* callback, void* callbackData);
void renderSpriteFrame(void* arg);
void renderSpriteFrameWithPalette(void* arg);
void drawNumericString(char* text, s16 x, s16 y, s16 z, s32 texture, u16 priority, u16 layer);
void* getCurrentAllocation(void);

static s16 hud_widescreen_margin(void) {
    float aspect = recomp_get_target_aspect_ratio(ORIGINAL_ASPECT);
    float extraWidth;

    if (aspect > HUD_ASPECT_LIMIT) {
        aspect = HUD_ASPECT_LIMIT;
    }

    extraWidth = (SCREEN_WIDTH * (aspect / ORIGINAL_ASPECT)) - SCREEN_WIDTH;
    if (extraWidth <= 0.0f) {
        return 0;
    }

    return (s16) (extraWidth * 0.5f);
}

static void set_hud_widescreen_align(u16 leftOrigin, u16 rightOrigin, s16 xOffset, s16 yOffset) {
    Gfx* gfx;

    gfx = gRegionAllocPtr;
    gEXSetScissorAlign(gfx++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, -((s32) SCREEN_WIDTH), 0, 0, 0,
                       (s32) SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, (s32) SCREEN_WIDTH, SCREEN_HEIGHT);
    gEXSetRectAlign(gfx++, leftOrigin, rightOrigin, xOffset * 4, yOffset * 4, xOffset * 4, yOffset * 4);
    gRegionAllocPtr = gfx;
}

static void reset_hud_widescreen_align(void) {
    Gfx* gfx = gRegionAllocPtr;

    gEXSetRectAlign(gfx++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    gEXSetScissorAlign(gfx++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0, 0, 0, (s32) SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, gTextClipAndOffsetData.clipLeft, gTextClipAndOffsetData.clipTop,
                  gTextClipAndOffsetData.clipRight, gTextClipAndOffsetData.clipBottom);
    gRegionAllocPtr = gfx;
}

static void set_lap_counter_widescreen_align(void) {
    s16 margin = hud_widescreen_margin();

    if (margin <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, -HUD_CORNER_ALIGN_OFFSET, -HUD_CORNER_ALIGN_OFFSET);
}

static void set_bottom_left_hud_align(void* unused) {
    (void) unused;

    if (hud_widescreen_margin() <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, -HUD_CORNER_ALIGN_OFFSET, HUD_CORNER_ALIGN_OFFSET);
}

static void set_top_right_hud_align(void* unused) {
    (void) unused;

    if (hud_widescreen_margin() <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT, -((s16) SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET),
                             -HUD_CORNER_ALIGN_OFFSET);
}

static void set_bottom_right_hud_align(void* unused) {
    (void) unused;

    if (hud_widescreen_margin() <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT, -((s16) SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET),
                             HUD_CORNER_ALIGN_OFFSET);
}

static void reset_corner_hud_align(void* unused) {
    (void) unused;

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

    debugEnqueueCallback(callbackIndex, 0, render_lap_counter_sprite, state);
    state->currentLap = state->player->currentLap + 1;
    debugEnqueueCallback(callbackIndex, 0, render_lap_counter_sprite_with_palette, &state->digitX1);
    debugEnqueueCallback(callbackIndex, 0, render_lap_counter_sprite, &state->digitX2);
    debugEnqueueCallback(callbackIndex, 0, render_lap_counter_sprite_with_palette, &state->digitX3);
}

RECOMP_PATCH void updatePlayerFinishPositionDisplay(FinishPositionDisplayState* state) {
    u16 callbackIndex = (u16) (state->playerIndex + 8);

    state->spriteIndex = state->player->finishPosition;

    debugEnqueueCallback(callbackIndex, 0, reset_corner_hud_align, NULL);
    debugEnqueueCallback(callbackIndex, 0, renderSpriteFrame, state);
    debugEnqueueCallback(callbackIndex, 0, set_bottom_left_hud_align, NULL);
}

RECOMP_PATCH void updatePlayerGoldDisplaySinglePlayer(PlayerGoldDisplayState* state) {
    s32 gold = state->player->raceCoins;
    u16 callbackIndex = (u16) (state->playerIndex + 8);

    format_gold_counter(state->goldTextBuffer, gold);

    debugEnqueueCallback(callbackIndex, 0, reset_corner_hud_align, NULL);
    drawNumericString(state->goldTextBuffer, state->x, state->y, 0xFF, (s32) state->digitsTexture, callbackIndex, 0);

    state->animCounter++;
    if ((s16) state->animCounter >= 12) {
        state->animCounter = 0;
    }

    state->animFrame = (s16) state->animCounter >> 1;

    debugEnqueueCallback(callbackIndex, 0, renderSpriteFrame, &state->iconX);
    debugEnqueueCallback(callbackIndex, 0, set_bottom_right_hud_align, NULL);
}

RECOMP_PATCH void updatePlayerRaceProgressIndicator(RaceProgressIndicatorState* state) {
    RaceProgressIndicatorAllocation* allocation;
    s32 i;
    u8 playerIndex;
    RaceProgressPlayerData* playerData;
    RaceProgressIndicatorElement* elem;
    s32 targetPosition;
    s32 delta;
    s8 flashState;
    s16 currentPosition;
    s32 playerCount;

    allocation = getCurrentAllocation();
    debugEnqueueCallback(0xC, 0, reset_corner_hud_align, NULL);

    playerCount = allocation->numPlayers;
    i = 0;
    if (playerCount > 0) {
        do {
            playerIndex = allocation->playerIndices[i];
            playerData = (RaceProgressPlayerData*) ((u8*) allocation->players + playerIndex * 0xBE8);

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
                if (playerData->playerStateFlags & 0x10) {
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
                if (!(playerData->playerStateFlags & 0x10)) {
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
            elem->hasActiveEffect = playerData->activeEffectCount != 0;

            debugEnqueueCallback(0xC, 0, renderSpriteFrameWithPalette, elem);
            i++;
            playerCount = allocation->numPlayers;
        } while (i < playerCount);
    }

    debugEnqueueCallback(0xC, 0, renderSpriteFrame, state);
    debugEnqueueCallback(0xC, 0, set_top_right_hud_align, NULL);
}
