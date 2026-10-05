#include "anim/master_timeline.h"

#include <cassert>

namespace snapper {
namespace {

const Shot* ShotAt(const Project& project, int index) {
  assert(index >= 0);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  const bool is_in_range = index < static_cast<int>(project.shots.size());
  return is_in_range ? project.shots[static_cast<size_t>(index)].get()
                     : nullptr;
}

// Frames from this shot's start to the next one's.
int Stride(const Project& project, int index) {
  const Shot* shot = ShotAt(project, index);
  assert(shot != nullptr);
  assert(shot->length.index() >= 0);
  return shot->length.index() -
         UsableTransition(*shot, ShotAt(project, index + 1)).index();
}

}  // namespace

Frame ShotStart(const Project& project, int index) {
  assert(index >= 0);
  assert(index <= static_cast<int>(project.shots.size()));
  int start = 0;
  for (int i = 0; i < index; ++i) {
    start += Stride(project, i);
  }
  return Frame(start);
}

Frame TotalLength(const Project& project) {
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  const int count = static_cast<int>(project.shots.size());
  const bool is_empty = count == 0;
  if (is_empty) {
    return Frame(0);
  }
  const int last_start = ShotStart(project, count - 1).index();
  assert(last_start >= 0);
  return Frame(last_start + ShotAt(project, count - 1)->length.index());
}

ShotMoment Locate(const Project& project, Frame master) {
  assert(master.index() >= 0);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  ShotMoment moment;
  const int count = static_cast<int>(project.shots.size());
  int start = 0;
  for (int i = 0; i < count; ++i) {
    const Shot& shot = *ShotAt(project, i);
    const int local = master.index() - start;
    const bool is_inside = local < shot.length.index();
    if (is_inside) {
      moment.shot = i;
      moment.local = Frame(local);
      const Shot* next = ShotAt(project, i + 1);
      const int overlap = UsableTransition(shot, next).index();
      const int into_overlap = local - (shot.length.index() - overlap);
      const bool is_mixing = overlap > 0 && into_overlap >= 0;
      if (is_mixing) {
        moment.next_shot = i + 1;
        moment.next_local = Frame(into_overlap);
        moment.mix = (into_overlap + 1.0) / (overlap + 1.0);
      }
      return moment;
    }
    start += Stride(project, i);
  }
  return moment;
}

}  // namespace snapper
