#include "ui/comp_timeline.h"

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
#include "ui/comp_drag.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kNameWidth = 130.0;
constexpr double kRuler = 18.0;
constexpr double kRow = 18.0;
// The ruler, the row of shot clips and the shot light.
constexpr int kFixedRows = 2;

}  // namespace

CompTimeline::CompTimeline(const Managers& managers) : managers_(managers) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(true);
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
  assert(managers_.history != nullptr);
  const Frame length = ReelLength(managers_.history->current());
  assert(length.index() >= 0);
  return std::max(1, length.index());
}

double CompTimeline::XOf(int frame) const {
  const double span = std::max(1.0, width() - kNameWidth - 8.0);
  assert(span > 0.0);
  assert(Length() > 0);
  return kNameWidth + span * frame / Length();
}

int CompTimeline::FreeFrameAt(double x) const {
  const double span = std::max(1.0, width() - kNameWidth - 8.0);
  assert(span > 0.0);
  assert(std::isfinite(x));
  return static_cast<int>(std::floor((x - kNameWidth) / span * Length()));
}

int CompTimeline::FrameAt(double x) const {
  assert(std::isfinite(x));
  assert(Length() > 0);
  return std::clamp(FreeFrameAt(x), 0, Length() - 1);
}

void CompTimeline::SeekTo(double x) {
  assert(std::isfinite(x));
  assert(managers_.playback != nullptr);
  managers_.playback->Seek(Frame(FrameAt(x)));
}

void CompTimeline::PaintShots(QPainter* painter, double y) const {
  assert(painter != nullptr);
  assert(y >= 0.0);
  const Project& project = managers_.history->current();
  const ShotId picked = managers_.selection->shot();
  painter->fillRect(QRectF(0, y, width(), kRow), QColor(0x2e, 0x2e, 0x32));
  painter->setPen(QColor(0xe6, 0xe6, 0xe6));
  painter->drawText(QRectF(4, y, kNameWidth - 8, kRow),
                    Qt::AlignLeft | Qt::AlignVCenter, tr("Shots"));
  for (const ReelTrack& track : project.reel.tracks) {
    for (const Clip& clip : track.clips) {
      const auto* source = std::get_if<ShotSource>(&clip.source);
      const bool is_shot = source != nullptr;
      if (!is_shot) {
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
  assert(managers_.playback != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), QColor(0x24, 0x24, 0x27));
  const int length = Length();
  painter.setPen(QColor(0x80, 0x80, 0x86));
  const int every = std::max(1, length / 12);
  for (int frame = 0; frame < length; frame += every) {
    const double x = XOf(frame);
    painter.drawLine(QPointF(x, kRuler - 5), QPointF(x, kRuler));
    painter.drawText(QPointF(x + 2, kRuler - 6), QString::number(frame));
  }
  PaintShots(&painter, kRuler);
  const bool has_clip = ShotClip() != nullptr;
  if (has_clip) {
    PaintBars(&painter);
    PaintLight(&painter);
  }
  const Frame playhead = managers_.playback->FrameOn(Timeline::kReel);
  const double x = XOf(playhead.index());
  painter.setPen(QPen(theme::kPick, 2.0));
  painter.drawLine(QPointF(x, 0), QPointF(x, height()));
}

void CompTimeline::PaintBars(QPainter* painter) const {
  assert(painter != nullptr);
  const ShotId shot = managers_.selection->shot();
  const CompEffectId picked = managers_.comp->PickedIn(shot);
  const Clip* clip = ShotClip();
  assert(clip != nullptr);
  const QRectF own(XOf(clip->start.index()), 0,
                   XOf(clip->end().index()) - XOf(clip->start.index()),
                   height());
  for (const Bar& bar : Bars()) {
    const CompEffect* effect = managers_.comp->Find(shot, bar.id);
    assert(effect != nullptr);
    const bool is_picked = bar.id == picked;
    const double y = bar.box.top() - 1;
    painter->fillRect(QRectF(0, y, width(), kRow),
                      is_picked ? QColor(0x3a, 0x2a, 0x30)
                                : QColor(0x2e, 0x2e, 0x32));
    painter->setPen(effect->is_on ? QColor(0xe6, 0xe6, 0xe6) : theme::kTextOff);
    painter->drawText(QRectF(4, y, kNameWidth - 8, kRow),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      CompManager::EffectName(*effect));
    QColor fill = KindMark(effect->kind).darker(effect->is_on ? 170 : 260);
    painter->setPen(is_picked ? QPen(theme::kPick, 2.0) : QPen(Qt::black));
    painter->setBrush(fill);
    painter->drawRect(bar.box);
    // On a doll it stops at the cut: what lies past it is hatched.
    const bool is_cut = bar.is_on_doll;
    if (is_cut) {
      painter->save();
      painter->setClipRegion(QRegion(bar.box.toAlignedRect()) -
                             QRegion(own.toAlignedRect()));
      painter->fillRect(bar.box, QBrush(QColor(0, 0, 0, 160),
                                        Qt::BDiagPattern));
      painter->restore();
    }
    painter->setPen(Qt::black);
    painter->setBrush(theme::kHandle);
    for (const int frame : CompManager::KeyFrames(*effect)) {
      const double x =
          XOf(clip->start.index() + frame - clip->in.index());
      const QPointF at(x, bar.box.center().y());
      painter->drawPolygon(
          QPolygonF({at + QPointF(0, -5), at + QPointF(5, 0),
                     at + QPointF(0, 5), at + QPointF(-5, 0)}));
    }
  }
}

void CompTimeline::PaintLight(QPainter* painter) const {
  assert(painter != nullptr);
  const Shot* shot =
      FindShot(managers_.history->current(), managers_.selection->shot());
  const Clip* clip = ShotClip();
  assert(clip != nullptr);
  const double y =
      kRuler + kRow * (1 + static_cast<double>(Bars().size()));
  painter->fillRect(QRectF(0, y, width(), kRow), QColor(0x2e, 0x2e, 0x32));
  painter->setPen(QColor(0xe6, 0xe6, 0xe6));
  painter->drawText(QRectF(4, y, kNameWidth - 8, kRow),
                    Qt::AlignLeft | Qt::AlignVCenter, tr("Shot light"));
  const bool is_keyed = shot != nullptr && shot->light.keys.size() > 1;
  if (!is_keyed) {
    return;
  }
  painter->setPen(Qt::black);
  painter->setBrush(theme::kHandle);
  for (const auto& key : shot->light.keys) {
    const QPointF at(
        XOf(clip->start.index() + key.frame.index() - clip->in.index()),
        y + kRow / 2.0);
    painter->drawPolygon(QPolygonF({at + QPointF(0, -5), at + QPointF(5, 0),
                                    at + QPointF(0, 5),
                                    at + QPointF(-5, 0)}));
  }
}

}  // namespace snapper
