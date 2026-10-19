// StageView's drop: a character card dragged here goes on the stage,
// and picture files dropped here become picture layers, or with Alt held
// references to animate over.

#include <QMimeData>

#include <cassert>
#include <set>

#include "base/text.h"
#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"
#include "ui/character_cards.h"
#include "ui/character_drop.h"
#include "ui/stage_view.h"

namespace snapper {
namespace {

// True when the drop carries characters or files.
bool IsWanted(const QMimeData* data, const Managers& managers) {
  assert(data != nullptr);
  assert(managers.stage != nullptr);
  const bool is_character = !CharactersOfDrop(data).isEmpty();
  const bool is_files = !FilesOfDrop(data).isEmpty();
  return is_character || is_files;
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
  const QStringList characters = CharactersOfDrop(event->mimeData());
  const bool is_character = !characters.isEmpty();
  if (is_character) {
    event->acceptProposedAction();
    emit Problem(PutCharactersOnStage(managers_, characters));
  }
  const QStringList files =
      is_character ? QStringList() : FilesOfDrop(event->mimeData());
  // Alt: the files are references to animate over, not pictures.
  const bool is_reference_drop =
      !files.isEmpty() && managers_.references != nullptr &&
      event->modifiers().testFlag(Qt::AltModifier);
  if (is_reference_drop) {
    event->acceptProposedAction();
    DropReferences(files);
    update();
    return;
  }
  const bool is_files = !files.isEmpty();
  if (is_files) {
    event->acceptProposedAction();
    const auto frame = CurrentFrame();
    const ShotId target =
        frame.has_value() ? frame->shot : managers_.selection->shot();
    std::set<Pick> picks;
    QString problem;
    {
      EditScope step(managers_.history,
                     files.size() == 1 ? Tr("Add picture")
                                       : Tr("Add %1 pictures")
                                             .arg(files.size()));
      for (const QString& file : files) {
        const auto layer = managers_.stage->AddImage(target, file);
        if (!layer) {
          problem = layer.error().message;
          step.Cancel();
          break;
        }
        picks.insert(Pick{*layer});
      }
    }
    if (problem.isEmpty()) {
      managers_.selection->PickThings(
          picks, PickMode::kReplace,
          picks.empty() ? LayerId() : picks.begin()->layer);
    }
    emit Problem(problem);
  }
  update();
}

}  // namespace snapper
