// StageView's undo flash: what an undo or redo changed lights up, drawn
// from the real pixels of the layers that differ. The old state has a
// renderer of its own.

#include <QTimer>

#include <cassert>
#include <set>

#include "edit/change_diff.h"
#include "edit/history_manager.h"
#include "ui/stage_view.h"

namespace snapper {
void StageView::ShowStep(const Project& before, bool is_travel) {
  assert(managers_.history != nullptr);
  assert(before.shots.size() <= static_cast<size_t>(kMaxShots));
  const bool is_shown = is_travel;
  if (!is_shown) {
    return;
  }
  // The flash is slow to work out; the undone state paints first.
  QTimer::singleShot(0, this, [this, before] { BuildFlash(before); });
}

void StageView::BuildFlash(const Project& before) {
  assert(managers_.history != nullptr);
  assert(before.shots.size() <= static_cast<size_t>(kMaxShots));
  const auto frame = CurrentFrame();
  const bool has_frame = frame.has_value();
  if (!has_frame) {
    return;
  }
  const Project& project = managers_.history->current();
  const Shot* old_shot = FindShot(before, frame->shot);
  const Shot* new_shot = FindShot(project, frame->shot);
  const bool has_both = old_shot != nullptr && new_shot != nullptr;
  if (!has_both) {
    return;
  }
  std::set<LayerId> only;
  for (const LayerId layer : ChangedLayers(before, project, frame->shot)) {
    only.insert(layer);
  }
  const bool has_changes = !only.empty();
  if (!has_changes) {
    return;
  }
  flash_.Show(was_renderer_.RenderLayers(before, *old_shot, frame->local,
                                     frame->scale, only),
              renderer_.RenderLayers(project, *new_shot, frame->local,
                                     frame->scale, only),
              frame->corner);
}

}  // namespace snapper
