#include "ui/draw_actions.h"

#include <QAction>
#include <QStandardItemModel>

#include <cassert>

#include "ui/playhead.h"
#include "ui/problem.h"
#include "ui/storyboard_actions.h"
#include "ui/toast_manager.h"

namespace snapper {
namespace {

// The picked piece at the playhead, if a piece is picked.
std::optional<std::pair<TrackRef, Frame>> PickedPiece(
    const Managers& managers) {
  assert(managers.selection != nullptr);
  const auto spot = SpotOf(managers);
  const auto track = spot ? PickedTrack(managers, spot->shot) : std::nullopt;
  const bool is_piece = track && track->kind == TrackKind::kPiece;
  if (!is_piece) {
    return std::nullopt;
  }
  return std::pair{*track, spot->local};
}

void Report(const Managers& managers, const Result<void>& result) {
  assert(managers.toasts != nullptr);
  const QString problem = ProblemOf(result);
  if (!problem.isEmpty()) {
    managers.toasts->Problem(problem);
  }
}

}  // namespace

QString WhyNoDrawHere(const Managers& managers, bool is_edit) {
  assert(managers.history != nullptr);
  if (managers.draw == nullptr) {
    return QObject::tr("Drawing isn't available here.");
  }
  const auto picked = PickedPiece(managers);
  if (!picked) {
    return QObject::tr("Pick a piece of a doll on the stage first.");
  }
  return managers.draw->WhyNoDraw(picked->first, picked->second, is_edit);
}

void RequestDraw(const Managers& managers, DrawStart start) {
  assert(managers.toasts != nullptr);
  const QString why_not = WhyNoDrawHere(managers, false);
  if (!why_not.isEmpty()) {
    managers.toasts->Problem(why_not);
    return;
  }
  const auto picked = PickedPiece(managers);
  Report(managers, managers.draw->Begin(picked->first, picked->second, start));
}

void RequestEditDrawing(const Managers& managers) {
  assert(managers.toasts != nullptr);
  const QString why_not = WhyNoDrawHere(managers, true);
  if (!why_not.isEmpty()) {
    managers.toasts->Problem(why_not);
    return;
  }
  const auto picked = PickedPiece(managers);
  Report(managers, managers.draw->BeginEdit(picked->first, picked->second));
}

int AddDrawStarts(QComboBox* box, const Managers& managers) {
  assert(box != nullptr);
  assert(managers.history != nullptr);
  if (managers.draw == nullptr) {
    return 0;
  }
  const QString why_not = WhyNoDrawHere(managers, false);
  const auto* items = qobject_cast<QStandardItemModel*>(box->model());
  const QStringList names = {QObject::tr("+ New drawing: blank"),
                             QObject::tr("+ New drawing: copy of this one"),
                             QObject::tr("+ New drawing: copy of the default")};
  for (const QString& name : names) {
    box->addItem(name);
    QStandardItem* item =
        items != nullptr ? items->item(box->count() - 1) : nullptr;
    if (item != nullptr) {
      item->setEnabled(why_not.isEmpty());
      item->setToolTip(why_not.isEmpty()
                           ? QObject::tr("Draw it in Snapper (N on the stage).")
                           : why_not);
    }
  }
  return static_cast<int>(names.size());
}

DrawStart DrawStartAt(int at) {
  assert(at >= 0);
  assert(at < 3);
  return at == 0 ? DrawStart::kBlank
         : at == 1 ? DrawStart::kShowing
                   : DrawStart::kDefault;
}

void AddDrawActions(QMenu* menu, const Managers& managers) {
  assert(menu != nullptr);
  assert(managers.history != nullptr);
  if (managers.draw == nullptr) {
    return;
  }
  QAction* draw = menu->addAction(QObject::tr("Draw new drawing"));
  draw->setShortcut(QKeySequence(Qt::Key_N));
  const QString no_draw = WhyNoDrawHere(managers, false);
  draw->setEnabled(no_draw.isEmpty());
  draw->setToolTip(no_draw.isEmpty()
                       ? QObject::tr("A blank drawing for the piece, with "
                                     "its shown drawing to look at (hold "
                                     "R, or D for the onion).")
                       : no_draw);
  QObject::connect(draw, &QAction::triggered, menu, [managers] {
    RequestDraw(managers, DrawStart::kBlank);
  });
  QAction* edit = menu->addAction(QObject::tr("Edit drawing"));
  edit->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_N));
  const QString no_edit = WhyNoDrawHere(managers, true);
  edit->setEnabled(no_edit.isEmpty());
  edit->setToolTip(no_edit.isEmpty()
                       ? QObject::tr("Draw over the drawing it shows; a "
                                     "Krita drawing is copied first.")
                       : no_edit);
  QObject::connect(edit, &QAction::triggered, menu,
                   [managers] { RequestEditDrawing(managers); });
  AddStoryboardActions(menu, managers);
}

void AddDrawKeys(QWidget* widget, const Managers& managers) {
  assert(widget != nullptr);
  assert(managers.history != nullptr);
  if (managers.draw == nullptr) {
    return;
  }
  auto* draw = new QAction(QObject::tr("Draw new drawing"), widget);
  draw->setShortcut(QKeySequence(Qt::Key_N));
  draw->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(draw, &QAction::triggered, widget, [managers] {
    RequestDraw(managers, DrawStart::kBlank);
  });
  auto* edit = new QAction(QObject::tr("Edit drawing"), widget);
  edit->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_N));
  edit->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(edit, &QAction::triggered, widget,
                   [managers] { RequestEditDrawing(managers); });
  widget->addAction(draw);
  widget->addAction(edit);
}

}  // namespace snapper
