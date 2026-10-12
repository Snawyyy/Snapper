// StageView's drop: a doll card dragged here goes on the stage, and
// picture files dropped here become objects placed where they fell.

#include <QMimeData>

#include <cassert>
#include <set>

#include "edit/history_manager.h"
#include "edit/object_manager.h"
#include "edit/selection_manager.h"
#include "render/frame_renderer.h"
#include "ui/doll_cards.h"
#include "ui/doll_drop.h"
#include "ui/stage_view.h"

namespace snapper {
namespace {

// True when the drop carries dolls, or files while objects can be made.
bool IsWanted(const QMimeData* data, const Managers& managers) {
  assert(data != nullptr);
  const bool is_doll = !DollsOfDrop(data).isEmpty();
  const bool is_files =
      managers.objects != nullptr && !FilesOfDrop(data).isEmpty();
  return is_doll || is_files;
}

}  // namespace

void StageView::dragEnterEvent(QDragEnterEvent* event) {
  assert(event != nullptr);
  assert(event->mimeData() != nullptr);
  const bool is_wanted = IsWanted(event->mimeData(), managers_);
  if (is_wanted) {
    is_drop_target_ = true;
    event->acceptProposedAction();
    update();
  }
}

void StageView::dragMoveEvent(QDragMoveEvent* event) {
  assert(event != nullptr);
  assert(event->mimeData() != nullptr);
  const bool is_wanted = IsWanted(event->mimeData(), managers_);
  if (is_wanted) {
    event->acceptProposedAction();
  }
}

void StageView::dragLeaveEvent(QDragLeaveEvent* event) {
  assert(event != nullptr);
  assert(managers_.selection != nullptr);
  is_drop_target_ = false;
  update();
}

void StageView::dropEvent(QDropEvent* event) {
  assert(event != nullptr);
  assert(event->mimeData() != nullptr);
  is_drop_target_ = false;
  const QStringList dolls = DollsOfDrop(event->mimeData());
  const bool is_doll = !dolls.isEmpty();
  if (is_doll) {
    event->acceptProposedAction();
    emit Problem(PutDollsOnStage(managers_, dolls));
  }
  const QStringList files = managers_.objects != nullptr
                                ? FilesOfDrop(event->mimeData())
                                : QStringList();
  const auto frame = CurrentFrame();
  const bool is_files = !is_doll && !files.isEmpty();
  if (is_files) {
    event->acceptProposedAction();
    const Project& project = managers_.history->current();
    const Shot* shot =
        frame.has_value() ? FindShot(project, frame->shot) : nullptr;
    // Where it fell, in shot space; the middle when that is unknown.
    std::optional<QPointF> at;
    if (shot != nullptr) {
      bool is_invertible = false;
      const QTransform back =
          FrameRenderer::ViewTransform(project, *shot, frame->local,
                                       frame->scale)
              .inverted(&is_invertible);
      if (is_invertible) {
        at = back.map(event->position() - frame->corner);
      }
    }
    QStringList notes;
    const ShotId target =
        shot != nullptr ? frame->shot : managers_.selection->shot();
    const auto layers =
        managers_.objects->AddToShot(target, files, at, &notes);
    if (layers) {
      std::set<Pick> picks;
      for (const LayerId layer : *layers) {
        picks.insert({layer, QString()});
      }
      managers_.selection->PickThings(
          picks, PickMode::kReplace,
          layers->empty() ? LayerId() : layers->front());
    }
    emit Problem(layers ? notes.join(QStringLiteral(" "))
                        : layers.error().message);
  }
  update();
}

}  // namespace snapper
