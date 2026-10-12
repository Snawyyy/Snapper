// StageView's undo flash: what an undo or redo changed lights up, drawn
// from the real pixels of the pieces (or layers) that differ. The old
// state has a renderer of its own, so drawing it does not throw away the
// poses the stage keeps for the new one.

#include <QTimer>

#include <cassert>
#include <map>
#include <set>

#include "edit/change_diff.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "render/stage_geometry.h"
#include "ui/stage_view.h"

namespace snapper {
namespace {

using Only = std::map<LayerId, std::set<QString>>;

// Adds to only the pieces of a doll layer that stand differently on the
// two sides; a moved layer changes all of them.
void AddPieces(const Project& before, const Project& after,
               const StageFrame& frame, LayerId layer, PoseMemo* old_memo,
               PoseMemo* new_memo, Only* only) {
  assert(only != nullptr);
  assert(frame.scale > 0.0);
  const Shot* old_shot = FindShot(before, frame.shot);
  const Shot* new_shot = FindShot(after, frame.shot);
  const Doll* doll = DollOfLayer(after, frame.shot, layer);
  const bool is_usable = old_shot != nullptr && new_shot != nullptr &&
                         doll != nullptr;
  if (!is_usable) {
    return;
  }
  for (const RigPiece& piece : doll->rig.pieces) {
    const auto was = PieceOnScreen(before, *old_shot, layer, piece.name,
                                   frame.local, frame.scale, old_memo);
    const auto now = PieceOnScreen(after, *new_shot, layer, piece.name,
                                   frame.local, frame.scale, new_memo);
    const bool is_changed = now.has_value() &&
                            (!was.has_value() || was->outline != now->outline);
    if (is_changed) {
      (*only)[layer].insert(piece.name);
    }
  }
}

}  // namespace

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
  // A layer that maps to nothing is drawn whole; a doll maps to the
  // pieces that changed.
  Only only;
  for (const LayerId layer : ChangedLayers(before, project, frame->shot)) {
    const bool is_doll = DollOfLayer(project, frame->shot, layer) != nullptr;
    if (is_doll) {
      AddPieces(before, project, *frame, layer, was_renderer_.cache()->poses(),
                renderer_.cache()->poses(), &only);
    } else {
      only[layer];
    }
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
