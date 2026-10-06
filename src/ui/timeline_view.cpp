#include "ui/timeline_view.h"

#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/master_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "media/audio_clip.h"
#include "ui/playhead.h"
#include "ui/theme.h"
#include "ui/timeline_layout.h"

namespace snapper {

TimelineView::TimelineView(const Managers& managers)
    : managers_(managers), step_mode_(this),
      frame_width_(kDefaultFrameWidth) {
  assert(managers_.IsComplete());
  step_mode_.setObjectName("step_mode");
  step_mode_.setCheckable(true);
  step_mode_.setFocusPolicy(Qt::NoFocus);
  step_mode_.setGeometry(1, 1, kNameWidth - 2, kRulerHeight - 2);
  connect(&step_mode_, &QToolButton::clicked, this, [this](bool is_on) {
    managers_.playback->SetStepMode(is_on ? StepMode::kAnimation
                                          : StepMode::kScrub);
  });
  connect(managers_.playback, &PlaybackManager::StepModeChanged, this,
          &TimelineView::ShowStepMode);
  ShowStepMode();
  setFocusPolicy(Qt::StrongFocus);
  setMinimumHeight(kRulerHeight + kWaveHeight + 3 * kRowHeight);
  const auto update_me = [this] { update(); };
  connect(managers_.history, &HistoryManager::Changed, this, update_me);
  connect(managers_.selection, &SelectionManager::Changed, this, update_me);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          update_me);
  connect(managers_.playback, &PlaybackManager::SongChanged, this,
          update_me);
  assert(frame_width_ > 0.0);
}

void TimelineView::ShowStepMode() {
  assert(managers_.playback != nullptr);
  const bool is_animation =
      managers_.playback->step_mode() == StepMode::kAnimation;
  step_mode_.setChecked(is_animation);
  step_mode_.setText(is_animation ? tr("Animation mode") : tr("Scrub mode"));
  step_mode_.setToolTip(
      is_animation
          ? tr("The arrow keys jump to the next frame where something "
               "changes, skipping held frames. Click for one frame at a "
               "time.")
          : tr("The arrow keys move one frame at a time. Click to jump "
               "between frames where something changes."));
  assert(step_mode_.isCheckable());
}

std::optional<Frame> TimelineView::FrameAt(double x) const {
  assert(frame_width_ > 0.0);
  assert(std::isfinite(x));
  const bool is_frames = x >= kNameWidth;
  if (!is_frames) {
    return std::nullopt;
  }
  const double index = (x - kNameWidth) / frame_width_ + scroll_;
  return Frame(static_cast<int>(std::floor(index)));
}

double TimelineView::XOf(Frame frame) const {
  assert(frame_width_ > 0.0);
  assert(frame.index() >= 0);
  return kNameWidth + (frame.index() - scroll_) * frame_width_;
}

void TimelineView::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(managers_.selection != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), theme::kField);
  const auto rows =
      TimelineRows(managers_.history->current(), managers_.selection->shot());
  const bool has_shot = !rows.empty();
  if (!has_shot) {
    painter.setPen(theme::kTextOff);
    painter.drawText(rect(), Qt::AlignCenter, tr("No shot picked."));
    return;
  }
  PaintRuler(&painter);
  PaintWave(&painter);
  PaintGrid(&painter);
  PaintRows(&painter, rows);
  PaintPlayhead(&painter);
  if (is_boxing_) {
    painter.setPen(QPen(theme::kPick, 1.0, Qt::DashLine));
    painter.setBrush(QColor(theme::kPick.red(), theme::kPick.green(),
                            theme::kPick.blue(), 40));
    painter.drawRect(QRectF(box_from_, box_to_).normalized());
  }
}

int TimelineView::LabelStep() const {
  assert(frame_width_ > 0.0);
  assert(kMinLabelGap > 0.0);
  // The smallest step that keeps numbers apart, on counts animators
  // think in.
  for (const int step : kLabelSteps) {
    const bool is_roomy = step * frame_width_ >= kMinLabelGap;
    if (is_roomy) {
      return step;
    }
  }
  return kLabelSteps.back();
}

void TimelineView::PaintRuler(QPainter* painter) const {
  assert(painter != nullptr);
  const Shot* shot =
      FindShot(managers_.history->current(), managers_.selection->shot());
  assert(shot != nullptr);
  painter->fillRect(QRectF(0, 0, width(), kRulerHeight), theme::kFace);
  // Beyond the shot's end is dimmed: keys there never play.
  const double end = XOf(shot->length);
  painter->fillRect(QRectF(end, 0, width() - end, height()),
                    theme::kFaceDark);
  const int first = std::max(0, FrameAt(kNameWidth)->index());
  const int last = FrameAt(width())->index();
  const int step = LabelStep();
  for (int f = first; f <= last; ++f) {
    const double x = XOf(Frame(f));
    const bool is_second = f % kFramesPerSecond == 0;
    const bool is_two = f % 2 == 0;
    painter->setPen(is_second ? theme::kText : theme::kTextOff);
    const double tick = is_second ? kRulerHeight : (is_two ? 6.0 : 3.0);
    painter->drawLine(QPointF(x, kRulerHeight - tick),
                      QPointF(x, kRulerHeight));
    const bool has_label = f % step == 0;
    if (has_label) {
      // Numbers sit over the frame's middle, where its keys are.
      painter->drawText(QRectF(x + frame_width_ / 2.0 - kMinLabelGap / 2.0,
                               0, kMinLabelGap, kRulerHeight - 4),
                        Qt::AlignHCenter | Qt::AlignVCenter,
                        QString::number(f));
    }
  }
}

void TimelineView::PaintGrid(QPainter* painter) const {
  assert(painter != nullptr);
  assert(frame_width_ > 0.0);
  const int first = std::max(0, FrameAt(kNameWidth)->index());
  const int last = FrameAt(width())->index();
  const int step = LabelStep();
  const double top = kRulerHeight + kWaveHeight;
  for (int f = first; f <= last; ++f) {
    const bool is_second = f % kFramesPerSecond == 0;
    const bool is_labelled = f % step == 0;
    // Every frame faintly when there is room, numbered frames a little
    // more, seconds most.
    const bool is_drawn =
        is_second || is_labelled || frame_width_ >= kMinGridWidth;
    if (!is_drawn) {
      continue;
    }
    painter->setPen(is_second     ? kGridSecond
                    : is_labelled ? kGridLabel
                                  : kGridFrame);
    const double x = XOf(Frame(f));
    painter->drawLine(QPointF(x, top), QPointF(x, height()));
  }
}

void TimelineView::PaintWave(QPainter* painter) const {
  assert(painter != nullptr);
  const auto song = managers_.playback->song();
  const Project& project = managers_.history->current();
  const int index = ShotIndex(project, managers_.selection->shot());
  const bool has_wave = song != nullptr && index >= 0;
  const QRectF band(kNameWidth, kRulerHeight, width() - kNameWidth,
                    kWaveHeight);
  painter->fillRect(band, theme::kFaceDark);
  painter->fillRect(QRectF(0, kRulerHeight, kNameWidth, kWaveHeight),
                    theme::kFace);
  painter->setPen(theme::kTextOff);
  painter->drawText(QRectF(6, kRulerHeight, kNameWidth - 8, kWaveHeight),
                    Qt::AlignVCenter | Qt::AlignLeft,
                    has_wave ? tr("Song") : tr("No song"));
  if (!has_wave) {
    return;
  }
  const double start = SecondsAtFrame(ShotStart(project, index));
  const double per_pixel = 1.0 / (frame_width_ * kFramesPerSecond);
  const double middle = band.center().y();
  painter->setPen(theme::kPick.darker(130));
  for (int x = kNameWidth; x < width(); ++x) {
    const double at = start + (scroll_ + (x - kNameWidth) / frame_width_) /
                                  kFramesPerSecond;
    const double half =
        PeakBetween(*song, at, at + per_pixel) * (kWaveHeight / 2.0 - 1.0);
    painter->drawLine(QPointF(x, middle - half), QPointF(x, middle + half));
  }
}

void TimelineView::PaintPlayhead(QPainter* painter) const {
  assert(painter != nullptr);
  assert(frame_width_ > 0.0);
  const auto here = PlayheadHere();
  if (!here) {
    return;
  }
  const double x = XOf(*here) + frame_width_ / 2.0;
  painter->setPen(QPen(theme::kPick, 2.0));
  painter->drawLine(QPointF(x, 0), QPointF(x, height()));
  // The frame the playhead is on, in a tag on the ruler.
  const QString number = QString::number(here->index());
  const double tag_width =
      painter->fontMetrics().horizontalAdvance(number) + 8.0;
  const QRectF tag(x - tag_width / 2.0, 0, tag_width, kRulerHeight);
  painter->fillRect(tag, theme::kPick);
  painter->setPen(theme::kText);
  painter->drawText(tag, Qt::AlignCenter, number);
}

std::optional<Frame> TimelineView::PlayheadHere() const {
  assert(managers_.selection != nullptr);
  const auto spot = SpotOf(managers_);
  const bool is_here = spot && spot->shot == managers_.selection->shot();
  assert(!is_here || spot->local.index() >= 0);
  return is_here ? std::optional<Frame>(spot->local) : std::nullopt;
}

int TimelineView::RowAt(double y, size_t count) const {
  assert(std::isfinite(y));
  assert(count < 100000);
  const double top = kRulerHeight + kWaveHeight;
  const int row = static_cast<int>(std::floor((y - top) / kRowHeight));
  const bool is_row = y >= top && row < static_cast<int>(count);
  return is_row ? row : -1;
}

std::optional<Frame> TimelineView::KeyNear(const TimelineRow& row,
                                           double x) const {
  assert(std::isfinite(x));
  assert(row.keys.size() == row.is_eased.size());
  const double reach = std::max(frame_width_ / 2.0, kKeyReach);
  for (const Frame key : row.keys) {
    const bool is_near =
        std::abs(XOf(key) + frame_width_ / 2.0 - x) <= reach;
    if (is_near) {
      return key;
    }
  }
  return std::nullopt;
}

}  // namespace snapper
