// The Pose timeline's storyboard rows: layout and paint (see
// timeline_storyboard.h; the mouse and keys are in
// timeline_storyboard_input.cpp).

#include "ui/timeline_storyboard.h"

#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/master_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reference_manager.h"
#include "edit/selection_manager.h"
#include "edit/storyboard_edits.h"
#include "edit/storyboard_manager.h"
#include "ui/grab.h"
#include "ui/paint_clip.h"
#include "ui/storyboard_actions.h"
#include "ui/theme.h"
#include "ui/timeline_eye.h"
#include "ui/timeline_layout.h"
#include "ui/timeline_view.h"
#include "ui/toast_manager.h"

namespace snapper {
namespace {

// A storyboard row is dashed and grey-green, so it never reads as a
// doll's key row or a reference bar.
constexpr QColor kBoardFill{140, 200, 150, 30};
constexpr QColor kBoardLine{150, 210, 160};

QPolygonF Diamond(QPointF center) {
  assert(std::isfinite(center.x()));
  assert(kKeySize > 0.0);
  return QPolygonF({center + QPointF(0, -kKeySize),
                    center + QPointF(kKeySize, 0),
                    center + QPointF(0, kKeySize),
                    center + QPointF(-kKeySize, 0)});
}

}  // namespace

TimelineStoryboard::TimelineStoryboard(const Managers& managers,
                                       TimelineView* view)
    : QObject(view), managers_(managers), view_(view) {
  assert(view_ != nullptr);
  assert(managers_.history != nullptr);
  const bool is_available = managers_.storyboard != nullptr;
  if (!is_available) {
    return;
  }
  view_->installEventFilter(this);
  connect(managers_.storyboard, &StoryboardManager::Changed, view_,
          [this] { view_->update(); });
}

TimelineStoryboard::~TimelineStoryboard() {
  assert(view_ != nullptr);
  assert(drag_ == nullptr || is_pressed_);
  // A drag cut short by closing lands nothing.
  const bool is_dragging = drag_ != nullptr;
  if (is_dragging) {
    drag_->Cancel();
  }
}

ShotId TimelineStoryboard::Shot() const {
  assert(managers_.selection != nullptr);
  assert(managers_.history != nullptr);
  return managers_.selection->shot();
}

std::vector<LayerId> TimelineStoryboard::Rows() const {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  std::vector<LayerId> rows;
  const bool is_available = managers_.storyboard != nullptr;
  const ::snapper::Shot* shot =
      is_available ? FindShot(managers_.history->current(), Shot()) : nullptr;
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return rows;
  }
  for (auto it = shot->layers.rbegin(); it != shot->layers.rend(); ++it) {
    const bool is_board = BoardOf(*it) != nullptr;
    if (is_board) {
      rows.push_back(it->id);
    }
  }
  return rows;
}

double TimelineStoryboard::Height() const {
  assert(kRowHeight > 0);
  assert(view_ != nullptr);
  return static_cast<double>(Rows().size()) * kRowHeight;
}

double TimelineStoryboard::Top() const {
  assert(managers_.selection != nullptr);
  const size_t references = managers_.references != nullptr
                                ? managers_.references->Of(Shot()).size()
                                : 0;
  assert(references <= static_cast<size_t>(kMaxReferencesPerShot));
  return kRulerHeight + kWaveHeight +
         static_cast<double>(references) * kRowHeight;
}

int TimelineStoryboard::RowAt(QPointF point) const {
  assert(std::isfinite(point.y()));
  assert(kRowHeight > 0);
  const double top = Top();
  const int count = static_cast<int>(Rows().size());
  const bool is_in_band =
      point.y() >= top && point.y() < top + count * kRowHeight;
  if (!is_in_band) {
    return -1;
  }
  return static_cast<int>(std::floor((point.y() - top) / kRowHeight));
}

LayerId TimelineStoryboard::LayerAt(QPointF point) const {
  assert(std::isfinite(point.x()));
  assert(std::isfinite(point.y()));
  const int row = RowAt(point);
  const std::vector<LayerId> rows = Rows();
  const bool is_row = row >= 0 && row < static_cast<int>(rows.size());
  return is_row ? rows[static_cast<size_t>(row)] : LayerId();
}

double TimelineStoryboard::FrameWidth() const {
  assert(view_ != nullptr);
  const double width = view_->XOf(Frame(1)) - view_->XOf(Frame(0));
  assert(width > 0.0);
  return width;
}

std::optional<Frame> TimelineStoryboard::CelNear(LayerId layer,
                                                 double x) const {
  assert(std::isfinite(x));
  assert(managers_.storyboard != nullptr);
  const StoryboardLayer* board = managers_.storyboard->Of(Shot(), layer);
  const bool has_board = board != nullptr;
  if (!has_board) {
    return std::nullopt;
  }
  const double half = FrameWidth() / 2.0;
  std::vector<QPointF> spots;
  for (const StoryboardCel& cel : board->cels) {
    spots.emplace_back(view_->XOf(cel.at) + half, 0.0);
  }
  const int nearest =
      NearestGrab(spots, QPointF(x, 0.0), std::max(half, kGrabReach));
  const bool is_hit = nearest >= 0;
  return is_hit ? std::optional<Frame>(
                      board->cels[static_cast<size_t>(nearest)].at)
                : std::nullopt;
}

void TimelineStoryboard::Paint(QPainter* painter) const {
  assert(painter != nullptr);
  assert(managers_.history != nullptr);
  const std::vector<LayerId> rows = Rows();
  const ::snapper::Shot* shot =
      FindShot(managers_.history->current(), Shot());
  const bool has_rows = !rows.empty() && shot != nullptr;
  if (!has_rows) {
    return;
  }
  double top = Top();
  for (const LayerId id : rows) {
    const Layer* layer = FindLayer(*shot, id);
    const bool is_layer = layer != nullptr;
    if (is_layer) {
      PaintRow(painter, *layer, top, shot->length);
    }
    top += kRowHeight;
  }
}

void TimelineStoryboard::PaintRow(QPainter* painter, const Layer& layer,
                                  double top, Frame shot_length) const {
  assert(painter != nullptr);
  const StoryboardLayer* board = BoardOf(layer);
  assert(board != nullptr);
  const bool is_shown = layer.is_visible;
  const QColor ink = is_shown ? theme::kText : theme::kTextOff;
  const double width = view_->width();
  const QRectF name(0, top, kNameWidth, kRowHeight);
  painter->save();
  painter->fillRect(name, theme::kFace);
  painter->setPen(ink);
  painter->drawText(
      name.adjusted(6, 0, -(kEyeSize + 10.0), 0),
      Qt::AlignVCenter | Qt::AlignLeft,
      painter->fontMetrics().elidedText(
          layer.name, Qt::ElideRight,
          static_cast<int>(kNameWidth - kEyeSize - 16.0)));
  PaintEye(EyeRect(top), is_shown, painter);
  {
    const PaintClip keep_in(
        painter, QRectF(kNameWidth, top, width - kNameWidth, kRowHeight));
    painter->setOpacity(is_shown ? 1.0 : 0.4);
    const double from = view_->XOf(Frame(0));
    const double to = view_->XOf(shot_length);
    painter->setBrush(kBoardFill);
    painter->setPen(QPen(kBoardLine, 1.0, Qt::DashLine));
    painter->drawRect(QRectF(from, top + 3.0, std::max(to - from, 2.0),
                             kRowHeight - 6.0));
    const double middle = top + kRowHeight / 2.0;
    const double half = FrameWidth() / 2.0;
    const std::set<Frame> picked =
        managers_.storyboard->Picked(Shot(), layer.id);
    for (size_t i = 0; i < board->cels.size(); ++i) {
      const StoryboardCel& cel = board->cels[i];
      const bool is_last = i + 1 == board->cels.size();
      const double x = view_->XOf(cel.at) + half;
      const double next = view_->XOf(board->cels[is_last ? i : i + 1].at);
      const double end = is_last ? std::max(to, x) : next + half;
      painter->setPen(QPen(kBoardLine, 2.0));
      painter->drawLine(QPointF(x, middle), QPointF(end, middle));
      const bool is_picked = picked.contains(cel.at);
      painter->setPen(QPen(is_picked ? theme::kText : theme::kShadow, 1.0));
      painter->setBrush(is_picked ? theme::kPick : kBoardLine);
      painter->drawPolygon(Diamond(QPointF(x, middle)));
    }
    painter->setOpacity(1.0);
  }
  painter->setPen(theme::kFaceDark);
  painter->drawLine(QPointF(0, top + kRowHeight),
                    QPointF(width, top + kRowHeight));
  painter->restore();
}

void TimelineStoryboard::Seek(Frame frame) {
  assert(managers_.playback != nullptr);
  assert(frame.index() >= 0);
  const Project& project = managers_.history->current();
  const int index = ShotIndex(project, Shot());
  const bool has_shot = index >= 0;
  if (!has_shot) {
    return;
  }
  const ::snapper::Shot& shot = *project.shots[static_cast<size_t>(index)];
  const int inside =
      std::clamp(frame.index(), 0, std::max(shot.length.index() - 1, 0));
  managers_.playback->Seek(Frame(ShotStart(project, index).index() + inside));
}

void TimelineStoryboard::Aim(LayerId layer) {
  assert(managers_.storyboard != nullptr);
  assert(layer.IsValid());
  managers_.storyboard->Pick(Shot(), layer, {}, PickMode::kReplace);
}

void TimelineStoryboard::Report(const QString& problem) const {
  assert(managers_.toasts != nullptr);
  assert(problem.size() < 100000);
  const bool has_problem = !problem.isEmpty();
  if (has_problem) {
    managers_.toasts->Problem(problem);
  }
}

}  // namespace snapper
