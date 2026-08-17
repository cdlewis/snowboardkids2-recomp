/*
 * RT64 can stretch the race camera to widescreen, but the original HUD is drawn
 * with fixed 4:3 screen coordinates. Without explicit rect/scissor alignment,
 * corner HUD elements stay inset from the original 320x240 viewport instead of
 * tracking the widened screen edges. These hooks wrap each corner HUD draw with
 * temporary extended alignment state, then restore the normal text clip/scissor
 * so unrelated UI continues to use the game's original coordinates.
 */

#include "patches.h"
#include "assets.h"
#include "audio/audio.h"
#include "graphics/clip_text_render.h"

#define ORIGINAL_ASPECT (4.0f / 3.0f)
#define HUD_SCREEN_WIDTH 320.0f
#define SCREEN_HEIGHT 240
#define HUD_CORNER_BASE_INSET 16
#define HUD_CORNER_INSET 24
#define HUD_CORNER_ALIGN_OFFSET ((HUD_CORNER_BASE_INSET * 2) - HUD_CORNER_INSET)
#define HUD_HORIZONTAL_2P_LAP_TOP_INSET HUD_CORNER_ALIGN_OFFSET
#define HUD_VERTICAL_2P_LAP_X -0x44
#define HUD_VERTICAL_2P_LAP_Y -0x68
#define HUD_VERTICAL_2P_FINISH_X -0x44
#define HUD_MULTIPLAYER_ITEM_RIGHT_INSET_FIXED 37
#define RACE_SPLIT_DIVIDER_WIDTH 2
#define ITEM_GROUP_COVERED_RIGHT_EDGE_FIXED 2
#define RACE_PROGRESS_COVERED_RIGHT_EDGE_FIXED 4
#define PLAYER_OVERLAY_VIEWPORT_BASE 8
#define FULL_SCREEN_RACE_VIEWPORT_SLOT 0xC
#define OVERLAY_CALLBACK_LAYER 6
#define PLAYER_OVERLAY_VIEWPORT_SLOT(playerIndex) ((u16) ((playerIndex) + PLAYER_OVERLAY_VIEWPORT_BASE))
#define SECONDS_TO_TICKS(s) ((s) * 30)

extern float recomp_get_target_aspect_ratio(float original);
extern float recomp_get_target_hud_aspect_ratio(float original);
extern s32 gRaceUsesVerticalTwoPlayerSplit;
extern const char sGoldFormatShort[];
extern const char sGoldFormatLong[];
extern const char D_8009E868_9F468[];
static const char sMultiplayerGoldFormat[] = "%5d";
extern Gfx* gDisplayListAllocPtr;
extern TextClipAndOffsetData gTextClipAndOffsetData;

typedef enum {
    RACE_HUD_LAYOUT_SINGLE_PLAYER,
    RACE_HUD_LAYOUT_TWO_PLAYER_HORIZONTAL,
    RACE_HUD_LAYOUT_TWO_PLAYER_VERTICAL,
    RACE_HUD_LAYOUT_MULTIPLAYER_GRID,
} RaceHudLayout;

typedef struct {
    s32 centerFixed;
    s32 rightFixed;
} PlayerItemGroupBounds;

static RaceHudLayout sRaceHudLayout = RACE_HUD_LAYOUT_SINGLE_PLAYER;
static PlayerItemGroupBounds sPlayerItemGroupBounds[4];

typedef struct {
    s16 x;
    s16 y;
    MemoryAllocatorNode* assetTable;
    s16 baseAssetIndex;
    s8 assetType;
    u8 paddingB;
    s32 paddingC;
    s32 frameCounter;
    u16 renderPriority;
    s16 halfSizeRender;
} FloatingItemSpriteTask;

static s16 hudWidescreenMargin(void) {
    float aspect = recomp_get_target_hud_aspect_ratio(ORIGINAL_ASPECT);
    float extraWidth;

    extraWidth = (HUD_SCREEN_WIDTH * (aspect / ORIGINAL_ASPECT)) - HUD_SCREEN_WIDTH;
    if (extraWidth <= 0.0f) {
        return 0;
    }

    return (s16) (extraWidth * 0.5f);
}

static s32 usesExpandedHudLayout(void) {
    return hudWidescreenMargin() > 0;
}

static s32 usesSplitScreenColumns(void) {
    return sRaceHudLayout == RACE_HUD_LAYOUT_TWO_PLAYER_VERTICAL ||
           sRaceHudLayout == RACE_HUD_LAYOUT_MULTIPLAYER_GRID;
}

static s32 usesAdjustedHudLayout(void) {
    // Split rows and columns both need explicit viewport ownership, even when the global HUD ratio is Original.
    return usesExpandedHudLayout() || sRaceHudLayout != RACE_HUD_LAYOUT_SINGLE_PLAYER;
}

static s32 roundFloatToS32(f32 value) {
    return (s32) (value + ((value >= 0.0f) ? 0.5f : -0.5f));
}

static f32 targetAspectScale(void) {
    return recomp_get_target_aspect_ratio(ORIGINAL_ASPECT) / ORIGINAL_ASPECT;
}

static f32 aspectAdjustedSourceX(f32 targetX) {
    return (HUD_SCREEN_WIDTH * 0.5f) + (targetX - HUD_SCREEN_WIDTH * 0.5f) * targetAspectScale();
}

static s32 aspectAdjustedOffsetFixed(f32 targetX, f32 authoredOriginX, s32 offsetFixed) {
    return roundFloatToS32((aspectAdjustedSourceX(targetX) - authoredOriginX) * 4.0f) + offsetFixed;
}

static f32 getSharedHudTargetX(u16 origin) {
    f32 targetAspect = recomp_get_target_aspect_ratio(ORIGINAL_ASPECT);
    f32 hudAspect = recomp_get_target_hud_aspect_ratio(ORIGINAL_ASPECT);
    f32 safeHalfWidth = (HUD_SCREEN_WIDTH * 0.5f) * (hudAspect / targetAspect);

    return (HUD_SCREEN_WIDTH * 0.5f) +
           (((f32) origin / G_EX_ORIGIN_CENTER) - 1.0f) * safeHalfWidth;
}

static u16 getHudOriginForTargetX(f32 targetX) {
    return (u16) roundFloatToS32(targetX * (G_EX_ORIGIN_RIGHT / HUD_SCREEN_WIDTH));
}

static s32 hudUsesFullOutputWidth(void) {
    return recomp_get_target_hud_aspect_ratio(ORIGINAL_ASPECT) >=
           recomp_get_target_aspect_ratio(ORIGINAL_ASPECT) - 0.001f;
}

static Gfx* setFullOutputHudScissor(Gfx* gfx) {
    gEXSetScissorAlign(gfx++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, -((s32) HUD_SCREEN_WIDTH), 0, 0, 0,
                       (s32) HUD_SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, (s32) HUD_SCREEN_WIDTH, SCREEN_HEIGHT);
    gEXSetRectAspect(gfx++, G_EX_ASPECT_ADJUST);
    return gfx;
}

static Gfx* setAspectAdjustedScissor(Gfx* gfx) {
    // explicit rectangle aspect adjustment preserves sprite proportions inside split framebuffer regions.
    gEXSetScissor(gfx++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, SCREEN_HEIGHT);
    gEXSetRectAspect(gfx++, G_EX_ASPECT_ADJUST);

    return gfx;
}

static void setSharedHudWidescreenAlign(u16 leftOrigin, u16 rightOrigin, s16 xOffset, s16 yOffset) {
    if (hudUsesFullOutputWidth()) {
        Gfx* gfx = setFullOutputHudScissor(gDisplayListAllocPtr);

        gEXSetRectAlign(gfx++, leftOrigin, rightOrigin, xOffset * 4, yOffset * 4, xOffset * 4, yOffset * 4);
        gDisplayListAllocPtr = gfx;
        return;
    }

    Gfx* gfx = setAspectAdjustedScissor(gDisplayListAllocPtr);
    u16 targetLeftOrigin = getHudOriginForTargetX(getSharedHudTargetX(leftOrigin));
    u16 targetRightOrigin = getHudOriginForTargetX(getSharedHudTargetX(rightOrigin));

    // extended origins own placement; explicit aspect adjustment independently preserves sprite scale. */
    gEXSetRectAlign(gfx++, targetLeftOrigin, targetRightOrigin, xOffset * 4, yOffset * 4, xOffset * 4,
                    yOffset * 4);
    gDisplayListAllocPtr = gfx;
}

static s32 getActiveViewportRightEdge(void) {
    s32 clipRightEdge = gTextClipAndOffsetData.clipRight;

    /*
     * The root and right-column clips end at pixel 319. Convert that inclusive last pixel to the logical edge at 320;
     * left-column clips already carry their shared edge at 160.
     */
    if (clipRightEdge == (s32) HUD_SCREEN_WIDTH - 1) {
        clipRightEdge++;
    }

    return clipRightEdge;
}

static f32 getActiveViewportCenterX(void) {
    return (gTextClipAndOffsetData.clipLeft + getActiveViewportRightEdge()) * 0.5f;
}

static void setSharedRaceProgressHudAlign(s32 authoredCenterFixed, s16 yOffset) {
    Gfx* gfx = setFullOutputHudScissor(gDisplayListAllocPtr);

    // The output centre is invariant under every HUD ratio; explicit aspect adjustment preserves tracker width.
    gEXSetRectAlign(gfx++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, -authoredCenterFixed, yOffset * 4,
                    -authoredCenterFixed, yOffset * 4);
    gDisplayListAllocPtr = gfx;
}

static void setSharedRaceProgressEdgeAlign(u16 origin, s32 xOffsetFixed) {
    Gfx* gfx = gDisplayListAllocPtr;

    if (hudUsesFullOutputWidth()) {
        gfx = setFullOutputHudScissor(gfx);
        gEXSetRectAlign(gfx++, origin, origin, xOffsetFixed, 0, xOffsetFixed, 0);
        gDisplayListAllocPtr = gfx;
        return;
    }

    gfx = setAspectAdjustedScissor(gfx);
    u16 targetOrigin = getHudOriginForTargetX(getSharedHudTargetX(origin));

    // The authored vertical tracker is already centred on the 240-line screen; edge alignment only changes its X.
    gEXSetRectAlign(gfx++, targetOrigin, targetOrigin, xOffsetFixed, 0, xOffsetFixed, 0);
    gDisplayListAllocPtr = gfx;
}

static s32 usesHorizontalSplit(void) {
    return sRaceHudLayout == RACE_HUD_LAYOUT_TWO_PLAYER_HORIZONTAL;
}

static s32 usesVerticalTwoPlayerSplit(void) {
    return sRaceHudLayout == RACE_HUD_LAYOUT_TWO_PLAYER_VERTICAL;
}

static void updateRaceHudLayoutMode(void) {
    GameState* gameState = getCurrentAllocation();

    if (gameState->playerCount >= 3) {
        sRaceHudLayout = RACE_HUD_LAYOUT_MULTIPLAYER_GRID;
    } else if (gameState->playerCount == 2) {
        sRaceHudLayout = gRaceUsesVerticalTwoPlayerSplit ? RACE_HUD_LAYOUT_TWO_PLAYER_VERTICAL
                                                        : RACE_HUD_LAYOUT_TWO_PLAYER_HORIZONTAL;
    } else {
        sRaceHudLayout = RACE_HUD_LAYOUT_SINGLE_PLAYER;
    }
}

static s32 isRightColumnPlayer(s32 playerIndex) {
    return usesVerticalTwoPlayerSplit() ? playerIndex == 1 : usesSplitScreenColumns() && playerIndex >= 2;
}

typedef enum {
    VIEWPORT_HUD_ANCHOR_LEFT,
    VIEWPORT_HUD_ANCHOR_PHYSICAL_LEFT,
    VIEWPORT_HUD_ANCHOR_CENTER,
    VIEWPORT_HUD_ANCHOR_RIGHT,
} ViewportHudAnchor;

typedef struct {
    f32 safeLeftX;
    f32 safeCenterX;
    f32 safeRightX;
    s16 logicalLeft;
    s16 logicalCenter;
    s16 logicalRight;
} ViewportHudBounds;

static ViewportHudBounds getViewportHudBounds(void) {
    float targetAspect = recomp_get_target_aspect_ratio(ORIGINAL_ASPECT);
    float hudAspect = recomp_get_target_hud_aspect_ratio(ORIGINAL_ASPECT);
    s32 logicalLeft = gTextClipAndOffsetData.clipLeft;
    s32 logicalRight = getActiveViewportRightEdge();

    // Right-column clips begin at the framebuffer centre, underneath the two-pixel vertical divider. Start their HUD
    // bounds after the divider's right half instead of counting the covered pixel as usable padding. Left-column right
    // anchors already resolve against the divider's visible outer edge through RT64's right-edge rectangle alignment.
    if (usesSplitScreenColumns() && logicalLeft >= (s32) HUD_SCREEN_WIDTH / 2) {
        logicalLeft += RACE_SPLIT_DIVIDER_WIDTH / 2;
    }

    float clipWidth = logicalRight - logicalLeft;
    float clipHeight = gTextClipAndOffsetData.clipBottom - gTextClipAndOffsetData.clipTop;
    // Horizontal rows retain the full screen width; only column layouts derive their safe width from clip height.
    float safeHeightFraction = usesHorizontalSplit() ? 1.0f : clipHeight / (float) SCREEN_HEIGHT;
    float viewportAspect =
        targetAspect * (clipWidth / HUD_SCREEN_WIDTH) / safeHeightFraction;
    float safeAspect = hudAspect < viewportAspect ? hudAspect : viewportAspect;
    float logicalCenter = (logicalLeft + logicalRight) * 0.5f;
    float safeHalfWidth =
        (HUD_SCREEN_WIDTH * 0.5f) * (safeAspect / targetAspect) * safeHeightFraction;
    ViewportHudBounds bounds;

    bounds.safeLeftX = logicalCenter - safeHalfWidth;
    bounds.safeCenterX = logicalCenter;
    bounds.safeRightX = logicalCenter + safeHalfWidth;
    bounds.logicalLeft = logicalLeft;
    bounds.logicalCenter = (s16) logicalCenter;
    bounds.logicalRight = logicalRight;
    return bounds;
}

static void setViewportHudRectAlign(f32 targetX, s32 xOffsetFixed, s32 yOffsetFixed) {
    u16 origin = getHudOriginForTargetX(targetX);

    if (hudUsesFullOutputWidth()) {
        s16 scissorHalfWidth = (s16) (120.0f * recomp_get_target_aspect_ratio(ORIGINAL_ASPECT));

        gEXSetScissorAlign(gDisplayListAllocPtr++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0, 0, 0,
                           (s32) HUD_SCREEN_WIDTH, SCREEN_HEIGHT);
        gEXSetScissor(gDisplayListAllocPtr++, G_SC_NON_INTERLACE, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER,
                      -scissorHalfWidth, 0, scissorHalfWidth, SCREEN_HEIGHT);
        gEXSetRectAlign(gDisplayListAllocPtr++, origin, origin, xOffsetFixed, yOffsetFixed, xOffsetFixed,
                        yOffsetFixed);
        gEXSetRectAspect(gDisplayListAllocPtr++, G_EX_ASPECT_ADJUST);
        return;
    }

    /*
     * map the authored viewport-local anchor into this viewport's HUD safe area. Explicit rectangle aspect
     * adjustment preserves sprite proportions even when the framebuffer pair represents one split-screen quadrant.
     */
    gDisplayListAllocPtr = setAspectAdjustedScissor(gDisplayListAllocPtr);
    gEXSetRectAlign(gDisplayListAllocPtr++, origin, origin, xOffsetFixed, yOffsetFixed, xOffsetFixed, yOffsetFixed);
}

static void setSplitViewportHudAlign(ViewportHudAnchor anchor, s16 xOffset, s16 yOffset) {
    ViewportHudBounds bounds = getViewportHudBounds();
    f32 targetX;
    s16 logicalAnchor;

    if (anchor == VIEWPORT_HUD_ANCHOR_PHYSICAL_LEFT) {
        targetX = bounds.logicalLeft;
        logicalAnchor = bounds.logicalLeft;
    } else if (anchor == VIEWPORT_HUD_ANCHOR_LEFT) {
        targetX = bounds.safeLeftX;
        logicalAnchor = bounds.logicalLeft;
    } else if (anchor == VIEWPORT_HUD_ANCHOR_RIGHT) {
        targetX = bounds.safeRightX;
        logicalAnchor = bounds.logicalRight;
    } else {
        targetX = bounds.safeCenterX;
        logicalAnchor = bounds.logicalCenter;
    }

    setViewportHudRectAlign(targetX, (-logicalAnchor + xOffset) * 4, yOffset * 4);
}

static void setViewportCenteredHudAlign(s32 authoredCenterFixed, s16 yOffset) {
    f32 centerX = getActiveViewportCenterX();

    if (hudUsesFullOutputWidth()) {
        s16 scissorHalfWidth = (s16) (120.0f * recomp_get_target_aspect_ratio(ORIGINAL_ASPECT));
        u16 centerOrigin = (u16) roundFloatToS32(centerX * (G_EX_ORIGIN_RIGHT / HUD_SCREEN_WIDTH));

        gEXSetScissorAlign(gDisplayListAllocPtr++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0, 0, 0,
                           (s32) HUD_SCREEN_WIDTH, SCREEN_HEIGHT);
        gEXSetScissor(gDisplayListAllocPtr++, G_SC_NON_INTERLACE, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER,
                      -scissorHalfWidth, 0, scissorHalfWidth, SCREEN_HEIGHT);
        gEXSetRectAlign(gDisplayListAllocPtr++, centerOrigin, centerOrigin, -authoredCenterFixed, yOffset * 4,
                        -authoredCenterFixed, yOffset * 4);
        gEXSetRectAspect(gDisplayListAllocPtr++, G_EX_ASPECT_ADJUST);
        return;
    }

    /*
     * centre the authored rectangle group in the active physical viewport. HUD safe-area ratios only affect
     * edge anchors; the viewport centre is invariant across Original, 16:9, and Expanded HUD layouts.
     */
    s32 adjustedOffset = aspectAdjustedOffsetFixed(centerX, 0.0f, -authoredCenterFixed);

    gDisplayListAllocPtr = setAspectAdjustedScissor(gDisplayListAllocPtr);
    gEXSetRectAlign(gDisplayListAllocPtr++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, adjustedOffset, yOffset * 4,
                    adjustedOffset, yOffset * 4);
}

static void setViewportTopRightItemHudAlign(s32 authoredRightFixed, s16 yOffset) {
    ViewportHudBounds bounds = getViewportHudBounds();
    f32 targetRight = bounds.safeRightX;
    s32 offsetFixed = -authoredRightFixed - HUD_MULTIPLAYER_ITEM_RIGHT_INSET_FIXED;

    if (usesVerticalTwoPlayerSplit()) {
        // the large vertical-2P item frame needs the same additional corner inset as its coin group.
        offsetFixed -= HUD_CORNER_ALIGN_OFFSET * 4;
    }

    // anchor each split-column item pair by its visible right edge so it follows the selected HUD safe area.
    setViewportHudRectAlign(targetRight, offsetFixed, yOffset * 4);
}

static void setSplitColumnPlayerLeftHudAlign(s16 yOffset) {
    setSplitViewportHudAlign(VIEWPORT_HUD_ANCHOR_LEFT, -HUD_CORNER_ALIGN_OFFSET, yOffset);
}

static void setSplitColumnPlayerRightHudAlign(s16 yOffset) {
    setSplitViewportHudAlign(VIEWPORT_HUD_ANCHOR_RIGHT, HUD_CORNER_ALIGN_OFFSET, yOffset);
}

static void setPlayerViewportBottomLeftHudAlign(SpriteRenderArg* sprite, s16 yOffset) {
    ViewportHudBounds bounds = getViewportHudBounds();
    f32 targetLeft = bounds.safeLeftX;
    s32 targetInset = usesHorizontalSplit() ? HUD_CORNER_INSET : HUD_CORNER_ALIGN_OFFSET;
    s32 authoredLeftFixed = (sprite->x + gTextClipAndOffsetData.offsetX) * 4;

    /*
     * map the rectangle's actual authored edge to the same per-viewport safe-area inset as the lap group.
     * Unlike the generic corner helper, this does not assume that every split-screen asset starts at the same
     * authored coordinate. The safe-area target is required for both horizontal rows and vertical columns so the
     * finish-position group follows the selected HUD mode instead of drifting to the physical viewport edge.
     */
    setViewportHudRectAlign(targetLeft, -authoredLeftFixed + targetInset * 4, yOffset * 4);
}

static void setPlayerViewportTopLeftHudAlign(LapCounterMultiplayerState* state) {
    SpriteRenderArg* icon = (SpriteRenderArg*) state;
    ViewportHudBounds bounds = getViewportHudBounds();
    f32 targetLeft = bounds.safeLeftX;
    s32 targetInset = usesHorizontalSplit() ? HUD_CORNER_INSET : HUD_CORNER_ALIGN_OFFSET;
    s32 authoredLeftFixed = (icon->x + gTextClipAndOffsetData.offsetX) * 4;
    s32 authoredTopFixed = (icon->y + gTextClipAndOffsetData.offsetY) * 4;
    s32 targetTopInset = HUD_HORIZONTAL_2P_LAP_TOP_INSET;

    if (usesSplitScreenColumns()) {
        /*
         * align the lap group's visible top edge with the held-item frames in vertical 2P, 3P, and 4P.
         * Their authored item Y differs by layout, while offsetY - clipTop describes the active viewport's local
         * origin. Reusing the item's -8 alignment translation keeps this relationship independent of output size.
         * Horizontal 2P intentionally retains its original compact lap inset.
         */
        s32 authoredItemY = usesVerticalTwoPlayerSplit() ? -0x60 : -0x30;
        targetTopInset = authoredItemY + gTextClipAndOffsetData.offsetY - gTextClipAndOffsetData.clipTop -
                         HUD_CORNER_ALIGN_OFFSET;
    }

    s32 targetTopFixed = (gTextClipAndOffsetData.clipTop + targetTopInset) * 4;

    /*
     * position the compact lap icon and text as one authored group. Deriving the translation from the active
     * clip and the group's actual coordinates avoids coupling viewport placement to layout-specific magic X values.
     * Target the selected HUD safe-area edge so the lap group shares the finish-position sprite's left anchor in both
     * two-player split layouts. Horizontal rows retain the authored 24-pixel corner inset, while vertical columns use
     * the same compact 8-pixel inset as their right-side HUD groups. Both groups' authored left coordinates are their
     * visible left anchors, so applying the same target inset keeps the visible lap and finish-position edges aligned.
     */
    setViewportHudRectAlign(targetLeft, -authoredLeftFixed + targetInset * 4,
                            -authoredTopFixed + targetTopFixed);
}

static void setPlayerTopLeftHudAlign(s32 playerIndex) {
    if (usesHorizontalSplit() && !usesExpandedHudLayout()) {
        setSplitColumnPlayerLeftHudAlign(-HUD_CORNER_ALIGN_OFFSET);
        return;
    }

    if (usesSplitScreenColumns()) {
        setSplitColumnPlayerLeftHudAlign(-HUD_CORNER_ALIGN_OFFSET);
        return;
    }

    if (isRightColumnPlayer(playerIndex)) {
        setSharedHudWidescreenAlign(G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER,
                                    -(HUD_SCREEN_WIDTH / 2) + HUD_CORNER_ALIGN_OFFSET, -HUD_CORNER_ALIGN_OFFSET);
    } else {
        setSharedHudWidescreenAlign(G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, -HUD_CORNER_ALIGN_OFFSET,
                                    -HUD_CORNER_ALIGN_OFFSET);
    }
}

static void setPlayerTopRightHudAlign(s32 playerIndex) {
    if (usesHorizontalSplit() && !usesExpandedHudLayout()) {
        setSplitColumnPlayerRightHudAlign(-HUD_CORNER_ALIGN_OFFSET);
        return;
    }

    if (usesSplitScreenColumns()) {
        setSplitColumnPlayerRightHudAlign(-HUD_CORNER_ALIGN_OFFSET);
        return;
    }

    if (isRightColumnPlayer(playerIndex)) {
        setSharedHudWidescreenAlign(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT,
                                    -((s16) HUD_SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET), -HUD_CORNER_ALIGN_OFFSET);
    } else {
        setSharedHudWidescreenAlign(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT,
                                    -((s16) HUD_SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET), -HUD_CORNER_ALIGN_OFFSET);
    }
}

static s32 getItemGroupCenterFixed(PlayerItemDisplayState* state) {
    SpriteRenderArg* primary = (SpriteRenderArg*) state;
    SpriteRenderArg* secondary = (SpriteRenderArg*) &state->secondaryItemX;
    SpriteFrameEntry* primaryFrame = &primary->spriteData->frames[primary->frameIndex];
    SpriteFrameEntry* secondaryFrame = &secondary->spriteData->frames[secondary->frameIndex];
    s32 left = primary->x < secondary->x ? primary->x : secondary->x;
    s32 primaryRight = primary->x + primaryFrame->width;
    s32 secondaryRight = secondary->x + secondaryFrame->width;
    s32 right = primaryRight > secondaryRight ? primaryRight : secondaryRight;

    /*
     * The sum of the exclusive frame bounds is twice the mathematical centre. RDP rectangle coverage leaves the
     * visible group half a pixel left of that point, so express the visible centre directly in 10.2 coordinates.
     */
    return (left + right) * 2 + gTextClipAndOffsetData.offsetX * 4 - ITEM_GROUP_COVERED_RIGHT_EDGE_FIXED;
}

static s32 getItemGroupRightFixed(PlayerItemDisplayState* state) {
    SpriteRenderArg* primary = (SpriteRenderArg*) state;
    SpriteRenderArg* secondary = (SpriteRenderArg*) &state->secondaryItemX;
    SpriteFrameEntry* primaryFrame = &primary->spriteData->frames[primary->frameIndex];
    SpriteFrameEntry* secondaryFrame = &secondary->spriteData->frames[secondary->frameIndex];
    s32 primaryRight = primary->x + primaryFrame->width;
    s32 secondaryRight = secondary->x + secondaryFrame->width;
    s32 right = primaryRight > secondaryRight ? primaryRight : secondaryRight;

    return (right + gTextClipAndOffsetData.offsetX) * 4 - ITEM_GROUP_COVERED_RIGHT_EDGE_FIXED;
}

static void setPlayerBottomRightHudAlign(s32 playerIndex) {
    if (usesHorizontalSplit() && !usesExpandedHudLayout()) {
        setSplitColumnPlayerRightHudAlign(HUD_CORNER_ALIGN_OFFSET);
        return;
    }

    if (usesSplitScreenColumns()) {
        setSplitColumnPlayerRightHudAlign(HUD_CORNER_ALIGN_OFFSET);
        return;
    }

    if (isRightColumnPlayer(playerIndex)) {
        setSharedHudWidescreenAlign(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT,
                                    -((s16) HUD_SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET), HUD_CORNER_ALIGN_OFFSET);
    } else {
        setSharedHudWidescreenAlign(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT,
                                    -((s16) HUD_SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET), HUD_CORNER_ALIGN_OFFSET);
    }
}

static void resetHudWidescreenAlign(void) {
    Gfx* gfx = gDisplayListAllocPtr;

    gEXSetRectAspect(gfx++, G_EX_ASPECT_AUTO);
    gEXSetRectAlign(gfx++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    gEXSetScissorAlign(gfx++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0, 0, 0, (s32) HUD_SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, gTextClipAndOffsetData.clipLeft, gTextClipAndOffsetData.clipTop,
                  gTextClipAndOffsetData.clipRight, gTextClipAndOffsetData.clipBottom);
    gDisplayListAllocPtr = gfx;
}

static void applyPlayerViewportOverlayRectState(void* unused) {
    f32 viewportCenterX = getActiveViewportCenterX();
    u16 viewportLeftOrigin = getHudOriginForTargetX(gTextClipAndOffsetData.clipLeft);
    u16 viewportRightOrigin = getHudOriginForTargetX(getActiveViewportRightEdge());
    u16 viewportOrigin = getHudOriginForTargetX(viewportCenterX);
    s32 viewportOffsetFixed = -roundFloatToS32(viewportCenterX * 4.0f);

    /*
     * keep the game's framebuffer coordinates unchanged while making RT64's aspect correction and clipping
     * local to the active player's overlay viewport.
     */
    gEXSetScissor(gDisplayListAllocPtr++, G_SC_NON_INTERLACE, viewportLeftOrigin, viewportRightOrigin, 0,
                  gTextClipAndOffsetData.clipTop, 0, gTextClipAndOffsetData.clipBottom);
    gEXSetRectAlign(gDisplayListAllocPtr++, viewportOrigin, viewportOrigin, viewportOffsetFixed, 0,
                    viewportOffsetFixed, 0);
    gEXSetRectAspect(gDisplayListAllocPtr++, G_EX_ASPECT_ADJUST);
}

static void applyFullScreenOverlayRectState(void* unused) {
    // preserve authored overlay proportions around the full output centre.
    gEXSetScissor(gDisplayListAllocPtr++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0,
                  SCREEN_HEIGHT);
    gEXSetRectAlign(gDisplayListAllocPtr++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, -(SCREEN_WIDTH / 2) * 4, 0,
                    -(SCREEN_WIDTH / 2) * 4, 0);
    gEXSetRectAspect(gDisplayListAllocPtr++, G_EX_ASPECT_ADJUST);
}

static void restoreViewportRectState(void* unused) {
    gEXSetRectAspect(gDisplayListAllocPtr++, G_EX_ASPECT_AUTO);
    gEXSetRectAlign(gDisplayListAllocPtr++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    gEXSetScissorAlign(gDisplayListAllocPtr++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0, 0, 0,
                       (s32) HUD_SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetScissor(gDisplayListAllocPtr++, G_SC_NON_INTERLACE, gTextClipAndOffsetData.clipLeft,
                  gTextClipAndOffsetData.clipTop, gTextClipAndOffsetData.clipRight,
                  gTextClipAndOffsetData.clipBottom);
}

static void beginPlayerViewportOverlay(s32 playerIndex) {
    // Callbacks execute LIFO. Enqueue restoration first so it runs after the player-local overlay.
    enqueueCallbackBySlotIndex(PLAYER_OVERLAY_VIEWPORT_SLOT(playerIndex), OVERLAY_CALLBACK_LAYER,
                               restoreViewportRectState, NULL);
}

static void endPlayerViewportOverlay(s32 playerIndex) {
    // Enqueue setup last so it runs before the player-local overlay.
    enqueueCallbackBySlotIndex(PLAYER_OVERLAY_VIEWPORT_SLOT(playerIndex), OVERLAY_CALLBACK_LAYER,
                               applyPlayerViewportOverlayRectState, NULL);
}

static void beginFullScreenOverlay(void) {
    // Callbacks execute LIFO. Enqueue restoration first so it runs after the full-screen overlay.
    enqueueCallbackBySlotIndex(FULL_SCREEN_RACE_VIEWPORT_SLOT, OVERLAY_CALLBACK_LAYER, restoreViewportRectState,
                               NULL);
}

static void endFullScreenOverlay(void) {
    // Enqueue setup last so it runs before the full-screen overlay.
    enqueueCallbackBySlotIndex(FULL_SCREEN_RACE_VIEWPORT_SLOT, OVERLAY_CALLBACK_LAYER,
                               applyFullScreenOverlayRectState, NULL);
}

static SpriteRenderArg* copySpriteArgWithXOffset(SpriteRenderArg* src, s16 xOffset) {
    SpriteRenderArg* dst = (SpriteRenderArg*) advanceLinearAlloc(sizeof(SpriteRenderArg));

    if (dst != NULL) {
        *dst = *src;
        dst->x += xOffset;
    }

    return dst;
}

static void setPlayerLapCounterMultiplayerEdgeAlign(void* arg) {
    LapCounterMultiplayerState* state = arg;

    if (!usesAdjustedHudLayout()) {
        return;
    }

    if (usesSplitScreenColumns() || usesHorizontalSplit()) {
        setPlayerViewportTopLeftHudAlign(state);
    } else if (!usesSplitScreenColumns()) {
        setSharedHudWidescreenAlign(G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, -HUD_CORNER_ALIGN_OFFSET,
                                    -HUD_CORNER_ALIGN_OFFSET);
    } else {
        setPlayerTopLeftHudAlign(state->playerIndex);
    }
}

static void setBottomLeftHudAlign(void* unused) {
    if (!usesExpandedHudLayout()) {
        return;
    }

    setSharedHudWidescreenAlign(G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, -HUD_CORNER_ALIGN_OFFSET,
                                HUD_CORNER_ALIGN_OFFSET);
}

static void setPlayerBottomLeftHudAlign(void* arg) {
    FinishPositionDisplayState* state = arg;

    if (!usesAdjustedHudLayout()) {
        return;
    }

    if (usesSplitScreenColumns() || usesHorizontalSplit()) {
        setPlayerViewportBottomLeftHudAlign((SpriteRenderArg*) state, HUD_CORNER_ALIGN_OFFSET);
        return;
    }

    if (isRightColumnPlayer(state->playerIndex)) {
        setSharedHudWidescreenAlign(G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER,
                                    -(HUD_SCREEN_WIDTH / 2) + HUD_CORNER_ALIGN_OFFSET, HUD_CORNER_ALIGN_OFFSET);
    } else {
        setSharedHudWidescreenAlign(G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, -HUD_CORNER_ALIGN_OFFSET,
                                    HUD_CORNER_ALIGN_OFFSET);
    }
}

static void setTopLeftHudAlign(void* unused) {
    if (!usesExpandedHudLayout()) {
        return;
    }

    setSharedHudWidescreenAlign(G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, -HUD_CORNER_ALIGN_OFFSET,
                                -HUD_CORNER_ALIGN_OFFSET);
}

static void setBottomRightHudAlign(void* unused) {
    if (!usesExpandedHudLayout()) {
        return;
    }

    setSharedHudWidescreenAlign(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT,
                                -((s16) HUD_SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET), HUD_CORNER_ALIGN_OFFSET);
}

static void setShotCrossTopRightHudAlign(void* unused) {
    if (!usesExpandedHudLayout()) {
        return;
    }

    setSharedHudWidescreenAlign(G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT,
                                -((s16) HUD_SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET) - 24,
                                -HUD_CORNER_ALIGN_OFFSET);
}

static void setRaceProgressHudAlign(void* arg) {
    SpriteRenderArg* tracker = arg;

    if (usesSplitScreenColumns()) {
        SpriteFrameEntry* frame = &tracker->spriteData->frames[tracker->frameIndex];
        s32 authoredCenterFixed =
            (tracker->x + gTextClipAndOffsetData.offsetX) * 4 + frame->width * 2 -
            RACE_PROGRESS_COVERED_RIGHT_EDGE_FIXED;

        /*
         * the N64 texture rectangle's covered lower-right edge places the tracker's visible one-pixel stroke
         * one authored pixel left of the frame-bounds centre. Map that visible centre to the split divider centre;
         * explicit rectangle aspect adjustment makes this covered-edge correction invariant across HUD ratios.
         */
        setSharedRaceProgressHudAlign(authoredCenterFixed, 0);
        return;
    }

    if (usesExpandedHudLayout()) {
        setSharedRaceProgressEdgeAlign(G_EX_ORIGIN_RIGHT,
                                       -((s16) HUD_SCREEN_WIDTH - HUD_CORNER_ALIGN_OFFSET) * 4);
    }
}

static void drawRaceSplitDividers(void* arg) {
    SpriteRenderArg* tracker = arg;
    SpriteFrameEntry* trackerFrame = &tracker->spriteData->frames[tracker->frameIndex];
    // The tracker belongs to the shared full-screen HUD even when slot 0xC resolves through a player viewport.
    s32 trackerTop = tracker->y + SCREEN_HEIGHT / 2;
    s32 trackerBottom = trackerTop + trackerFrame->height - 1;
    Gfx* gfx;

    if (!usesSplitScreenColumns()) {
        return;
    }

    gfx = gDisplayListAllocPtr;

    gEXPushScissor(gfx++);
    gEXPushOtherMode(gfx++);
    gEXPushFillColor(gfx++);
    gfx = setAspectAdjustedScissor(gfx);
    /*
     * Anchor the authored divider centre to the physical output centre, just like the race-progress tracker.
     * Leaving the divider on the implicit native origin can place it one scaled source pixel to the left at output
     * sizes whose centre does not map cleanly through RT64's aspect adjustment.
     */
    gEXSetRectAlign(gfx++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, -SCREEN_WIDTH * 2 + 4, 0,
                    -SCREEN_WIDTH * 2 + 4, 0);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(gfx++, 0x00010001);

    /*
     * match the original two-native-pixel horizontal viewport gap and center that equal-width column beneath
     * the tracker. Leave the tracker's exact authored span untouched: this callback executes after the shared HUD
     * callbacks, so filling through that span would incorrectly cover its white bar and player markers.
     */
    if (trackerTop > 0) {
        gDPFillRectangle(gfx++, SCREEN_WIDTH / 2 - 1, 0, SCREEN_WIDTH / 2, trackerTop - 1);
    }
    if (trackerBottom < SCREEN_HEIGHT - 1) {
        gDPFillRectangle(gfx++, SCREEN_WIDTH / 2 - 1, trackerBottom + 1, SCREEN_WIDTH / 2, SCREEN_HEIGHT - 1);
    }

    gDPPipeSync(gfx++);
    gEXPopFillColor(gfx++);
    gEXPopOtherMode(gfx++);
    gEXPopScissor(gfx++);
    gEXSetRectAspect(gfx++, G_EX_ASPECT_AUTO);
    gEXSetRectAlign(gfx++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);

    gDisplayListAllocPtr = gfx;
    gGraphicsMode = -1;
}

static void resetCornerHudAlign(void* unused) {
    if (!usesAdjustedHudLayout()) {
        return;
    }

    resetHudWidescreenAlign();
}

static void setPlayerLapCounterHudAlign(void* arg) {
    LapCounterSinglePlayerState* state = arg;

    if (!usesAdjustedHudLayout()) {
        return;
    }

    if (isRightColumnPlayer(state->playerIndex)) {
        setPlayerTopRightHudAlign(state->playerIndex);
    } else {
        setPlayerTopLeftHudAlign(state->playerIndex);
    }
}

static void setPlayerGoldHudAlign(void* arg) {
    PlayerGoldDisplayState* state = arg;

    if (!usesAdjustedHudLayout()) {
        return;
    }

    if (usesVerticalTwoPlayerSplit()) {
        setPlayerBottomRightHudAlign(state->playerIndex);
    } else if (usesHorizontalSplit() && state->playerIndex == 0) {
        setPlayerTopRightHudAlign(state->playerIndex);
    } else {
        setPlayerBottomRightHudAlign(state->playerIndex);
    }
}

static void setPlayerItemHudAlign(void* arg) {
    PlayerItemDisplayState* state = arg;
    s32 groupCenterFixed;
    s32 groupRightFixed;

    if (!usesAdjustedHudLayout()) {
        return;
    }

    groupCenterFixed = getItemGroupCenterFixed(state);
    groupRightFixed = getItemGroupRightFixed(state);
    sPlayerItemGroupBounds[state->playerIndex].centerFixed = groupCenterFixed;
    sPlayerItemGroupBounds[state->playerIndex].rightFixed = groupRightFixed;

    if (usesSplitScreenColumns()) {
        setViewportTopRightItemHudAlign(groupRightFixed, -HUD_CORNER_ALIGN_OFFSET);
    } else if (usesHorizontalSplit()) {
        setViewportCenteredHudAlign(groupCenterFixed, -HUD_CORNER_ALIGN_OFFSET);
    } else {
        setViewportCenteredHudAlign(groupCenterFixed, 0);
    }
}

static void setPlayerFloatingItemHudAlign(void* arg) {
    FloatingItemSpriteTask* state = arg;
    s32 playerIndex = state->renderPriority - 8;

    if (!usesAdjustedHudLayout() || playerIndex < 0 || playerIndex >= 4) {
        return;
    }

    if (usesSplitScreenColumns()) {
        setViewportTopRightItemHudAlign(sPlayerItemGroupBounds[playerIndex].rightFixed, -HUD_CORNER_ALIGN_OFFSET);
    } else if (usesHorizontalSplit()) {
        setViewportCenteredHudAlign(sPlayerItemGroupBounds[playerIndex].centerFixed, -HUD_CORNER_ALIGN_OFFSET);
    } else {
        setViewportCenteredHudAlign(sPlayerItemGroupBounds[playerIndex].centerFixed, 0);
    }
}

RECOMP_PATCH void initPlayerItemDisplayTask(PlayerItemDisplayState* state) {
    GameState* gameState;
    s32 playerMode;

    gameState = (GameState*) getCurrentAllocation();
    state->player = &gameState->players[state->playerIndex];
    playerMode = gameState->playerCount;

    if (playerMode >= 3) {
        goto else_branch;
    }
    if (playerMode == 0) {
        goto else_branch;
    }

    // @recomp preserve the original item-display initialization, selecting the large-item arrangement for vertical 2P
    if (playerMode == 1 || (playerMode == 2 && gRaceUsesVerticalTwoPlayerSplit)) {
        state->primaryItemX = -0x20;
        state->primaryItemY = -0x60;
        state->secondaryItemX = 0;
        state->secondaryItemY = -0x60;
    } else {
        state->primaryItemX = -0x88;
        state->primaryItemY = -0x30;
        state->secondaryItemX = -0x68;
        state->secondaryItemY = -0x30;
    }
    state->secondaryItemAsset = state->primaryItemAsset =
        loadCompressedData(&playerItemIconAsset_ROM_START, &playerItemIconAsset_ROM_END, 0x2608);
    state->itemCountX = state->primaryItemX + 0x18;
    state->itemCountY = state->primaryItemY + 0x10;
    state->digitAsset = loadCompressedData(&digit_sprite_ROM_START, &digit_sprite_ROM_END, 0x508);
    goto callbacks;

else_branch:
    state->primaryItemX = -0x10;
    state->primaryItemY = -0x30;
    state->secondaryItemX = 0;
    state->secondaryItemY = -0x30;
    state->secondaryItemAsset = state->primaryItemAsset =
        loadCompressedData(&playerItemIconMultiplayerAsset_ROM_START, &playerItemIconMultiplayerAsset_ROM_END, 0xB08);
    state->unk28 = 0;
    state->charDisplayPtr = &state->charDisplayValue;
    state->charDisplayX = state->primaryItemX + 8;
    state->charDisplayY = state->primaryItemY + 8;
    state->digitAsset = loadCompressedData(&playerItemIconAsset_ROM_START, &playerItemIconAsset_ROM_END, 0x2608);
    state->charDisplayFlag = 0;

callbacks:
    if (gameState->playerCount < 3) {
        setCallbackWithContinue(updatePlayerItemDisplaySinglePlayer);
    } else {
        setCallbackWithContinue(updatePlayerItemDisplayMultiplayer);
    }
    setCleanupCallback(cleanupPlayerItemDisplayTask);
}

// @recomp wrap calls to renderSpriteFrame to adjust for widescreen
RECOMP_PATCH void updatePlayerItemDisplaySinglePlayer(PlayerItemDisplayState* state) {
    Player* player;
    Player* playerRef;
    u8 tempValue;
    void* callback;

    updateRaceHudLayoutMode();

    // @recomp wrap texture rendering call to adjust for widescreen
    enqueueCallbackBySlotIndex((state->playerIndex + 8) & 0xFFFF, 0, resetCornerHudAlign, NULL);

    player = state->player;
    tempValue = player->primaryItemAmmo;
    if (tempValue != 0) {
        state->itemCountValue = tempValue;
        enqueueCallbackBySlotIndex((state->playerIndex + 8) & 0xFFFF, 0, renderSpriteFrame, &state->itemCountX);
    }

    callback = renderSpriteFrame;
    tempValue = state->player->primaryItemId;
    state->primaryItemIndex = tempValue;
    enqueueCallbackBySlotIndex((state->playerIndex + 8) & 0xFFFF, 0, callback, state);

    player = state->player;
    if ((player->unkBD8 & 1) != 0) {
        spawnFloatingItemSprite(state->primaryItemX - 8, state->primaryItemY - 8, 0, state->playerIndex + 8, 0);
        playerRef = state->player;
        tempValue = playerRef->unkBD8;
        playerRef->unkBD8 = tempValue & 0xFE;
    }

    tempValue = state->player->secondaryItemId;
    state->secondaryItemIndex = tempValue + 7;
    enqueueCallbackBySlotIndex((state->playerIndex + 8) & 0xFFFF, 0, callback, &state->secondaryItemX);

    player = state->player;
    if ((player->unkBD8 & 2) != 0) {
        spawnFloatingItemSprite(state->secondaryItemX - 8, state->secondaryItemY - 8, 1, state->playerIndex + 8, 0);
        playerRef = state->player;
        tempValue = playerRef->unkBD8;
        playerRef->unkBD8 = tempValue & 0xFD;
    }

    // @recomp wrap texture rendering call to adjust for widescreen
    enqueueCallbackBySlotIndex((state->playerIndex + 8) & 0xFFFF, 0, setPlayerItemHudAlign, state);
}

// @recomp wrap calls to renderSpriteFrame(WithPalette) to adjust for widescreen
RECOMP_PATCH void updatePlayerItemDisplayMultiplayer(PlayerItemDisplayState* state) {
    Player* player;
    u8 tempValue;
    Player* playerRef;
    void* callback;

    // @recomp wrap texture rendering call to adjust for widescreen
    updateRaceHudLayoutMode();
    enqueueCallbackBySlotIndex((state->playerIndex + 8) & 0xFFFF, 0, resetCornerHudAlign, NULL);

    player = state->player;
    tempValue = player->primaryItemAmmo;
    if (tempValue != 0) {
        state->charDisplayValue = tempValue + 0x30;
        enqueueCallbackBySlotIndex((state->playerIndex + 8) & 0xFFFF, 0, renderTextPalette, &state->charDisplayX);
    }

    callback = renderSpriteFrame;
    tempValue = state->player->primaryItemId;
    state->primaryItemIndex = tempValue;
    enqueueCallbackBySlotIndex((state->playerIndex + 8) & 0xFFFF, 0, callback, state);

    player = state->player;
    if ((player->unkBD8 & 1) != 0) {
        spawnFloatingItemSprite(state->primaryItemX - 4, state->primaryItemY - 4, 0, state->playerIndex + 8, 1);
        playerRef = state->player;
        tempValue = playerRef->unkBD8;
        playerRef->unkBD8 = tempValue & 0xFE;
    }

    tempValue = state->player->secondaryItemId;
    state->secondaryItemIndex = tempValue + 7;
    enqueueCallbackBySlotIndex((state->playerIndex + 8) & 0xFFFF, 0, callback, &state->secondaryItemX);

    player = state->player;
    if ((player->unkBD8 & 2) != 0) {
        spawnFloatingItemSprite(state->secondaryItemX - 4, state->secondaryItemY - 4, 1, state->playerIndex + 8, 1);
        playerRef = state->player;
        tempValue = playerRef->unkBD8;
        playerRef->unkBD8 = tempValue & 0xFD;
    }

    // @recomp wrap texture rendering call to adjust for widescreen
    enqueueCallbackBySlotIndex((state->playerIndex + 8) & 0xFFFF, 0, setPlayerItemHudAlign, state);
}

RECOMP_PATCH void updateFloatingItemSprite(FloatingItemSpriteTask* state) {
    state->baseAssetIndex = (state->frameCounter >> 1) + 0x45;
    state->frameCounter++;

    if (state->frameCounter == 0x10) {
        terminateCurrentTask();
    }

    // @recomp wrap texture rendering call to adjust for player viewport + screen layout
    enqueueCallbackBySlotIndex(state->renderPriority, 1, resetCornerHudAlign, NULL);

    if (state->halfSizeRender == 0) {
        enqueueCallbackBySlotIndex(state->renderPriority, 1, renderSpriteFrameWithPalette, state);
    } else {
        enqueueCallbackBySlotIndex(state->renderPriority, 1, renderHalfSizeSpriteWithCustomPalette, state);
    }

    // @recomp wrap texture rendering call to adjust for player viewport + screen layout
    enqueueCallbackBySlotIndex(state->renderPriority, 1, setPlayerFloatingItemHudAlign, state);
}

RECOMP_PATCH void initPlayerLapCounterTask(LapCounterState* state) {
    LapCounterAllocation* allocation;
    char* textBuffer;
    s16 temp;

    allocation = (LapCounterAllocation*) getCurrentAllocation();
    state->player = (void*) (((u8*) allocation->players) + (state->playerIndex * 0xBE8));

    if (allocation->numPlayers == 1) {
        state->x = -0x88;
        state->y = -0x60;
        state->lapIconAsset =
            loadCompressedData(&lapCounterIconAsset_ROM_START, &lapCounterIconAsset_ROM_END, 0x168);
        state->spriteIndex = 0;
        state->digitX1 = ((u16) state->x) + 0x1C;
        state->digitY1 = state->y;
        state->digitsAsset =
            loadCompressedData(&digit_sprite_ROM_START, &COSTUME_SLOT_00_COMPRESSED_DATA_ROM_START, 0x508);
        state->digitX2 = ((u16) state->digitX1) + 8;
        state->unk16 = 1;
        state->unk20 = 1;
        state->digitY2 = state->y;
        state->unk1C = state->lapIconAsset;
        temp = state->digitX2;
        state->digitY3 = state->y;
        state->unk28 = state->digitsAsset;
        state->digitX3 = ((u16) temp) + 8;
        state->totalLaps = allocation->totalLaps + 1;
        state->unk2E = 3;
    } else {
        state->y = -0x30;
        if (allocation->numPlayers == 2) {
            // @recomp adjust lap counter position depending on 2 player split mode.
            if (gRaceUsesVerticalTwoPlayerSplit) {
                state->x = HUD_VERTICAL_2P_LAP_X;
                state->y = HUD_VERTICAL_2P_LAP_Y;
                state->textX = state->x + 0x18;
            } else {
                state->x = -0x18;
                state->textX = 0;
            }
            state->textY = state->y;
        } else {
            if (state->playerIndex < 2) {
                state->x = -0x44;
            } else {
                state->x = 0x2C;
            }
            state->textX = state->x;
            state->textY = ((u16) state->y) + 8;
        }
        textBuffer = state->lapTextBuffer;
        state->lapIconAsset = loadCompressedData(&lapCounterMultiplayerIconAsset_ROM_START,
                                                 &lapCounterMultiplayerIconAsset_ROM_END, 0x98);
        state->spriteIndex = 0;
        state->digitsAsset = 0;
        _Sprintf(textBuffer, D_8009E868_9F468, 1, allocation->totalLaps + 1);
        state->unk34 = 1;
        state->lapText = textBuffer;
    }

    setCleanupCallback(cleanupPlayerLapCounterTask);
    if (allocation->numPlayers == 1) {
        setCallbackWithContinue(updatePlayerLapCounterSinglePlayer);
    } else {
        setCallbackWithContinue(updatePlayerLapCounterMultiplayer);
    }
}

// @recomp wrap calls to renderSpriteFrame(WithPalette) to adjust for widescreen
RECOMP_PATCH void updatePlayerLapCounterSinglePlayer(LapCounterSinglePlayerState* state) {
    // @recomp wrap texture rendering call to adjust for widescreen
    updateRaceHudLayoutMode();
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, resetCornerHudAlign, NULL);

    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderSpriteFrame, state);
    state->currentLap = state->player->currentLap + 1;
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderSpriteFrameWithPalette, &state->digitX1);
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderSpriteFrame, &state->digitX2);
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderSpriteFrameWithPalette, &state->digitX3);

    // @recomp wrap texture rendering call to adjust for widescreen
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, setPlayerLapCounterHudAlign, state);
}

// @recomp significantly alter updatePlayerLapCounterMultiplayer to account for widescreen HUD elements.
// the challenge here is properly supporting 1p, 2p and 3/4p modes since these all have different aligment
// characteristics.
RECOMP_PATCH void updatePlayerLapCounterMultiplayer(LapCounterMultiplayerState* state) {
    updateRaceHudLayoutMode();

    if (!usesAdjustedHudLayout()) {
        enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderSpriteFrame, state);
        state->unk3C = state->player->currentLap + 0x31;
        enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderTextPalette, &state->unk30);
        return;
    }

    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, resetCornerHudAlign, NULL);
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderSpriteFrame, state);
    state->unk3C = state->player->currentLap + 0x31;
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderTextPalette, &state->unk30);
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, setPlayerLapCounterMultiplayerEdgeAlign, state);
}

// @recomp significantly alter updatePlayerFinishPositionDisplay to account for widescreen HUD elements.
// the challenge here is properly supporting 1p, 2p and 3/4p modes since these all have different aligment
// characteristics.
RECOMP_PATCH void updatePlayerFinishPositionDisplay(FinishPositionDisplayState* state) {
    state->spriteIndex = state->player->finishPosition;
    updateRaceHudLayoutMode();
    if (usesVerticalTwoPlayerSplit()) {
        state->x = HUD_VERTICAL_2P_FINISH_X;
        state->y = 0x48;
    }

    if (!usesAdjustedHudLayout()) {
        enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderSpriteFrame, state);
        return;
    }

    if (usesSplitScreenColumns() || usesHorizontalSplit()) {
        enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, resetCornerHudAlign, NULL);
        enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderSpriteFrame, state);
        enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, setPlayerBottomLeftHudAlign, state);
    } else {
        enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, resetCornerHudAlign, NULL);
        enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderSpriteFrame, state);
        enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, setBottomLeftHudAlign, NULL);
    }
}

RECOMP_PATCH void updatePlayerGoldDisplaySinglePlayer(PlayerGoldDisplayState* state) {
    s32 gold = state->player->raceCoins;

    updateRaceHudLayoutMode();
    if (usesVerticalTwoPlayerSplit()) {
        state->x = 7;
        state->y = 0x58;
        state->iconX = state->x + 0x28;
        ((GoldDisplayState*) state)->iconY = state->y;
    }

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
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, setPlayerGoldHudAlign, state);
}

RECOMP_PATCH void updatePlayerGoldDisplayMultiplayer(MultiplayerGoldDisplayState* state) {
    s32 gold = state->player->raceCoins;

    updateRaceHudLayoutMode();

    if (gold < 100) {
        state->digitCount = 1;
    } else {
        state->digitCount = 2;
    }

    _Sprintf(state->goldTextBuffer, sMultiplayerGoldFormat, state->player->raceCoins);

    // @recomp wrap texture rendering call to adjust for widescreen
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, resetCornerHudAlign, NULL);

    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderTextPalette, &state->textX);

    state->animCounter++;
    if ((s16) state->animCounter >= 12) {
        state->animCounter = 0;
    }

    state->animFrame = (s16) state->animCounter >> 1;

    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, renderHalfSizeSpriteFrame, &state->iconX);

    // @recomp wrap texture rendering call to adjust for widescreen
    enqueueCallbackBySlotIndex((u16) (state->playerIndex + 8), 0, setPlayerGoldHudAlign, state);
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
    updateRaceHudLayoutMode();

    // @recomp fill the column divider around, but never over, the shared race-progress tracker.
    enqueueCallbackBySlotIndex(0xC, 0, drawRaceSplitDividers, state);

    // @recomp wrap race progress indicator rendering with its shared-viewport widescreen alignment.
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
    enqueueCallbackBySlotIndex(0xC, 0, setRaceProgressHudAlign, state);
}

RECOMP_PATCH void renderTrickScoreDisplay(TrickScoreDisplayState* state) {
    u16 viewportSlot = PLAYER_OVERLAY_VIEWPORT_SLOT(state->playerIndex);

    // @recomp render the trick-score display in its player's aspect-corrected overlay viewport.
    beginPlayerViewportOverlay(state->playerIndex);

    enqueueCallbackBySlotIndex(viewportSlot, OVERLAY_CALLBACK_LAYER, renderSpriteFrame, state);

    if (state->useGoldFormat == 0) {
        drawNumericString(state->scoreText, state->xPos + 0x38, state->yPos, 0xFF, state->digitsTexture,
                          viewportSlot, OVERLAY_CALLBACK_LAYER);
    } else {
        state->textX = state->xPos + 0x38;
        enqueueCallbackBySlotIndex(viewportSlot, OVERLAY_CALLBACK_LAYER, renderTextPalette, &state->textX);
    }

    // @recomp render the trick-score display in its player's aspect-corrected overlay viewport.
    endPlayerViewportOverlay(state->playerIndex);
}

RECOMP_PATCH void renderPauseMenuDisplay(PauseMenuDisplayState* state) {
    GameState* gameState;
    s32 i;

    gameState = getCurrentAllocation();
    i = 0;
    if (gameState->gamePaused == 1) {
        // @recomp render the pause menu as a full-screen overlay.
        beginFullScreenOverlay();

        do {
            if (gameState->pauseMenuSelection == i) {
                state->elements[i].padA[0] = 0x12;
            } else {
                state->elements[i].padA[0] = 0x11;
            }
            enqueueCallbackBySlotIndex(FULL_SCREEN_RACE_VIEWPORT_SLOT, OVERLAY_CALLBACK_LAYER,
                                       renderSpriteFrameWithPalette, &state->elements[i]);
            i++;
        } while (i < 3);
        renderTintedSpriteGrid(state->backgroundAsset, -0x20, -8, 4, 1, 0, 0x80, 0, 0, 0xFF, 0x80,
                               FULL_SCREEN_RACE_VIEWPORT_SLOT, OVERLAY_CALLBACK_LAYER);

        // @recomp render the pause menu as a full-screen overlay.
        endFullScreenOverlay();
    }
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

    if (!usesExpandedHudLayout()) {
        enqueueCallbackBySlotIndex(8, 0, renderSpriteFrame, &state->sprite);
        drawNumericString(buffer, state->sprite.x + 0x10, state->sprite.y + 0x10, 0xFF, state->digitAsset, 8, 1);
        return;
    }

    if (state->layoutMode == 0) {
        // @recomp active Shoot Cross counter/icon render together in layer 0 so widescreen state is shared.
        enqueueCallbackBySlotIndex(8, 0, resetCornerHudAlign, NULL);

        // @recomp tune the hit-count digits under the widened letterbox icon.
        countX = state->sprite.x + 0x18;
        drawNumericString(buffer, countX, state->sprite.y + 0x10, 0xFF, state->digitAsset, 8, 0);

        // @recomp tune only the icon relative to the hit count.
        iconArg = copySpriteArgWithXOffset(&state->sprite, 8);
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
