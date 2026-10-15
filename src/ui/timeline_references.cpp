// TimelineView's reference rows: one per reference of the shot, above
// the doll rows, never counted in the shot's length.

#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMimeData>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>
#include <vector>

#include "edit/history_manager.h"
#include "edit/reference_manager.h"
#include "edit/selection_manager.h"
#include "ui/deck_paint.h"
#include "ui/doll_cards.h"
#include "ui/form_helpers.h"
#include "ui/paint_clip.h"
#include "ui/pick_keys.h"
#include "ui/problem.h"
#include "ui/theme.h"
#include "ui/timeline_eye.h"
#include "ui/timeline_layout.h"
#include "ui/timeline_view.h"

namespace snapper {
namespace {

// A reference bar is blue and dashed, so it never reads as a key bar.
constexpr QColor kReferenceFill{90, 140, 255, 50};
constexpr QColor kReferenceLine{120, 170, 255};
// Its markers, faint down the doll rows to key against.
constexpr QColor kMarkAcross{255, 220, 80, 70};
double BandTop() { return kRulerHeight + kWaveHeight; }

}  // namespace

int TimelineView::ReferenceRows() const {
  assert(managers_.selection != nullptr);
  const bool has_manager = managers_.references != nullptr;
  if (!has_manager) {
    return 0;
  }
  const auto count =
      managers_.references->Of(managers_.selection->shot()).size();
  assert(count <= static_cast<size_t>(kMaxReferencesPerShot));
  return static_cast<int>(count);
}

double TimelineView::RowsTop() const {
  const double top =
      BandTop() + ReferenceRows() * kRowHeight + storyboard_.Height();
  assert(top >= BandTop());
  return top;
}

int TimelineView::ReferenceRowAt(double y) const {
  assert(std::isfinite(y));
  const int count = ReferenceRows();
  const bool is_in_band = y >= BandTop() && y < BandTop() + count * kRowHeight;
  if (!is_in_band) {
    return -1;
  }
  const int row = static_cast<int>(std::floor((y - BandTop()) / kRowHeight));
  assert(row >= 0 && row < count);
  return row;
}

void TimelineView::PaintReferences(QPainter* painter) const {
  assert(painter != nullptr);
  assert(FrameWidth() > 0.0);
  const bool has_manager = managers_.references != nullptr;
  if (!has_manager) {
    return;
  }
  const ShotId shot = managers_.selection->shot();
  const std::set<ReferenceId> picked = managers_.references->PicksIn(shot);
  const auto x_at = [this](double frame) { return scale_.XOf(frame); };
  double top = BandTop();
  painter->save();
  for (const Reference& reference : managers_.references->Of(shot)) {
    const bool is_picked = picked.contains(reference.id);
    const bool is_missing = ReferenceManager::IsMissing(reference);
    const QRectF name(0, top, kNameWidth, kRowHeight);
    painter->fillRect(name, deck::kPlastic);
    const QString file = QFileInfo(reference.path).fileName();
    deck::PaintLabel(
        painter, name.adjusted(0, 0, -(kEyeSize + 6.0), 0),
        is_missing ? tr("Missing: %1").arg(file) : tr("Ref %1").arg(file),
        is_picked, is_missing || !reference.is_shown);
    PaintEye(EyeRect(top), reference.is_shown, painter);
    {
      const PaintClip keep_in(painter, QRectF(kNameWidth, top,
                                              width() - kNameWidth,
                                              kRowHeight));
      const double from = x_at(reference.start.index());
      const double to = reference.IsStill()
                            ? width()
                            : x_at(ReferenceEndFrame(reference));
      const QRectF bar(from, top + 4.0, std::max(to - from, 2.0),
                       kRowHeight - 8.0);
      painter->setOpacity(reference.is_shown ? 1.0 : 0.4);
      painter->setBrush(kReferenceFill);
      painter->setPen(QPen(is_missing ? theme::kPick : kReferenceLine,
                           is_picked ? 1.6 : 1.0, Qt::DashLine));
      painter->drawRect(bar);
      painter->setPen(QPen(theme::kHandle, 1.5));
      for (const double mark : reference.marks) {
        const double x = x_at(ReferenceFrameOf(reference, mark) + 0.5);
        painter->drawLine(QPointF(x, top + 2.0),
                          QPointF(x, top + kRowHeight - 2.0));
      }
      painter->setOpacity(1.0);
    }
    top += kRowHeight;
  }
  painter->restore();
}

void TimelineView::PaintReferenceMarks(QPainter* painter) const {
  assert(painter != nullptr);
  assert(FrameWidth() > 0.0);
  const bool has_manager = managers_.references != nullptr;
  if (!has_manager) {
    return;
  }
  const PaintClip keep_in(painter, FrameArea(RowsTop()));
  painter->setPen(QPen(kMarkAcross, 1.0));
  for (const Reference& reference :
       managers_.references->Of(managers_.selection->shot())) {
    const bool is_seen =
        reference.is_shown && managers_.references->are_shown();
    for (const double mark : is_seen ? reference.marks
                                     : std::vector<double>()) {
      const double x =
          scale_.XOf(ReferenceFrameOf(reference, mark) + 0.5);
      painter->drawLine(QPointF(x, RowsTop()), QPointF(x, height()));
    }
  }
}

bool TimelineView::ReferencePress(const QMouseEvent& event) {
  assert(managers_.selection != nullptr);
  assert(reference_drag_ == nullptr);
  const int index = ReferenceRowAt(event.position().y());
  const bool is_row = index >= 0 && event.button() == Qt::LeftButton;
  if (!is_row) {
    return false;
  }
  const ShotId shot = managers_.selection->shot();
  const std::vector<Reference> references = managers_.references->Of(shot);
  const Reference& reference = references[static_cast<size_t>(index)];
  const double top = BandTop() + index * kRowHeight;
  const bool is_eye = EyeGrabRect(top).contains(event.position());
  if (is_eye) {
    Report(ProblemOf(managers_.references->SetShown(shot, reference.id,
                                                    !reference.is_shown)));
    return true;
  }
  // The pick rule (pick_keys.h). A drag moves only the row it grabs, so
  // a plain click on a picked row narrows the pick at once.
  const auto mode = ClickMode(
      event.modifiers(), managers_.references->IsPicked(shot, reference.id));
  const PickMode applied = mode.value_or(PickMode::kReplace);
  managers_.references->PickMany(shot, {reference.id}, applied,
                                 reference.id);
  const bool is_on_frames = event.position().x() >= kNameWidth;
  const bool drags = is_on_frames && applied == PickMode::kReplace;
  if (!drags) {
    return true;
  }
  is_reference_slide_ = event.modifiers().testFlag(Qt::AltModifier);
  reference_dragged_ = reference.id;
  reference_press_x_ = event.position().x();
  reference_start_ = reference.start;
  reference_skip_ = reference.skip;
  reference_drag_ = std::make_unique<EditScope>(
      managers_.history,
      is_reference_slide_ ? tr("Slide reference") : tr("Reference start"));
  setCursor(Qt::SizeHorCursor);
  return true;
}

bool TimelineView::ReferenceDragTo(QPointF at) {
  assert(std::isfinite(at.x()));
  assert(FrameWidth() > 0.0);
  const bool is_dragging = reference_drag_ != nullptr;
  if (!is_dragging) {
    return false;
  }
  const ShotId shot = managers_.selection->shot();
  const int frames =
      static_cast<int>(std::lround((at.x() - reference_press_x_) /
                                   FrameWidth()));
  const Reference* reference =
      managers_.references->Find(shot, reference_dragged_);
  const bool is_there = reference != nullptr;
  if (!is_there) {
    return true;
  }
  // Dragging the video right shows what was earlier under each frame.
  const auto done =
      is_reference_slide_
          ? managers_.references->SetSkip(
                shot, reference_dragged_,
                reference_skip_ - static_cast<double>(frames) /
                                      kFramesPerSecond * reference->speed)
          : managers_.references->SetStart(
                shot, reference_dragged_,
                Frame(reference_start_.index() + frames));
  Report(ProblemOf(done));
  update();
  return true;
}

bool TimelineView::EndReferenceDrag() {
  const bool is_dragging = reference_drag_ != nullptr;
  if (!is_dragging) {
    return false;
  }
  reference_drag_.reset();
  unsetCursor();
  update();
  assert(reference_drag_ == nullptr);
  return true;
}

bool TimelineView::CancelReferenceDrag() {
  const bool is_dragging = reference_drag_ != nullptr;
  if (!is_dragging) {
    return false;
  }
  reference_drag_->Cancel();
  reference_drag_.reset();
  unsetCursor();
  update();
  assert(reference_drag_ == nullptr);
  return true;
}

bool TimelineView::ReferenceMenu(QPoint at, QPoint where) {
  assert(managers_.selection != nullptr);
  assert(at.y() >= -100000);
  const int index = ReferenceRowAt(at.y());
  const bool is_row = index >= 0;
  if (!is_row) {
    return false;
  }
  const ShotId shot = managers_.selection->shot();
  const std::vector<Reference> references = managers_.references->Of(shot);
  const Reference& reference = references[static_cast<size_t>(index)];
  const ReferenceId id = reference.id;
  // A right click on an unpicked row picks it; on a picked one it keeps
  // the pick, so Remove takes all of it.
  const bool was_picked = managers_.references->IsPicked(shot, id);
  if (!was_picked) {
    managers_.references->Pick(shot, id);
  }
  const auto count = managers_.references->PicksIn(shot).size();
  QMenu menu(this);
  menu.setToolTipsVisible(true);
  const bool is_missing = ReferenceManager::IsMissing(reference);
  connect(menu.addAction(is_missing ? tr("Find file...")
                                    : tr("Change file...")),
          &QAction::triggered, this, [this, shot, id] {
            const QString path = QFileDialog::getOpenFileName(
                this, tr("Find the reference file"));
            const bool is_picked = !path.isEmpty();
            if (is_picked) {
              Report(ProblemOf(managers_.references->Relink(shot, id, path)));
            }
          });
  QAction* clear = menu.addAction(tr("Clear markers"));
  Explain(clear, reference.marks.empty() ? tr("It has no markers.")
                                         : QString());
  connect(clear, &QAction::triggered, this, [this, shot, id] {
    Report(ProblemOf(managers_.references->ClearMarks(shot, id)));
  });
  menu.addSeparator();
  connect(menu.addAction(count > 1 ? tr("Remove %1 references").arg(count)
                                   : tr("Remove reference")),
          &QAction::triggered, this, [this, shot] {
            Report(ProblemOf(managers_.references->RemovePicked(shot)));
          });
  menu.exec(where);
  return true;
}

void TimelineView::dragEnterEvent(QDragEnterEvent* event) {
  assert(event != nullptr);
  assert(event->mimeData() != nullptr);
  const bool is_files = managers_.references != nullptr &&
                        !FilesOfDrop(event->mimeData()).isEmpty();
  if (is_files) {
    event->acceptProposedAction();
  }
}

void TimelineView::dragMoveEvent(QDragMoveEvent* event) {
  assert(event != nullptr);
  assert(event->mimeData() != nullptr);
  // Only with Alt: the timeline takes files as references alone.
  const bool is_reference = managers_.references != nullptr &&
                            event->modifiers().testFlag(Qt::AltModifier);
  if (is_reference) {
    event->acceptProposedAction();
  } else {
    event->ignore();
  }
}

void TimelineView::dropEvent(QDropEvent* event) {
  assert(event != nullptr);
  assert(event->mimeData() != nullptr);
  const QStringList files = FilesOfDrop(event->mimeData());
  const bool is_reference = managers_.references != nullptr &&
                            event->modifiers().testFlag(Qt::AltModifier) &&
                            !files.isEmpty();
  if (!is_reference) {
    Report(tr("Hold Alt while dropping to add a reference here."));
    return;
  }
  event->acceptProposedAction();
  QStringList problems;
  for (const QString& file : files) {
    const auto added =
        managers_.references->Add(managers_.selection->shot(), file);
    if (!added) {
      problems.append(added.error().message);
    }
  }
  Report(problems.join(QStringLiteral(" ")));
}

}  // namespace snapper
