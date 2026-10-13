// The Pose timeline's Draft row (see timeline_draft.h).

#include "ui/timeline_draft.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "anim/master_timeline.h"
#include "edit/draft_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reference_manager.h"
#include "ui/draw_actions.h"
#include "ui/form_helpers.h"
#include "ui/grab.h"
#include "ui/paint_clip.h"
#include "ui/pick_keys.h"
#include "ui/problem.h"
#include "ui/theme.h"
#include "ui/timeline_layout.h"
#include "ui/timeline_view.h"
#include "ui/toast_manager.h"

namespace snapper {
namespace {

// The draft row is dashed and grey-green, so it never reads as a doll's
// key row or a reference bar.
constexpr QColor kDraftFill{140, 200, 150, 30};
constexpr QColor kDraftLine{150, 210, 160};
constexpr double kEyeSize = 14.0;

QRectF EyeRect(double top) {
  assert(top >= 0.0);
  assert(kNameWidth > kEyeSize);
  return QRectF(kNameWidth - kEyeSize - 6.0,
                top + (kRowHeight - kEyeSize) / 2.0, kEyeSize, kEyeSize);
}

QPolygonF Diamond(QPointF center) {
  assert(std::isfinite(center.x()));
  assert(kKeySize > 0.0);
  return QPolygonF({center + QPointF(0, -kKeySize),
                    center + QPointF(kKeySize, 0),
                    center + QPointF(0, kKeySize),
                    center + QPointF(-kKeySize, 0)});
}

}  // namespace

TimelineDraft::TimelineDraft(const Managers& managers, TimelineView* view)
    : QObject(view), managers_(managers), view_(view) {
  assert(view_ != nullptr);
  assert(managers_.history != nullptr);
  if (managers_.draft == nullptr) {
    return;
  }
  view_->installEventFilter(this);
  connect(managers_.draft, &DraftManager::Changed, view_,
          [this] { view_->update(); });
}

TimelineDraft::~TimelineDraft() {
  // A drag cut short by closing lands nothing.
  if (drag_ != nullptr) {
    drag_->Cancel();
  }
}

ShotId TimelineDraft::Shot() const {
  assert(managers_.selection != nullptr);
  return managers_.selection->shot();
}

double TimelineDraft::Height() const {
  assert(managers_.history != nullptr);
  const bool has_row = managers_.draft != nullptr &&
                       managers_.draft->Of(Shot()) != nullptr;
  return has_row ? kRowHeight : 0.0;
}

double TimelineDraft::Top() const {
  const size_t references = managers_.references != nullptr
                                ? managers_.references->Of(Shot()).size()
                                : 0;
  assert(references <= static_cast<size_t>(kMaxReferencesPerShot));
  return kRulerHeight + kWaveHeight +
         static_cast<double>(references) * kRowHeight;
}

bool TimelineDraft::IsInRow(QPointF point) const {
  assert(std::isfinite(point.y()));
  const double top = Top();
  return Height() > 0.0 && point.y() >= top && point.y() < top + kRowHeight;
}

double TimelineDraft::FrameWidth() const {
  const double width = view_->XOf(Frame(1)) - view_->XOf(Frame(0));
  assert(width > 0.0);
  return width;
}

std::optional<Frame> TimelineDraft::CelNear(double x) const {
  assert(std::isfinite(x));
  const Draft* draft = managers_.draft->Of(Shot());
  if (draft == nullptr) {
    return std::nullopt;
  }
  const double half = FrameWidth() / 2.0;
  std::vector<QPointF> spots;
  for (const DraftCel& cel : draft->cels) {
    spots.emplace_back(view_->XOf(cel.at) + half, 0.0);
  }
  const int nearest =
      NearestGrab(spots, QPointF(x, 0.0), std::max(half, kGrabReach));
  return nearest >= 0
             ? std::optional<Frame>(draft->cels[static_cast<size_t>(nearest)].at)
             : std::nullopt;
}

void TimelineDraft::Paint(QPainter* painter) const {
  assert(painter != nullptr);
  const Draft* draft =
      managers_.draft != nullptr ? managers_.draft->Of(Shot()) : nullptr;
  const ::snapper::Shot* shot = FindShot(managers_.history->current(), Shot());
  if (draft == nullptr || shot == nullptr) {
    return;
  }
  const double top = Top();
  const double width = view_->width();
  const QRectF name(0, top, kNameWidth, kRowHeight);
  painter->save();
  painter->fillRect(name, theme::kFace);
  painter->setPen(draft->is_shown ? theme::kText : theme::kTextOff);
  painter->drawText(name.adjusted(6, 0, -(kEyeSize + 10.0), 0),
                    Qt::AlignVCenter | Qt::AlignLeft, tr("Draft"));
  // The eye: open with a pupil when shown, a line when hidden.
  const QRectF eye = EyeRect(top);
  const QRectF lid = eye.adjusted(0.0, 3.0, 0.0, -3.0);
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(draft->is_shown ? theme::kText : theme::kTextOff, 1.2));
  if (draft->is_shown) {
    painter->drawEllipse(lid);
    painter->setBrush(theme::kText);
    painter->drawEllipse(eye.center(), 2.0, 2.0);
  } else {
    painter->drawLine(QPointF(lid.left(), lid.center().y()),
                      QPointF(lid.right(), lid.center().y()));
  }
  {
    const PaintClip keep_in(
        painter, QRectF(kNameWidth, top, width - kNameWidth, kRowHeight));
    painter->setOpacity(draft->is_shown ? 1.0 : 0.4);
    const double from = view_->XOf(Frame(0));
    const double to = view_->XOf(shot->length);
    painter->setBrush(kDraftFill);
    painter->setPen(QPen(kDraftLine, 1.0, Qt::DashLine));
    painter->drawRect(QRectF(from, top + 3.0, std::max(to - from, 2.0),
                             kRowHeight - 6.0));
    const double middle = top + kRowHeight / 2.0;
    const double half = FrameWidth() / 2.0;
    const std::set<Frame> picked = managers_.draft->Picked(Shot());
    for (size_t i = 0; i < draft->cels.size(); ++i) {
      const DraftCel& cel = draft->cels[i];
      const bool is_last = i + 1 == draft->cels.size();
      const double x = view_->XOf(cel.at) + half;
      const double end =
          is_last ? std::max(to, x) : view_->XOf(draft->cels[i + 1].at) + half;
      painter->setPen(QPen(kDraftLine, 2.0));
      painter->drawLine(QPointF(x, middle), QPointF(end, middle));
      const bool is_picked = picked.contains(cel.at);
      painter->setPen(QPen(is_picked ? theme::kText : theme::kShadow, 1.0));
      painter->setBrush(is_picked ? theme::kPick : kDraftLine);
      painter->drawPolygon(Diamond(QPointF(x, middle)));
    }
    painter->setOpacity(1.0);
  }
  painter->setPen(theme::kFaceDark);
  painter->drawLine(QPointF(0, top + kRowHeight),
                    QPointF(width, top + kRowHeight));
  painter->restore();
}

bool TimelineDraft::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  const bool is_view = watched == view_ && managers_.draft != nullptr;
  if (!is_view) {
    return QObject::eventFilter(watched, event);
  }
  switch (event->type()) {
    case QEvent::MouseButtonPress:
      return Press(*static_cast<QMouseEvent*>(event));
    case QEvent::MouseMove:
      return MoveTo(*static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonRelease:
      return Release(*static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonDblClick:
      return DoubleClick(*static_cast<QMouseEvent*>(event));
    case QEvent::ContextMenu:
      return Menu(*static_cast<QContextMenuEvent*>(event));
    case QEvent::KeyPress:
      return Key(*static_cast<QKeyEvent*>(event));
    case QEvent::ShortcutOverride: {
      // Delete and Escape reach the row first while it has cels picked
      // or a drag running.
      const auto* key = static_cast<QKeyEvent*>(event);
      const bool is_ours =
          (key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace ||
           key->key() == Qt::Key_Escape) &&
          (drag_ != nullptr || !managers_.draft->Picked(Shot()).empty());
      if (is_ours) {
        event->accept();
        return true;
      }
      return false;
    }
    default:
      return QObject::eventFilter(watched, event);
  }
}

bool TimelineDraft::Press(const QMouseEvent& event) {
  assert(managers_.draft != nullptr);
  assert(drag_ == nullptr);
  if (!IsInRow(event.position())) {
    // A press anywhere else on the timeline leaves the cels.
    managers_.draft->ClearPick();
    return false;
  }
  // Right click opens the row's menu (a ContextMenu event of its own).
  if (event.button() != Qt::LeftButton) {
    return true;
  }
  const ShotId shot = Shot();
  const bool is_eye = EyeRect(Top()).adjusted(-4, -4, 4, 4).contains(
      event.position());
  if (is_eye) {
    const Draft* draft = managers_.draft->Of(shot);
    Report(ProblemOf(managers_.draft->SetShown(shot, !draft->is_shown)));
    return true;
  }
  const auto cel = CelNear(event.position().x());
  if (!cel) {
    managers_.draft->ClearPick();
    return true;
  }
  const bool is_picked = managers_.draft->Picked(shot).contains(*cel);
  const auto mode = ClickMode(event.modifiers(), is_picked);
  is_narrowing_ = !mode.has_value();
  if (mode) {
    managers_.draft->Pick(shot, {*cel}, *mode);
  }
  const std::set<Frame> picked = managers_.draft->Picked(shot);
  is_pressed_ = picked.contains(*cel);
  press_ = event.position();
  press_frame_ = *cel;
  origin_ = picked;
  return true;
}

bool TimelineDraft::MoveTo(const QMouseEvent& event) {
  assert(managers_.draft != nullptr);
  if (!is_pressed_) {
    return false;
  }
  const bool is_dragging =
      drag_ != nullptr || IsPastDeadZone(press_, event.position());
  if (!is_dragging) {
    return true;
  }
  if (drag_ == nullptr) {
    drag_ = std::make_unique<EditScope>(
        managers_.history, origin_.size() == 1 ? tr("Move draft cel")
                                               : tr("Move draft cels"));
    view_->setCursor(Qt::SizeHorCursor);
  }
  const auto frame = view_->FrameAt(std::max<double>(event.position().x(),
                                                     kNameWidth));
  const int offset = frame ? frame->index() - press_frame_.index() : 0;
  // A spot the cels can't land on leaves them where they last could.
  (void)managers_.draft->MoveCels(Shot(), origin_, offset);
  view_->update();
  return true;
}

bool TimelineDraft::Release(const QMouseEvent& event) {
  assert(managers_.draft != nullptr);
  if (!is_pressed_) {
    return false;
  }
  is_pressed_ = false;
  const bool was_drag = drag_ != nullptr;
  if (was_drag) {
    drag_.reset();
    view_->unsetCursor();
  } else if (is_narrowing_) {
    managers_.draft->Pick(Shot(), {press_frame_},
                          *ClickMode(event.modifiers(), false));
  }
  is_narrowing_ = false;
  view_->update();
  return true;
}

bool TimelineDraft::DoubleClick(const QMouseEvent& event) {
  assert(managers_.draft != nullptr);
  if (!IsInRow(event.position()) || event.position().x() < kNameWidth) {
    return IsInRow(event.position());
  }
  const auto frame = view_->FrameAt(event.position().x());
  const Draft* draft = managers_.draft->Of(Shot());
  if (!frame || draft == nullptr) {
    return true;
  }
  Seek(*frame);
  // On a cel's own frame it draws over it; elsewhere a new cel starts.
  RequestDraft(managers_, CelStartingAt(*draft, *frame) != nullptr);
  return true;
}

bool TimelineDraft::Menu(const QContextMenuEvent& event) {
  assert(managers_.draft != nullptr);
  if (!IsInRow(event.pos())) {
    return false;
  }
  const ShotId shot = Shot();
  const auto frame = view_->FrameAt(std::max(event.pos().x(), kNameWidth));
  const auto cel = CelNear(event.pos().x());
  if (cel && !managers_.draft->Picked(shot).contains(*cel)) {
    managers_.draft->Pick(shot, {*cel}, PickMode::kReplace);
  }
  QMenu menu(view_);
  menu.setToolTipsVisible(true);
  if (frame) {
    Seek(*frame);
  }
  for (const bool is_over : {false, true}) {
    QAction* draw = menu.addAction(is_over ? tr("Draw over this cel")
                                           : tr("Draw a new cel here"));
    Explain(draw, WhyNoDraftHere(managers_, is_over));
    connect(draw, &QAction::triggered, view_,
            [this, is_over] { RequestDraft(managers_, is_over); });
  }
  menu.addSeparator();
  const Draft* draft = managers_.draft->Of(shot);
  QAction* eye = menu.addAction(draft->is_shown ? tr("Hide draft")
                                                : tr("Show draft"));
  const bool is_shown = draft->is_shown;
  connect(eye, &QAction::triggered, view_, [this, shot, is_shown] {
    Report(ProblemOf(managers_.draft->SetShown(shot, !is_shown)));
  });
  QAction* remove = menu.addAction(tr("Delete picked cels"));
  remove->setShortcut(QKeySequence(Qt::Key_Delete));
  Explain(remove, managers_.draft->WhyNoDelete(shot));
  connect(remove, &QAction::triggered, view_, [this, shot] {
    Report(ProblemOf(
        managers_.draft->DeleteCels(shot, managers_.draft->Picked(shot))));
  });
  menu.exec(event.globalPos());
  return true;
}

bool TimelineDraft::Key(const QKeyEvent& event) {
  assert(managers_.draft != nullptr);
  const ShotId shot = Shot();
  if (event.key() == Qt::Key_Escape && drag_ != nullptr) {
    drag_->Cancel();
    drag_.reset();
    is_pressed_ = false;
    view_->unsetCursor();
    managers_.draft->Pick(shot, origin_, PickMode::kReplace);
    return true;
  }
  const std::set<Frame> picked = managers_.draft->Picked(shot);
  if (picked.empty()) {
    return false;
  }
  const bool is_delete =
      event.key() == Qt::Key_Delete || event.key() == Qt::Key_Backspace;
  if (is_delete) {
    Report(ProblemOf(managers_.draft->DeleteCels(shot, picked)));
    return true;
  }
  if (event.key() == Qt::Key_Escape) {
    managers_.draft->ClearPick();
    return true;
  }
  return false;
}

void TimelineDraft::Seek(Frame frame) {
  assert(managers_.playback != nullptr);
  const Project& project = managers_.history->current();
  const int index = ShotIndex(project, Shot());
  if (index < 0) {
    return;
  }
  const ::snapper::Shot& shot = *project.shots[static_cast<size_t>(index)];
  const int inside =
      std::clamp(frame.index(), 0, std::max(shot.length.index() - 1, 0));
  managers_.playback->Seek(Frame(ShotStart(project, index).index() + inside));
}

void TimelineDraft::Report(const QString& problem) const {
  assert(managers_.toasts != nullptr);
  if (!problem.isEmpty()) {
    managers_.toasts->Problem(problem);
  }
}

}  // namespace snapper
