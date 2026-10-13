#include "ui/comp_timeline.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/reel_timeline.h"
#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kNameWidth = 130.0;
constexpr double kRuler = 18.0;
constexpr double kRow = 18.0;
// The ruler, the row of shot clips and the shot light.
constexpr int kFixedRows = 2;

// The clip of shot that the effects are drawn over: the one showing on
// top at the playhead when it is the shot's, else the shot's first.
const Clip* ClipOfShot(const Project& project, ShotId shot, Frame playhead) {
  assert(playhead.index() >= 0);
  assert(shot.value() >= 0);
  const ReelShot here = ShotAtReel(project, playhead);
  const bool is_here = here.IsValid() && here.shot == shot;
  if (is_here) {
    return here.clip;
  }
  for (const ReelTrack& track : project.reel.tracks) {
    for (const Clip& clip : track.clips) {
      const auto* source = std::get_if<ShotSource>(&clip.source);
      if (source != nullptr && source->shot == shot) {
        return &clip;
      }
    }
  }
  return nullptr;
}

}  // namespace

CompTimeline::CompTimeline(const Managers& managers) : managers_(managers) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  setMinimumHeight(static_cast<int>(kRuler + 3 * kRow));
  const auto repaint = [this] { update(); };
  connect(managers_.history, &HistoryManager::Changed, this, [this] {
    const size_t rows = managers_.comp->Of(managers_.selection->shot()).size();
    setMinimumHeight(static_cast<int>(
        kRuler + (static_cast<double>(rows) + kFixedRows) * kRow + 4));
    update();
  });
  connect(managers_.comp, &CompManager::Changed, this, repaint);
  connect(managers_.selection, &SelectionManager::Changed, this, repaint);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, repaint);
}

int CompTimeline::Length() const {
  const Frame length = ReelLength(managers_.history->current());
  return std::max(1, length.index());
}

double CompTimeline::XOf(int frame) const {
  const double span = std::max(1.0, width() - kNameWidth - 8.0);
  return kNameWidth + span * frame / Length();
}

int CompTimeline::FrameAt(double x) const {
  const double span = std::max(1.0, width() - kNameWidth - 8.0);
  const int frame =
      static_cast<int>(std::floor((x - kNameWidth) / span * Length()));
  return std::clamp(frame, 0, Length() - 1);
}

void CompTimeline::SeekTo(double x) {
  assert(std::isfinite(x));
  managers_.playback->Seek(Frame(FrameAt(x)));
}

void CompTimeline::PaintShots(QPainter* painter, double y) const {
  assert(painter != nullptr);
  const Project& project = managers_.history->current();
  const ShotId picked = managers_.selection->shot();
  painter->fillRect(QRectF(0, y, width(), kRow), QColor(0x2e, 0x2e, 0x32));
  painter->setPen(QColor(0xe6, 0xe6, 0xe6));
  painter->drawText(QRectF(4, y, kNameWidth - 8, kRow),
                    Qt::AlignLeft | Qt::AlignVCenter, tr("Shots"));
  for (const ReelTrack& track : project.reel.tracks) {
    for (const Clip& clip : track.clips) {
      const auto* source = std::get_if<ShotSource>(&clip.source);
      if (source == nullptr) {
        continue;
      }
      const Shot* shot = FindShot(project, source->shot);
      const double left = XOf(clip.start.index());
      const double right = XOf(clip.end().index());
      const QRectF box(left, y + 1, std::max(2.0, right - left), kRow - 2);
      painter->setPen(Qt::black);
      painter->setBrush(source->shot == picked ? theme::kPick
                                               : QColor(0x4a, 0x4a, 0x52));
      painter->drawRect(box);
      painter->setPen(QColor(0xf2, 0xf2, 0xf2));
      painter->drawText(box.adjusted(3, 0, -2, 0),
                        Qt::AlignLeft | Qt::AlignVCenter,
                        shot != nullptr ? shot->name : QString());
    }
  }
}

void CompTimeline::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), QColor(0x24, 0x24, 0x27));
  const Project& project = managers_.history->current();
  const int length = Length();
  painter.setPen(QColor(0x80, 0x80, 0x86));
  const int every = std::max(1, length / 12);
  for (int frame = 0; frame < length; frame += every) {
    const double x = XOf(frame);
    painter.drawLine(QPointF(x, kRuler - 5), QPointF(x, kRuler));
    painter.drawText(QPointF(x + 2, kRuler - 6), QString::number(frame));
  }
  PaintShots(&painter, kRuler);
  const ShotId shot_id = managers_.selection->shot();
  const Shot* shot = FindShot(project, shot_id);
  const Frame playhead = managers_.playback->FrameOn(Timeline::kReel);
  const Clip* clip = shot != nullptr
                         ? ClipOfShot(project, shot_id, playhead)
                         : nullptr;
  if (clip != nullptr) {
    PaintEffects(&painter, *shot, *clip, kRuler + kRow);
  }
  const double x = XOf(playhead.index());
  painter.setPen(QPen(theme::kPick, 2.0));
  painter.drawLine(QPointF(x, 0), QPointF(x, height()));
}

void CompTimeline::PaintEffects(QPainter* painter, const Shot& shot,
                                const Clip& clip, double top) const {
  assert(painter != nullptr);
  std::vector<CompEffect> rows = shot.comp;
  std::ranges::reverse(rows);
  const CompEffectId picked = managers_.comp->PickedIn(shot.id);
  // A shot frame f shows at reel frame start + f - in, while the clip
  // shows it.
  const int shown = ShownLength(managers_.history->current(), clip).index();
  const auto reel_x = [&](int frame, bool* is_shown) {
    const int into = frame - clip.in.index();
    *is_shown = into >= 0 && into < shown;
    return XOf(clip.start.index() + into);
  };
  double y = top;
  const auto row_of = [&](const QString& name, const std::set<int>& keys,
                          bool is_picked) {
    painter->fillRect(QRectF(0, y, width(), kRow),
                      is_picked ? QColor(0x3a, 0x2a, 0x30)
                                : QColor(0x2e, 0x2e, 0x32));
    painter->setPen(QColor(0xe6, 0xe6, 0xe6));
    painter->drawText(QRectF(4, y, kNameWidth - 8, kRow),
                      Qt::AlignLeft | Qt::AlignVCenter, name);
    painter->setPen(Qt::black);
    painter->setBrush(theme::kHandle);
    for (const int frame : keys) {
      bool is_shown = false;
      const QPointF at(reel_x(frame, &is_shown), y + kRow / 2.0);
      if (!is_shown) {
        continue;
      }
      painter->drawPolygon(
          QPolygonF({at + QPointF(0, -5), at + QPointF(5, 0),
                     at + QPointF(0, 5), at + QPointF(-5, 0)}));
    }
    y += kRow;
  };
  for (const CompEffect& effect : rows) {
    row_of(CompManager::EffectName(effect), CompManager::KeyFrames(effect),
           effect.id == picked);
  }
  std::set<int> light;
  const bool is_light_keyed = shot.light.keys.size() > 1;
  if (is_light_keyed) {
    for (const auto& key : shot.light.keys) {
      light.insert(key.frame.index());
    }
  }
  row_of(tr("Shot light"), light, false);
}

void CompTimeline::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const double y = event->position().y();
  const int row = static_cast<int>(std::floor((y - kRuler) / kRow)) -
                  (kFixedRows - 1);
  const ShotId shot = managers_.selection->shot();
  std::vector<CompEffect> rows = managers_.comp->Of(shot);
  std::ranges::reverse(rows);
  const bool is_effect_row =
      y >= kRuler && row >= 0 && row < static_cast<int>(rows.size());
  if (is_effect_row) {
    managers_.comp->Pick(shot, rows[static_cast<size_t>(row)].id);
  }
  const bool is_on_frames = event->position().x() >= kNameWidth;
  if (is_on_frames) {
    SeekTo(event->position().x());
  }
}

void CompTimeline::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_held = event->buttons().testFlag(Qt::LeftButton);
  if (is_held) {
    SeekTo(event->position().x());
  }
}

}  // namespace snapper
