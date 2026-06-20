/*
 * RT64 can stretch the race camera to widescreen, but the original HUD is drawn
 * with fixed 4:3 screen coordinates. Without explicit rect/scissor alignment,
 * corner HUD elements stay inset from the original 320x240 viewport instead of
 * tracking the widened screen edges. These hooks wrap each corner HUD draw with
 * temporary extended alignment state, then restore the normal text clip/scissor
 * so unrelated UI continues to use the game's original coordinates.
 */
 
#include "patches.h"
#include "audio/audio.h"

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

#define SECONDS_TO_TICKS(s) ((s) * 30)

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

static SpriteRenderArg* copy_sprite_arg_with_x_offset(SpriteRenderArg* src, s16 xOffset) {
    SpriteRenderArg* dst = (SpriteRenderArg*) advanceLinearAlloc(sizeof(SpriteRenderArg));

    if (dst != NULL) {
        *dst = *src;
        dst->x += xOffset;
    }

    return dst;
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

static void setTopLeftHudAlign(void* unused) {
    if (hud_widescreen_margin() <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, -HUD_CORNER_ALIGN_OFFSET, -HUD_CORNER_ALIGN_OFFSET);
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

static void setShotCrossTopRightHudAlign(void* unused) {
    if (hud_widescreen_margin() <= 0) {
        return;
    }

    set_hud_widescreen_align(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT,
                             -((s16) HUD_SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET) - 24, -HUD_CORNER_ALIGN_OFFSET);
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

RECOMP_PATCH void updateSpeedCrossFinishPositionDisplay(FinishPositionDisplayState* state) {
    state->spriteIndex = state->player->finishPosition;

    // @recomp wrap the Speed Cross finish-position icon with top-left widescreen alignment.
    enqueueCallbackBySlotIndex(8, 6, resetCornerHudAlign, NULL);

    enqueueCallbackBySlotIndex(8, 6, renderSpriteFrame, state);

    // @recomp wrap the Speed Cross finish-position icon with top-left widescreen alignment.
    enqueueCallbackBySlotIndex(8, 6, setTopLeftHudAlign, NULL);
}

RECOMP_PATCH void updateShotCrossScoreDisplay(ShotCrossScoreDisplayState* state) {
    char buf[16];

    // @recomp wrap the Shoot Cross ammo/newspaper HUD with top-left widescreen alignment.
    enqueueCallbackBySlotIndex(8, 0, resetCornerHudAlign, NULL);

    _Sprintf(buf, sIntegerFormat, state->player->primaryItemAmmo);
    drawNumericString(buf, -0x70, -0x54, 0xFF, state->digitAsset, state->player->playerIndex + 8, 0);
    enqueueCallbackBySlotIndex(8, 0, renderSpriteFrame, &state->ammoPanel);
    enqueueCallbackBySlotIndex(8, 0, renderSpriteFrame, &state->ammoIcon);

    // @recomp wrap the Shoot Cross ammo/newspaper HUD with top-left widescreen alignment.
    enqueueCallbackBySlotIndex(8, 0, setTopLeftHudAlign, NULL);
}

RECOMP_PATCH void updateShotCrossItemCountDisplay(CrossHudCounterDisplayState* state) {
    char buffer[0x10];
    GameState* allocation;
    s16 countX;
    SpriteRenderArg* iconArg;

    allocation = getCurrentAllocation();

    if (state->cachedValue != allocation->shootCrossTargetsHit) {
        state->flashCounter = 9;
        state->cachedValue = allocation->shootCrossTargetsHit;
    }

    if (state->flashCounter & 1) {
        _Sprintf(buffer, sTwoDigitFormat, allocation->shootCrossTargetsHit);
    } else {
        _Sprintf(buffer, sTwoDigitHighlightFormat, allocation->shootCrossTargetsHit);
    }

    if (state->flashCounter != 0) {
        state->flashCounter--;
    }

    if (state->layoutMode == 0) {
        // @recomp active Shoot Cross counter/icon render together in layer 0 so widescreen state is shared.
        enqueueCallbackBySlotIndex(8, 0, resetCornerHudAlign, NULL);

        // @recomp tune the hit-count digits under the widened letterbox icon.
        countX = state->sprite.x + 0x18;
        drawNumericString(buffer, countX, state->sprite.y + 0x10, 0xFF, state->digitAsset, 8, 0);

        // @recomp tune only the icon relative to the hit count.
        iconArg = copy_sprite_arg_with_x_offset(&state->sprite, 8);
        enqueueCallbackBySlotIndex(8, 0, renderSpriteFrame, iconArg);
        enqueueCallbackBySlotIndex(8, 0, setShotCrossTopRightHudAlign, NULL);
    } else {
        // @recomp wrap the result-layout icon with top-left widescreen alignment.
        enqueueCallbackBySlotIndex(8, 0, resetCornerHudAlign, NULL);
        enqueueCallbackBySlotIndex(8, 0, renderSpriteFrame, &state->sprite);
        enqueueCallbackBySlotIndex(8, 0, setTopLeftHudAlign, NULL);

        // @recomp wrap the result-layout count with matching top-left widescreen alignment.
        enqueueCallbackBySlotIndex(8, 1, resetCornerHudAlign, NULL);
        countX = state->sprite.x + 0x10;
        drawNumericString(buffer, countX, state->sprite.y + 0x10, 0xFF, state->digitAsset, 8, 1);
        enqueueCallbackBySlotIndex(8, 1, setTopLeftHudAlign, NULL);
    }
}

RECOMP_PATCH void updateShotCrossCountdownTimer(ShotCrossCountdownTimerUpdateState* state) {
    char buffer[16];
    Allocation* allocation;
    s32 timeValue;
    s32 minutes;
    s32 seconds;
    s32 remainingTicks;
    s32 temp;

    allocation = getCurrentAllocation();

    if (allocation->activeRaceEffectCount == 0 && allocation->raceUpdatePaused == 0) {
        PlayerInfo* player = allocation->timeRemaining;
        if ((player->animFlags & 0x80000) == 0) {
            if (state->timeRemaining != 0) {
                state->timeRemaining--;
                if (state->timeRemaining == 0) {
                    allocation->timerExpired = 1;
                }
            }
        }
    }

    timeValue = state->timeRemaining;
    minutes = timeValue / 1800;
    temp = timeValue - minutes * 1800;
    seconds = temp / 30;
    temp = temp - seconds * 30;
    remainingTicks = temp * 100 / 30;

    if (state->timeRemaining < SECONDS_TO_TICKS(30)) {
        _Sprintf(buffer, sTimerFormatLow, minutes, seconds, remainingTicks);
    } else {
        _Sprintf(buffer, sTimerFormatNormal, minutes, seconds, remainingTicks);
    }

    // @recomp wrap the Shoot Cross countdown timer with bottom-right widescreen alignment.
    enqueueCallbackBySlotIndex(8, 0, resetCornerHudAlign, NULL);

    enqueueCallbackBySlotIndex(8, 0, renderSpriteFrame, state);
    drawNumericString(buffer, 0x48, 0x50, 0xFF, state->digitAsset, 8, 0);

    // @recomp wrap the Shoot Cross countdown timer with bottom-right widescreen alignment.
    enqueueCallbackBySlotIndex(8, 0, setBottomRightHudAlign, NULL);
}

RECOMP_PATCH void updateRaceTimerDisplay(RaceTimerState* state) {
    char sp20[0x10];
    Allocation* alloc;
    s32 minutes;
    s32 seconds;

    alloc = (Allocation*) getCurrentAllocation();

    if (alloc->activeRaceEffectCount != 0) {
        goto check_time_flag;
    }
    if (alloc->raceUpdatePaused != 0) {
        goto check_time_flag;
    }
    if (alloc->timeRemaining->animFlags & 0x80000) {
        goto set_7E;
    }
    if (state->elapsedTicks == 0x433C8) {
        goto check_time_flag;
    }
    state->elapsedTicks++;
    if (state->elapsedTicks != 0x433C8) {
        goto check_time_flag;
    }
    alloc->timerExpired = 1;
    playSoundEffectWithPriorityDefaultVolume(0x46, 6);

check_time_flag:
    if (!(alloc->timeRemaining->animFlags & 0x80000)) {
        goto after_7E;
    }
set_7E:
    if (state->elapsedTicks > 0x4309E) {
        goto after_7E;
    }
    alloc->raceTimerHoldFlag = 1;

after_7E:
    alloc->raceTimerElapsedTicks = state->elapsedTicks;

    minutes = state->elapsedTicks / 32400;
    seconds = (state->elapsedTicks % 32400) / 540;

    state->blinkCounter++;
    if (state->blinkCounter == 0x28) {
        state->blinkCounter = 0;
    }

    if (state->elapsedTicks > 0x431AB) {
        if (state->blinkCounter < 0x14) {
            _Sprintf(sp20, sSpeedCrossTimerBlinkColonFormat, minutes, seconds);
        } else {
            _Sprintf(sp20, sSpeedCrossTimerBlinkSpaceFormat, minutes, seconds);
        }
    } else {
        if (state->blinkCounter < 0x14) {
            _Sprintf(sp20, sSpeedCrossTimerNormalColonFormat, minutes, seconds);
        } else {
            _Sprintf(sp20, sSpeedCrossTimerNormalSpaceFormat, minutes, seconds);
        }
    }

    // @recomp wrap the Speed Cross timer with bottom-right widescreen alignment.
    enqueueCallbackBySlotIndex(8, 0, resetCornerHudAlign, NULL);

    enqueueCallbackBySlotIndex(8, 0, renderSpriteFrame, state);
    drawNumericString(sp20, 0x68, 0x50, 0xFF, state->digitAsset, 8, 0);

    // @recomp wrap the Speed Cross timer with bottom-right widescreen alignment.
    enqueueCallbackBySlotIndex(8, 0, setBottomRightHudAlign, NULL);
}

RECOMP_PATCH void updateSkillGameResultTimerDisplay(ShotCrossCountdownTimerState* state) {
    char timeString[16];
    SkillGameTimerAllocation* allocation;
    s32 time;
    s32 minutes;
    s32 seconds;
    s32 frames;
    s16 blinkCounter;
    const char* timeFormat;

    allocation = (SkillGameTimerAllocation*) getCurrentAllocation();
    time = allocation->elapsedTicks;
    minutes = time / 32400;
    seconds = (time % 32400) / 540;
    frames = ((time % 32400) % 540) / 9;

    blinkCounter = (u16) state->timeRemaining + 1;
    state->timeRemaining = blinkCounter;
    if (blinkCounter == 0x28) {
        state->timeRemaining = 0;
    }

    if (state->timeRemaining < 0x14) {
        timeFormat = sSkillGameResultTimerColonFormat;
    } else {
        timeFormat = sSkillGameResultTimerSpaceFormat;
    }
    _Sprintf(timeString, timeFormat, minutes, seconds, frames);

    // @recomp wrap the skill-game result timer with top-left widescreen alignment.
    enqueueCallbackBySlotIndex(8, 0, resetCornerHudAlign, NULL);

    enqueueCallbackBySlotIndex(8, 0, renderSpriteFrame, state);
    drawNumericString(timeString, -0x54, -0x28, 0xFF, state->digitAsset, 8, 0);

    // @recomp wrap the skill-game result timer with top-left widescreen alignment.
    enqueueCallbackBySlotIndex(8, 0, setTopLeftHudAlign, NULL);
}

RECOMP_PATCH void updateCrossRaceBadgeDisplay(CrossRaceBadgeState* state) {
    // @recomp wrap the cross-race badge with bottom-left widescreen alignment.
    enqueueCallbackBySlotIndex(8, 0, resetCornerHudAlign, NULL);
    enqueueCallbackBySlotIndex(8, 0, renderSpriteFrame, &state->bgX);
    enqueueCallbackBySlotIndex(8, 0, renderSpriteFrame, state);
    enqueueCallbackBySlotIndex(8, 0, setBottomLeftHudAlign, NULL);
}
