#include "patches.h"
#include "patch_helpers.h"

#include "D_800AFE8C_A71FC_type.h"
#include "graphics/graphics.h"
#include "system/task_scheduler.h"

DECLARE_FUNC(void, recomp_set_game_player_count, u32 playerCount);

extern u8 gTitleExitMode;

RECOMP_PATCH void exitTitleToNextMode(void) {
    recomp_set_game_player_count(1);
    returnToParentScheduler(gTitleExitMode);
}

RECOMP_PATCH void onPlayerCountProceed(void) {
    recomp_set_game_player_count(gGameSessionContext->numPlayers);
    returnToParentScheduler(1);
}

RECOMP_PATCH void onCharacterSelectCancel(void) {
    recomp_set_game_player_count(1);
    setViewportFadeValue(NULL, 0, 0);
    returnToParentScheduler(0xFF);
}
