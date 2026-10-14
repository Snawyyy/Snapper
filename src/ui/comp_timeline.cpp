#include "ui/comp_timeline.h"

#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "ui/comp_drag.h"
#include "ui/theme.h"
#include "ui/timeline_ruler.h"

namespace snapper {
namespace {

// The ruler, the row of shot clips and the shot light.
constexpr int kFixedRows = 2;
const QColor kRowColor(0x2e, 0x2e, 0x32);
const QColor kText(0xe6, 0xe6, 0xe6);

void PaintKey(QPainter* painter, QPointF at) {
  assert(painter != nullptr);
  assert(std::isfinite(at.x()));
  painter->drawPolygon(QPolygonF({at + QPointF(0, -5), at + QPointF(5, 0),
                                  at + QPointF(0, 5), at + QPointF(-5, 0)}));
}

}  // namespace

CompTimeline::CompTimeline(const Managers& managers)
    : managers_(managers),
      scale_(this, kNameWidth, kDefaultFrameWidth),
      step_mode_(managers.playback, this),
      settle_(managers.motion, managers.playback->FrameOn(Timeline::kReel),
              this),
      feedback_(managers.motion, this) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  step_mode_.setObjectName("step_mode");
  step_mode_.setGeometry(
      StepModeSwitch::CornerRect(static_cast<int>(kNameWidth)));
  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(true);
  setMinimumHeight(static_cast<int>(kRuler + 3 * kRow));
  scale_.SetLength(Length());
  scale_.FitAll();
  const auto repaint = [this] { update(); };
  connect(managers_.history, &HistoryManager::Changed, this, [this] {
    const size_t rows = Bars().size();
    setMinimumHeight(static_cast<int>(
        kRuler + (static_cast<double>(rows) + kFixedRows) * kRow + 4));
    scale_.SetLength(Length());
    update();
  });
  connect(managers_.comp, &CompManager::Changed, this, repaint);
  connect(managers_.selection, &SelectionManager::Changed, this, repaint);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, repaint);
  // The playhead overshoots a little as it lands on a frame.
  connect(&settle_, &PlayheadSettle::Changed, this, repaint);
  connect(&feedback_, &DragFeedback::Changed, this, repaint);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, [this] {
    settle_.MoveTo(managers_.playback->FrameOn(Timeline::kReel),
                   managers_.playback->IsPlaying());
  });
  // Playing pages the view along until it is scrolled by hand.
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          &CompTimeline::FollowPlayhead);
  connect(managers_.playback, &PlaybackManager::PlayingChanged, this,
          [this](bool is_playing) {
            if (is_playing) {
              scale_.BeginFollow();
            }
            FollowPlayhead();
          });
}

void CompTimeline::FollowPlayhead() {
  assert(managers_.playback != nullptr);
  assert(PixelsPerFrame() > 0.0);
  const bool is_playing_here = managers_.playback->IsPlaying() &&
                               managers_.playback->timeline() ==
                                   Timeline::kReel;
  const bool is_paged = is_playing_here && scale_.Follow(Playhead().index());
  if (is_paged) {
    update();
  }
}

int CompTimeline::Length() const {
  assert(managers_.history != nullptr);
  const Frame length = ReelLength(managers_.history->current());
  assert(length.index() >= 0);
  return std::max(1, length.index());
}

double CompTimeline::XOf(int frame) const {
  assert(PixelsPerFrame() > 0.0);
  assert(frame >= -kMaxFrame);
  return scale_.XOf(frame);
}

int CompTimeline::FreeFrameAt(double x) const {
  assert(std::isfinite(x));
  assert(PixelsPerFrame() > 0.0);
  return scale_.FrameAt(x);
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

void CompTimeline::PaintRuler(QPainter* painter) const {
  assert(painter != nullptr);
  assert(PixelsPerFrame() > 0.0);
  PaintFrameRuler(painter, scale_, width(), false);
  for (const Frame& marker : managers_.history->current().reel.markers) {
    const double x = XOf(marker.index());
    painter->fillRect(QRectF(x - 1, kRuler - 6, 3, 6), theme::kHandle);
  }
}

void CompTimeline::PaintShots(QPainter* painter, double y) const {
  assert(painter != nullptr);
  assert(y >= 0.0);
  const Project& project = managers_.history->current();
  const ShotId picked = managers_.selection->shot();
  painter->fillRect(QRectF(0, y, width(), kRow), kRowColor);
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
  painter.save();
  painter.setClipRect(QRectF(kNameWidth, 0, width() - kNameWidth, height()));
  PaintRuler(&painter);
  PaintShots(&painter, kRuler);
  PaintBars(&painter);
  const bool has_clip = ClipOf(managers_.selection->shot()) != nullptr;
  if (has_clip) {
    PaintLight(&painter);
  }
  PaintBox(&painter);
  const Frame now = managers_.playback->FrameOn(Timeline::kReel);
  PaintTimelinePlayhead(
      &painter, XOf(now.index()) + settle_.offset() * PixelsPerFrame(),
      now.index(), QRectF(kNameWidth, 0, width() - kNameWidth, height()));
  painter.restore();
  // The ruler's corner (the Scrub and Animation switch sits over it) and
  // the names column over what scrolled under it.
  painter.fillRect(QRectF(0, 0, kNameWidth, kRuler), theme::kFace);
  painter.fillRect(QRectF(0, kRuler, kNameWidth, height()), kRowColor);
  painter.setPen(kText);
  painter.drawText(QRectF(4, kRuler, kNameWidth - 8, kRow),
                   Qt::AlignLeft | Qt::AlignVCenter, tr("Shots"));
  const std::set<CompClipRef> picks = managers_.comp->ClipPicks();
  for (const Bar& bar : Bars()) {
    const CompEffect* effect = managers_.comp->Find(bar.ref.shot, bar.ref.id);
    assert(effect != nullptr);
    const double y = bar.box.top() - 1;
    const bool is_picked = picks.contains(bar.ref);
    painter.fillRect(QRectF(0, y, kNameWidth, kRow),
                     is_picked ? QColor(0x3a, 0x2a, 0x30) : kRowColor);
    painter.setPen(effect->is_on ? kText : theme::kTextOff);
    painter.drawText(QRectF(4, y, kNameWidth - 8, kRow),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     fontMetrics().elidedText(
                         managers_.comp->TargetTag(bar.ref.shot,
                                                   effect->target) +
                             QStringLiteral(" ") +
                             CompManager::EffectName(*effect),
                         Qt::ElideRight, static_cast<int>(kNameWidth - 8)));
  }
  const double light_y = kRuler + kRow * (1 + static_cast<double>(
                                                  Bars().size()));
  painter.setPen(kText);
  painter.drawText(QRectF(4, light_y, kNameWidth - 8, kRow),
                   Qt::AlignLeft | Qt::AlignVCenter, tr("Shot light"));
  feedback_.Paint(&painter, QRectF(rect()));
}

void CompTimeline::PaintBars(QPainter* painter) const {
  assert(painter != nullptr);
  assert(managers_.comp != nullptr);
  const std::set<CompClipRef> picks = managers_.comp->ClipPicks();
  for (const Bar& bar : Bars()) {
    const bool is_picked = picks.contains(bar.ref);
    painter->fillRect(QRectF(0, bar.box.top() - 1, width(), kRow),
                      is_picked ? QColor(0x3a, 0x2a, 0x30) : kRowColor);
    PaintBar(painter, bar, is_picked);
  }
}

void CompTimeline::PaintBar(QPainter* painter, const Bar& bar,
                            bool is_picked) const {
  assert(painter != nullptr);
  assert(bar.clip != nullptr);
  const CompEffect* effect = managers_.comp->Find(bar.ref.shot, bar.ref.id);
  assert(effect != nullptr);
  const QColor fill =
      KindMark(effect->kind).darker(effect->is_on ? 170 : 260);
  painter->setPen(is_picked ? QPen(theme::kPick, 2.0) : QPen(Qt::black));
  painter->setBrush(fill);
  painter->drawRect(bar.box);
  // On a doll it stops at the cut: what lies past it is hatched.
  const bool is_cut = bar.is_on_doll;
  if (is_cut) {
    const QRectF own(XOf(bar.clip->start.index()), 0,
                     XOf(bar.clip->end().index()) -
                         XOf(bar.clip->start.index()),
                     height());
    painter->save();
    painter->setClipRegion(QRegion(bar.box.toAlignedRect()) -
                               QRegion(own.toAlignedRect()),
                           Qt::IntersectClip);
    painter->fillRect(bar.box, QBrush(QColor(0, 0, 0, 160),
                                      Qt::BDiagPattern));
    painter->restore();
  }
  painter->setPen(Qt::black);
  painter->setBrush(theme::kHandle);
  for (const int frame : CompManager::KeyFrames(*effect)) {
    PaintKey(painter, QPointF(XOf(frame + bar.offset), bar.box.center().y()));
  }
}

void CompTimeline::PaintLight(QPainter* painter) const {
  assert(painter != nullptr);
  const Shot* shot =
      FindShot(managers_.history->current(), managers_.selection->shot());
  const Clip* clip = ClipOf(managers_.selection->shot());
  assert(clip != nullptr);
  const double y =
      kRuler + kRow * (1 + static_cast<double>(Bars().size()));
  painter->fillRect(QRectF(0, y, width(), kRow), kRowColor);
  const bool is_keyed = shot != nullptr && shot->light.keys.size() > 1;
  if (!is_keyed) {
    return;
  }
  painter->setPen(Qt::black);
  painter->setBrush(theme::kHandle);
  for (const auto& key : shot->light.keys) {
    PaintKey(painter,
             QPointF(XOf(clip->start.index() + key.frame.index() -
                         clip->in.index()),
                     y + kRow / 2.0));
  }
}

void CompTimeline::PaintBox(QPainter* painter) const {
  assert(painter != nullptr);
  assert(managers_.comp != nullptr);
  const bool is_boxing = grab_ == Grab::kBox && has_moved_;
  if (is_boxing) {
    painter->setPen(QPen(theme::kPick, 1.0, Qt::DashLine));
    painter->setBrush(QColor(0xd8, 0x30, 0x4f, 40));
    painter->drawRect(QRectF(press_at_, box_to_).normalized());
  }
}

}  // namespace snapper
