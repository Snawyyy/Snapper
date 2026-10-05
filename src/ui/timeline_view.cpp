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
#include "ui/theme.h"
#include "ui/timeline_layout.h"

namespace snapper {

TimelineView::TimelineView(const Managers& managers)
    : managers_(managers), frame_width_(kDefaultFrameWidth) {
  assert(managers_.IsComplete());
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
  PaintRows(&painter, rows);
  PaintPlayhead(&painter);
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
  for (int f = first; f <= last; ++f) {
    const double x = XOf(Frame(f));
    const bool is_second = f % kFramesPerSecond == 0;
    const bool is_two = f % 2 == 0;
    painter->setPen(is_second ? theme::kText : theme::kTextOff);
    const double tick = is_second ? kRulerHeight : (is_two ? 6.0 : 3.0);
    painter->drawLine(QPointF(x, kRulerHeight - tick),
                      QPointF(x, kRulerHeight));
    const bool has_label = is_second || (frame_width_ >= 14.0 && f % 6 == 0);
    if (has_label) {
      painter->drawText(QPointF(x + 2, kRulerHeight - 5),
                        QString::number(f));
    }
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
}

std::optional<Frame> TimelineView::PlayheadHere() const {
  const Project& project = managers_.history->current();
  const ShotMoment moment = Locate(project, managers_.playback->frame());
  const bool is_here =
      moment.shot >= 0 &&
      project.shots[static_cast<size_t>(moment.shot)]->id ==
          managers_.selection->shot();
  assert(managers_.selection != nullptr);
  return is_here ? std::optional<Frame>(moment.local) : std::nullopt;
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
