#ifndef SNAPPER_ANIM_LIGHT_TABLE_H_
#define SNAPPER_ANIM_LIGHT_TABLE_H_

#include <vector>

#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// How many frames either side the light table shows through.
inline constexpr int kLightTableReach = 2;

// One frame seen through the paper: its master frame, how strongly it
// shows (fainter further away) and whether it comes before the playhead.
struct OnionSkin final {
  Frame master;
  double opacity = 0.0;
  bool is_before = false;

  bool operator==(const OnionSkin&) const = default;
};

// The frames around master that the light table shows, nearest last so
// they draw on top: up to reach either side, never past the ends of
// master's own shot.
std::vector<OnionSkin> LightTableSkins(const Project& project, Frame master,
                                       int reach);

}  // namespace snapper

#endif  // SNAPPER_ANIM_LIGHT_TABLE_H_
