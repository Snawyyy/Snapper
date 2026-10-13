#include "ui/draw_actions.h"

#include <QAction>
#include <QStandardItemModel>

#include <cassert>

#include "edit/draft_manager.h"
#include "ui/playhead.h"
#include "ui/problem.h"
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
  if (managers.draft == nullptr) {
    return;
  }
  for (const bool is_over : {false, true}) {
    QAction* action = menu->addAction(
        is_over ? QObject::tr("Draw over draft cel")
                : QObject::tr("Draw draft cel here"));
    action->setShortcut(is_over ? QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_N)
                                : QKeySequence(Qt::ALT | Qt::Key_N));
    const QString why_not = WhyNoDraftHere(managers, is_over);
    action->setEnabled(why_not.isEmpty());
    action->setToolTip(why_not.isEmpty()
                           ? QObject::tr("Sketch the shot's plan on its "
                                         "draft, over the whole frame.")
                           : why_not);
    QObject::connect(action, &QAction::triggered, menu, [managers, is_over] {
      RequestDraft(managers, is_over);
    });
  }
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
  if (managers.draft == nullptr) {
    return;
  }
  auto* draft = new QAction(QObject::tr("Draw draft cel"), widget);
  draft->setShortcut(QKeySequence(Qt::ALT | Qt::Key_N));
  draft->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(draft, &QAction::triggered, widget,
                   [managers] { RequestDraft(managers, false); });
  auto* over = new QAction(QObject::tr("Draw over draft cel"), widget);
  over->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_N));
  over->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  QObject::connect(over, &QAction::triggered, widget,
                   [managers] { RequestDraft(managers, true); });
  widget->addAction(draft);
  widget->addAction(over);
}

QString WhyNoDraftHere(const Managers& managers, bool is_over) {
  assert(managers.history != nullptr);
  if (managers.draft == nullptr) {
    return QObject::tr("Drafts aren't available here.");
  }
  const auto spot = SpotOf(managers);
  if (!spot) {
    return QObject::tr("Put the playhead in a shot first.");
  }
  return managers.draft->WhyNoDraw(spot->shot, spot->local, is_over);
}

void RequestDraft(const Managers& managers, bool is_over) {
  assert(managers.toasts != nullptr);
  const QString why_not = WhyNoDraftHere(managers, is_over);
  if (!why_not.isEmpty()) {
    managers.toasts->Problem(why_not);
    return;
  }
  const auto spot = SpotOf(managers);
  Report(managers, is_over
                       ? managers.draft->DrawOver(spot->shot, spot->local)
                       : managers.draft->DrawNew(spot->shot, spot->local));
}

}  // namespace snapper
