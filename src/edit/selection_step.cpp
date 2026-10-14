// SelectionManager's stepping of the pick with Tab.

#include <cassert>
#include <variant>
#include <vector>

#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"

namespace snapper {
namespace {

// The index after (before, for -1) i among count, wrapping.
size_t Stepped(size_t i, int direction, size_t count) {
  assert(count > 0);
  assert(direction == 1 || direction == -1);
  const size_t step = direction > 0 ? 1 : count - 1;
  return (i + step) % count;
}

}  // namespace

void SelectionManager::StepPick(int direction) {
  assert(history_ != nullptr);
  assert(direction == 1 || direction == -1);
  const Project& project = history_->current();
  const Shot* shot = FindShot(project, shot_);
  const bool can_step = shot != nullptr && !picks_.empty();
  if (!can_step) {
    return;
  }
  // The pick the panels follow: the focused layer's first, else any.
  Pick current = *picks_.begin();
  for (const Pick& pick : picks_) {
    const bool is_focus = pick.layer == focus_;
    if (is_focus) {
      current = pick;
      break;
    }
  }
  const Doll* doll = DollOfLayer(project, shot_, current.layer);
  const bool is_piece = !current.piece.isEmpty() && doll != nullptr;
  if (is_piece) {
    const auto& pieces = doll->rig.pieces;
    for (size_t i = 0; i < pieces.size(); ++i) {
      const bool is_current = pieces[i].name == current.piece;
      if (is_current) {
        const QString next = pieces[Stepped(i, direction, pieces.size())].name;
        PickThings({{current.layer, next}}, PickMode::kReplace,
                   current.layer);
        return;
      }
    }
    return;
  }
  std::vector<LayerId> layers;
  for (const Layer& layer : shot->layers) {
    const bool is_pickable = IsStagePicture(layer);
    if (is_pickable) {
      layers.push_back(layer.id);
    }
  }
  for (size_t i = 0; i < layers.size(); ++i) {
    const bool is_current = layers[i] == current.layer;
    if (is_current) {
      const LayerId next = layers[Stepped(i, direction, layers.size())];
      PickThings({{next, QString()}}, PickMode::kReplace, next);
      return;
    }
  }
}

}  // namespace snapper
