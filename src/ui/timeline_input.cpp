// TimelineView's input: picking, sliding, menus, keys and the wheel.

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cassert>

#include "anim/master_timeline.h"
#include "anim/sampler.h"
#include "edit/history_manager.h"
#include "edit/key_manager.h"
#include "edit/playback_manager.h"
#include "edit/pose_manager.h"
#include "edit/selection_manager.h"
#include "ui/timeline_layout.h"
#include "ui/timeline_view.h"

namespace snapper {
namespace {

struct EaseChoice final {
  const char* label;
  Ease ease;
};

constexpr std::array<EaseChoice, kEaseCount> kEases = {{
    {"Hold (step)", Ease::kStep},
    {"Linear", Ease::kLinear},
    {"Ease in", Ease::kEaseIn},
    {"Ease out", Ease::kEaseOut},
    {"Ease in and out", Ease::kEaseInOut},
}};

constexpr int kWheelUnit = 120;

}  // namespace

void TimelineView::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.selection != nullptr);
  const QPointF at = event->position();
  const auto frame = FrameAt(at.x());
  const bool is_usable =
      frame.has_value() && event->button() == Qt::LeftButton;
  if (!is_usable) {
    return;
  }
  const bool is_ruler = at.y() < kRulerHeight + kWaveHeight;
  if (is_ruler) {
    is_scrubbing_ = true;
    Seek(*frame);
    return;
  }
  const Project& project = managers_.history->current();
  const auto rows = TimelineRows(project, managers_.selection->shot());
  const int index = RowAt(at.y(), rows.size());
  const bool is_row = index >= 0;
  if (!is_row) {
    managers_.selection->ClearKeys();
    return;
  }
  const TimelineRow& row = rows[static_cast<size_t>(index)];
  const bool is_layer = row.layer.IsValid();
  if (is_layer) {
    managers_.selection->SelectLayer(row.layer);
  }
  const auto key = KeyNear(row, at.x());
  const bool is_shift = event->modifiers().testFlag(Qt::ShiftModifier);
  if (!key) {
    managers_.selection->ClearKeys();
    Seek(*frame);
    return;
  }
  managers_.selection->SelectKeys(RowKeysAt(project, row, *key), is_shift);
  Seek(*key);
  key_drag_ = std::make_unique<EditScope>(managers_.history, tr("Move keys"));
  drag_frame_ = *key;
}

void TimelineView::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const auto frame = FrameAt(std::max<double>(event->position().x(),
                                              kNameWidth));
  assert(frame.has_value());
  if (is_scrubbing_) {
    Seek(*frame);
    return;
  }
  const int step = frame->index() - drag_frame_.index();
  const bool is_sliding = key_drag_ != nullptr && step != 0;
  if (!is_sliding) {
    return;
  }
  const auto moved = managers_.keys->Shift(managers_.selection->keys(), step);
  if (moved) {
    drag_frame_ = *frame;
    managers_.selection->SelectKeys(*moved, false);
  } else {
    emit Problem(moved.error().message);
  }
}

void TimelineView::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  is_scrubbing_ = false;
  key_drag_.reset();
}

void TimelineView::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const Project& project = managers_.history->current();
  const ShotId shot = managers_.selection->shot();
  const auto rows = TimelineRows(project, shot);
  const int index = RowAt(event->position().y(), rows.size());
  const auto frame = FrameAt(event->position().x());
  const bool is_usable = index >= 0 && frame.has_value();
  if (!is_usable) {
    return;
  }
  const TimelineRow& row = rows[static_cast<size_t>(index)];
  const bool is_camera = !row.layer.IsValid();
  if (is_camera) {
    const CameraPose now =
        Sample(FindShot(project, shot)->camera, *frame, CameraPose());
    Report(ProblemOf(managers_.pose->SetCamera(shot, *frame, now)));
  } else {
    Report(ProblemOf(managers_.pose->KeyInPlace(shot, row.layer, *frame)));
  }
}

void TimelineView::contextMenuEvent(QContextMenuEvent* event) {
  assert(event != nullptr);
  const Project& project = managers_.history->current();
  const auto rows = TimelineRows(project, managers_.selection->shot());
  const int index = RowAt(event->pos().y(), rows.size());
  const auto frame = FrameAt(event->pos().x());
  const bool is_usable = index >= 0 && frame.has_value();
  if (!is_usable) {
    return;
  }
  const TimelineRow& row = rows[static_cast<size_t>(index)];
  const auto key = KeyNear(row, event->pos().x());
  if (key) {
    const auto keys = RowKeysAt(project, row, *key);
    const bool is_picked = std::includes(
        managers_.selection->keys().begin(), managers_.selection->keys().end(),
        keys.begin(), keys.end());
    if (!is_picked) {
      managers_.selection->SelectKeys(keys, false);
    }
    KeyMenu(row, *key, event->globalPos());
  } else {
    EmptyMenu(row, *frame, event->globalPos());
  }
}

void TimelineView::KeyMenu(const TimelineRow& row, Frame frame,
                           QPoint where) {
  assert(!row.tracks.empty());
  assert(frame.index() >= 0);
  QMenu menu(this);
  QMenu& ease = *menu.addMenu(tr("Ease to next key"));
  for (const EaseChoice& choice : kEases) {
    connect(ease.addAction(tr(choice.label)), &QAction::triggered, this,
            [this, choice] {
              const auto& picked = managers_.selection->keys();
              Report(ProblemOf(managers_.keys->SetEase(picked, choice.ease)));
            });
  }
  connect(menu.addAction(tr("Copy keys")), &QAction::triggered, this,
          [this] {
            const auto& picked = managers_.selection->keys();
            Report(ProblemOf(managers_.keys->Copy(picked)));
          });
  connect(menu.addAction(tr("Delete keys")), &QAction::triggered, this,
          [this] {
            const auto& picked = managers_.selection->keys();
            Report(ProblemOf(managers_.keys->Remove(picked)));
          });
  menu.exec(where);
}

void TimelineView::EmptyMenu(const TimelineRow& row, Frame frame,
                             QPoint where) {
  assert(!row.tracks.empty());
  assert(frame.index() >= 0);
  QMenu menu(this);
  menu.setToolTipsVisible(true);
  const ShotId shot = managers_.selection->shot();
  QAction* paste = menu.addAction(tr("Paste keys here"));
  const QString why_not = managers_.keys->WhyNoPaste();
  paste->setEnabled(why_not.isEmpty());
  paste->setToolTip(why_not);
  connect(paste, &QAction::triggered, this, [this, shot, frame, row] {
    Report(ProblemOf(managers_.keys->Paste(shot, frame, row.layer)));
  });
  menu.addSeparator();
  connect(menu.addAction(tr("Hold one frame longer")), &QAction::triggered,
          this, [this, row, frame] {
            Report(ProblemOf(managers_.keys->Retime(row.tracks, frame, 1)));
          });
  connect(menu.addAction(tr("Hold one frame shorter")), &QAction::triggered,
          this, [this, row, frame] {
            Report(ProblemOf(managers_.keys->Retime(row.tracks, frame, -1)));
          });
  menu.exec(where);
}

void TimelineView::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const auto& picked = managers_.selection->keys();
  const bool is_delete = event->key() == Qt::Key_Delete ||
                         event->key() == Qt::Key_Backspace;
  const bool is_copy = event->matches(QKeySequence::Copy);
  const bool is_paste = event->matches(QKeySequence::Paste);
  const auto here = PlayheadHere();
  const bool can_paste_here = is_paste && here.has_value();
  if (is_delete) {
    Report(ProblemOf(managers_.keys->Remove(picked)));
  } else if (is_copy) {
    Report(ProblemOf(managers_.keys->Copy(picked)));
  } else if (can_paste_here) {
    Report(ProblemOf(managers_.keys->Paste(managers_.selection->shot(), *here,
                                 managers_.selection->layer())));
  } else {
    QWidget::keyPressEvent(event);
  }
}

void TimelineView::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  assert(frame_width_ > 0.0);
  const double notches = event->angleDelta().y() / double(kWheelUnit);
  const bool is_zoom = event->modifiers().testFlag(Qt::ControlModifier);
  if (is_zoom) {
    frame_width_ = std::clamp(frame_width_ * std::pow(1.25, notches),
                              kMinFrameWidth, kMaxFrameWidth);
  } else {
    scroll_ = std::max(0.0, scroll_ - notches * kScrollFrames);
  }
  event->accept();
  update();
}

void TimelineView::Seek(Frame local) {
  assert(local.index() >= 0);
  const Project& project = managers_.history->current();
  const int index = ShotIndex(project, managers_.selection->shot());
  const bool has_shot = index >= 0;
  if (has_shot) {
    const Shot& shot = *project.shots[static_cast<size_t>(index)];
    const Frame inside(std::min(local.index(), shot.length.index() - 1));
    managers_.playback->Seek(
        Frame(ShotStart(project, index).index() + inside.index()));
  }
}


}  // namespace snapper
