// ReelTimeline's decoration: clips popping in, puffing out and
// rewinding. It only changes how clips are drawn, never what they are.

#include <QPainter>

#include <algorithm>
#include <cassert>

#include "edit/change_diff.h"
#include "edit/history_manager.h"
#include "ui/change_effects.h"
#include "ui/reel_timeline.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// A clip is long, so it grows a fraction of what a key does: 1.4 times
// becomes 1.05.
constexpr double kClipGrowShare = 0.125;
constexpr int kGlowAlpha = 110;
// How far the ring around a popping clip spreads, in pixels.
constexpr double kRingSpread = 6.0;

QRectF Shrunk(const QRectF& box, double scale) {
  assert(scale >= 0.0);
  assert(box.isValid());
  const QPointF middle = box.center();
  const double half_width = box.width() * scale / 2.0;
  const double half_height = box.height() * scale / 2.0;
  return QRectF(middle.x() - half_width, middle.y() - half_height,
                half_width * 2.0, half_height * 2.0);
}

}  // namespace

QPointF ReelTimeline::SlideShift(const Clip& clip, int track) const {
  assert(track >= 0);
  assert(zoom() > 0.0);
  const auto slide = effects_.SlideOf(track, clip.id.value(), clip.start);
  const bool is_sliding = slide.has_value();
  if (!is_sliding) {
    return QPointF();
  }
  const double left = 1.0 - slide->t;
  const int tracks =
      static_cast<int>(managers_.history->current().reel.tracks.size());
  const bool has_old_track =
      slide->from_lane >= 0 && slide->from_lane < tracks;
  const double rise =
      has_old_track ? TrackTop(slide->from_lane) - TrackTop(track) : 0.0;
  return QPointF((slide->from.index() - clip.start.index()) * zoom() * left,
                 rise * left);
}

void ReelTimeline::ShowStep(const Project& before, bool is_travel) {
  assert(managers_.history != nullptr);
  assert(before.reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  ClipChanges changes = DiffClips(before, managers_.history->current());
  // A drag already showed the clips moving.
  if (is_landing_drag_) {
    changes.moved.clear();
  }
  effects_.ShowClips(changes, is_travel);
}

void ReelTimeline::PaintClipFx(QPainter* painter, const Clip& clip) const {
  assert(painter != nullptr);
  assert(clip.length.index() >= 1);
  const ClipSpot spot = FindClip(managers_.history->current().reel, clip.id);
  const int lane = spot.track;
  const double grow =
      1.0 + (effects_.ScaleOf(lane, clip.id.value(), clip.start) - 1.0) *
                kClipGrowShare;
  const QRectF box = ClipRect(clip.id);
  painter->save();
  painter->translate(SlideShift(clip, lane));
  painter->translate(box.center());
  painter->scale(grow, grow);
  painter->translate(-box.center());
  PaintClip(painter, clip);
  const double glow = effects_.GlowOf(lane, clip.id.value(), clip.start);
  painter->fillRect(box, QColor(255, 255, 255,
                                static_cast<int>(kGlowAlpha * glow)));
  const auto ring = effects_.RingOf(lane, clip.id.value(), clip.start);
  const bool is_ringed = ring.has_value();
  if (is_ringed) {
    QColor color = theme::kPick;
    color.setAlphaF(static_cast<float>(1.0 - *ring));
    painter->setPen(QPen(color, 2.0));
    painter->setBrush(Qt::NoBrush);
    const double spread = kRingSpread * *ring;
    painter->drawRect(box.adjusted(-spread, -spread, spread, spread));
  }
  painter->restore();
}

void ReelTimeline::PaintPuffs(QPainter* painter) const {
  assert(painter != nullptr);
  assert(zoom() > 0.0);
  const int tracks =
      static_cast<int>(managers_.history->current().reel.tracks.size());
  for (const ChangeEffect& puff : effects_.Puffs()) {
    // The track may be gone by now (undo of adding one).
    const bool is_on_a_track = puff.lane >= 0 && puff.lane < tracks;
    if (!is_on_a_track) {
      continue;
    }
    const double left = XOf(puff.frame);
    const QRectF box(left, TrackTop(puff.lane) + 2.0,
                     XOf(Frame(puff.frame.index() + puff.length.index())) -
                         left,
                     kTrackHeight - 4.0);
    QColor fill = theme::kText;
    fill.setAlphaF(static_cast<float>(0.5 * PuffOpacity(puff.t)));
    painter->fillRect(Shrunk(box, PuffScale(puff.t)), fill);
  }
}

}  // namespace snapper
