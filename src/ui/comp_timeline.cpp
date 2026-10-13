#include "ui/comp_timeline.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/master_timeline.h"
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

}  // namespace

CompTimeline::CompTimeline(const Managers& managers) : managers_(managers) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  setMinimumHeight(static_cast<int>(kRuler + 3 * kRow));
  const auto repaint = [this] { update(); };
  connect(managers_.history, &HistoryManager::Changed, this, [this] {
    const size_t rows = managers_.comp->Of(managers_.selection->shot()).size();
    setMinimumHeight(
        static_cast<int>(kRuler + (static_cast<double>(rows) + 1.0) * kRow + 4));
    update();
  });
  connect(managers_.comp, &CompManager::Changed, this, repaint);
  connect(managers_.selection, &SelectionManager::Changed, this, repaint);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, repaint);
}

int CompTimeline::Length() const {
  const Shot* shot =
      FindShot(managers_.history->current(), managers_.selection->shot());
  return shot != nullptr ? std::max(1, shot->length.index()) : 1;
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
  const Project& project = managers_.history->current();
  const int index = ShotIndex(project, managers_.selection->shot());
  const bool is_present = index >= 0;
  if (!is_present) {
    return;
  }
  managers_.playback->Seek(
      Frame(ShotStart(project, index).index() + FrameAt(x)));
}

void CompTimeline::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), QColor(0x24, 0x24, 0x27));
  const Project& project = managers_.history->current();
  const ShotId shot_id = managers_.selection->shot();
  const Shot* shot = FindShot(project, shot_id);
  if (shot == nullptr) {
    return;
  }
  const int length = Length();
  painter.setPen(QColor(0x80, 0x80, 0x86));
  const int every = std::max(1, length / 12);
  for (int frame = 0; frame < length; frame += every) {
    const double x = XOf(frame);
    painter.drawLine(QPointF(x, kRuler - 5), QPointF(x, kRuler));
    painter.drawText(QPointF(x + 2, kRuler - 6), QString::number(frame));
  }
  std::vector<CompEffect> rows = shot->comp;
  std::ranges::reverse(rows);
  const CompEffectId picked = managers_.comp->PickedIn(shot_id);
  double y = kRuler;
  const auto row_of = [&](const QString& name, const std::set<int>& keys,
                          bool is_picked) {
    const QRectF band(0, y, width(), kRow);
    painter.fillRect(band, is_picked ? QColor(0x3a, 0x2a, 0x30)
                                     : QColor(0x2e, 0x2e, 0x32));
    painter.setPen(QColor(0xe6, 0xe6, 0xe6));
    painter.drawText(QRectF(4, y, kNameWidth - 8, kRow),
                     Qt::AlignLeft | Qt::AlignVCenter, name);
    painter.setPen(Qt::black);
    painter.setBrush(theme::kHandle);
    for (const int frame : keys) {
      const QPointF at(XOf(frame), y + kRow / 2.0);
      painter.drawPolygon(QPolygonF({at + QPointF(0, -5), at + QPointF(5, 0),
                                     at + QPointF(0, 5), at + QPointF(-5, 0)}));
    }
    y += kRow;
  };
  for (const CompEffect& effect : rows) {
    row_of(CompManager::KindName(effect.kind), CompManager::KeyFrames(effect),
           effect.id == picked);
  }
  std::set<int> light;
  const bool is_light_keyed = shot->light.keys.size() > 1;
  if (is_light_keyed) {
    for (const auto& key : shot->light.keys) {
      light.insert(key.frame.index());
    }
  }
  row_of(tr("Shot light"), light, false);
  const ShotMoment moment =
      Locate(project, managers_.playback->FrameOn(Timeline::kShots));
  const bool is_here = moment.shot >= 0 &&
                       project.shots[static_cast<size_t>(moment.shot)]->id ==
                           shot_id;
  if (is_here) {
    const double x = XOf(moment.local.index());
    painter.setPen(QPen(theme::kPick, 2.0));
    painter.drawLine(QPointF(x, 0), QPointF(x, height()));
  }
}

void CompTimeline::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const double y = event->position().y();
  const int row = static_cast<int>(std::floor((y - kRuler) / kRow));
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
