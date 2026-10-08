#include "anim/master_timeline.h"

#include <cassert>

namespace snapper {

Frame ShotStart(const Project& project, int index) {
  assert(index >= 0);
  assert(index <= static_cast<int>(project.shots.size()));
  int start = 0;
  for (int i = 0; i < index; ++i) {
    start += project.shots[static_cast<size_t>(i)]->length.index();
  }
  return Frame(start);
}

Frame TotalLength(const Project& project) {
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  assert(kMaxShots > 0);
  return ShotStart(project, static_cast<int>(project.shots.size()));
}

ShotMoment Locate(const Project& project, Frame master) {
  assert(master.index() >= 0);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  ShotMoment moment;
  const int count = static_cast<int>(project.shots.size());
  int start = 0;
  for (int i = 0; i < count; ++i) {
    const int length = project.shots[static_cast<size_t>(i)]->length.index();
    const int local = master.index() - start;
    const bool is_inside = local < length;
    if (is_inside) {
      moment.shot = i;
      moment.local = Frame(local);
      return moment;
    }
    start += length;
  }
  return moment;
}

}  // namespace snapper
