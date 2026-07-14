#pragma once

namespace recomp {
struct GameEntry;
}

namespace sk2::launcher {

void register_callbacks(const recomp::GameEntry& game);

} // namespace sk2::launcher
