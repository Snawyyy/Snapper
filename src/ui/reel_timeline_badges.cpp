// ReelTimeline's cuts: the span mixed over and the badge that names the
// transition and opens it with one click.

#include <QPainter>
#include <QRectF>

#include <cassert>
#include <cmath>

#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "ui/reel_timeline.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr QColor kMixShade{0xff, 0xff, 0xff, 0x40};
constexpr double kBadgeWidth = 16.0;
constexpr double kBadgeHeight = 14.0;

// The letter a transition's badge shows: a cut |, swipes the way they
// go, a flash *, a fade ~.
QString GlyphOf(TransitionKind kind) {
  assert(static_cast<int>(kind) >= 0);
  assert(static_cast<int>(kind) < kTransitionKindCount);
  switch (kind) {
    case TransitionKind::kCut:
      return QStringLiteral("|");
    case TransitionKind::kSwipeLeft:
      return QStringLiteral("<");
    case TransitionKind::kSwipeRight:
      return QStringLiteral(">");
    case TransitionKind::kSwipeUp:
      return QStringLiteral("^");
    case TransitionKind::kSwipeDown:
      return QStringLiteral("v");
    case TransitionKind::kFlash:
      return QStringLiteral("*");
    case TransitionKind::kCrossfade:
      return QStringLiteral("~");
  }
  return QString();
}

// Where the badge of a cut at x sits: on the cut, low in the clip.
QRectF BadgeRect(double x, double bottom) {
  assert(std::isfinite(x) && std::isfinite(bottom));
  assert(kBadgeWidth > 0.0);
  return QRectF(x - kBadgeWidth / 2.0, bottom - kBadgeHeight - 2.0,
                kBadgeWidth, kBadgeHeight);
}

}  // namespace

ClipId ReelTimeline::BadgeAt(QPointF at) const {
  assert(managers_.history != nullptr);
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  for (const ReelTrack& track : managers_.history->current().reel.tracks) {
    for (const Clip& clip : track.clips) {
      const bool is_joined = NextTouching(track, clip) != nullptr;
      const bool is_hit =
          is_joined && BadgeRect(XOf(clip.end()), ClipRect(clip.id).bottom())
                           .contains(at);
      if (is_hit) {
        return clip.id;
      }
    }
  }
  return ClipId();
}

void ReelTimeline::PaintJoints(QPainter* painter) const {
  assert(painter != nullptr);
  assert(zoom_ > 0.0);
  const Reel& reel = managers_.history->current().reel;
  painter->save();
  painter->setClipRect(QRectF(kHeaderWidth, kRulerHeight, width(), height()));
  for (const ReelTrack& track : reel.tracks) {
    for (const Clip& clip : track.clips) {
      const bool is_joined = NextTouching(track, clip) != nullptr;
      if (!is_joined) {
        continue;
      }
      const QRectF box = ClipRect(clip.id);
      const double x = box.right();
      const int used = UsedTransition(track, clip).index();
      const bool has_mix = used > 0;
      if (has_mix) {
        // The span mixed over, faded towards the cut.
        const double from = XOf(Frame(clip.end().index() - used));
        const QRectF span(from, box.top(), x - from, box.height());
        painter->fillRect(span, kMixShade);
        painter->setPen(QPen(theme::kText, 1.0));
        painter->drawLine(span.bottomLeft(), span.topRight());
      }
      // The badge names the transition; a click on it opens it.
      const QRectF badge = BadgeRect(x, box.bottom());
      painter->setPen(QPen(has_mix ? theme::kText : theme::kTextOff, 1.0));
      painter->setBrush(theme::kFace);
      painter->drawRect(badge);
      painter->drawText(badge, Qt::AlignCenter, GlyphOf(clip.out.kind));
    }
  }
  painter->restore();
}

}  // namespace snapper
