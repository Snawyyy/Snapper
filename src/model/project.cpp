#include "model/project.h"

#include <cassert>

namespace snapper {

bool IsValidCanvas(CanvasSize canvas) {
  static_assert(kMinCanvasSide > 0);
  static_assert(kMinCanvasSide <= kMaxCanvasSide);
  const auto is_valid_side = [](int side) {
    return side >= kMinCanvasSide && side <= kMaxCanvasSide;
  };
  // Video encoders need even sides.
  const bool is_even = canvas.width % 2 == 0 && canvas.height % 2 == 0;
  return is_valid_side(canvas.width) && is_valid_side(canvas.height) &&
         is_even;
}

int ShotIndex(const Project& project, ShotId id) {
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  assert(id.value() >= 0);
  const int count = static_cast<int>(project.shots.size());
  for (int i = 0; i < count; ++i) {
    const bool is_match = project.shots[static_cast<size_t>(i)]->id == id;
    if (is_match) {
      return i;
    }
  }
  return -1;
}

const Shot* FindShot(const Project& project, ShotId id) {
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  assert(id.value() >= 0);
  const int index = ShotIndex(project, id);
  const bool is_found = index >= 0;
  return is_found ? project.shots[static_cast<size_t>(index)].get() : nullptr;
}

const Doll* FindDoll(const Project& project, const QString& name) {
  assert(project.dolls.size() <= static_cast<size_t>(kMaxProjectDolls));
  assert(!name.isEmpty());
  const auto found = project.dolls.find(name);
  const bool is_found = found != project.dolls.end() && found->second;
  return is_found ? found->second.get() : nullptr;
}

}  // namespace snapper
