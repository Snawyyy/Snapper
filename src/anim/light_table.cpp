#include "anim/light_table.h"

#include <cassert>

#include "anim/master_timeline.h"

namespace snapper {
namespace {

// The nearest frame shows this strongly; each step out halves it.
constexpr double kNearOpacity = 0.6;

}  // namespace

std::vector<OnionSkin> LightTableSkins(const Project& project, Frame master,
                                       int reach) {
  assert(master.index() >= 0);
  assert(reach >= 0 && reach < 100);
  std::vector<OnionSkin> skins;
  const ShotMoment now = Locate(project, master);
  if (now.shot < 0) {
    return skins;
  }
  const int first = ShotStart(project, now.shot).index();
  const int length =
      project.shots[static_cast<size_t>(now.shot)]->length.index();
  for (int step = reach; step >= 1; --step) {
    const double opacity = kNearOpacity / (1 << (step - 1));
    const int before = master.index() - step;
    const int after = master.index() + step;
    if (before >= first) {
      skins.push_back({Frame(before), opacity, true});
    }
    if (after < first + length) {
      skins.push_back({Frame(after), opacity, false});
    }
  }
  return skins;
}

}  // namespace snapper
