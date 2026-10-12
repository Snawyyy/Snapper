#include "ui/reel_timeline.h"

#include <QFileInfo>
#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "edit/shot_timing.h"
#include "edit/sound_manager.h"
#include "ui/checkerboard.h"
#include "ui/form_helpers.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kDefaultZoom = 2.0;
constexpr double kMinZoom = 0.02;
constexpr double kMaxZoom = 24.0;
constexpr double kMinLabelGap = 64.0;
// Ruler steps, in seconds, as editors count them.
constexpr std::array<int, 10> kLabelSteps = {1,  2,   5,   10,  15,
                                             30, 60, 120, 300, 600};
constexpr QColor kShotClip{0x4b, 0x5a, 0x8f};
constexpr QColor kVideoClip{0x2f, 0x77, 0x6a};
constexpr QColor kMissing{0x7d, 0x2c, 0x2c};

}  // namespace

ReelTimeline::ReelTimeline(const Managers& managers)
    : managers_(managers),
      hover_(managers.motion, this),
      feedback_(managers.motion, this),
      watcher_(managers.history, this),
      effects_(managers.motion, this),
      settle_(managers.motion, managers.playback->FrameOn(Timeline::kReel),
              this),
      drag_playhead_(managers.playback),
      zoom_(kDefaultZoom) {
  assert(managers_.IsComplete());
  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(true);
  setAcceptDrops(true);
  connect(&hover_, &HandleHover::Changed, this, [this] { update(); });
  connect(&feedback_, &DragFeedback::Changed, this, [this] { update(); });
  connect(&effects_, &ChangeEffects::Changed, this, [this] { update(); });
  connect(&watcher_, &ChangeWatcher::Stepped, this, &ReelTimeline::ShowStep);
  connect(&settle_, &PlayheadSettle::Changed, this, [this] { update(); });
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, [this] {
    settle_.MoveTo(managers_.playback->FrameOn(Timeline::kReel),
                   managers_.playback->IsPlaying());
  });
  const auto update_me = [this] {
    const int tracks =
        static_cast<int>(managers_.history->current().reel.tracks.size());
    setMinimumHeight(static_cast<int>(kRulerHeight + tracks * kTrackHeight));
    update();
  };
  connect(managers_.history, &HistoryManager::Changed, this, update_me);
  connect(managers_.selection, &SelectionManager::Changed, this, update_me);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          update_me);
  // Waveforms show as their files finish loading.
  if (managers_.sounds != nullptr) {
    connect(managers_.sounds, &SoundManager::SoundsChanged, this,
            [this] { update(); });
  }
  update_me();
  assert(acceptDrops());
}

double ReelTimeline::XOf(Frame frame) const {
  assert(zoom_ > 0.0);
  assert(scroll_ >= 0.0);
  return kHeaderWidth + (frame.index() - scroll_) * zoom_;
}

Frame ReelTimeline::FrameAt(double x) const {
  assert(zoom_ > 0.0);
  assert(std::isfinite(x));
  const double frame = (x - kHeaderWidth) / zoom_ + scroll_;
  return Frame(static_cast<int>(std::floor(std::max(frame, 0.0))));
}

double ReelTimeline::TrackTop(int track) const {
  assert(track >= 0);
  const int count =
      static_cast<int>(managers_.history->current().reel.tracks.size());
  assert(track < count);
  // The top track is drawn on top, as it shows on top.
  return kRulerHeight + (count - 1 - track) * kTrackHeight;
}

int ReelTimeline::TrackAt(double y) const {
  assert(std::isfinite(y));
  const int count =
      static_cast<int>(managers_.history->current().reel.tracks.size());
  assert(count <= kMaxAllTracks);
  const int row = static_cast<int>(std::floor((y - kRulerHeight) /
                                              kTrackHeight));
  const bool is_on_tracks = y >= kRulerHeight && row >= 0 && row < count;
  return is_on_tracks ? count - 1 - row : -1;
}

QRectF ReelTimeline::ClipRect(ClipId clip) const {
  assert(clip.value() >= 0);
  assert(managers_.history != nullptr);
  const Reel& reel = managers_.history->current().reel;
  const ClipSpot spot = FindClip(reel, clip);
  const bool is_present = spot.IsValid();
  if (!is_present) {
    return QRectF();
  }
  const Clip& found = *ClipOf(reel, clip);
  const double left = XOf(found.start);
  return QRectF(left, TrackTop(spot.track) + 2.0, XOf(found.end()) - left,
                kTrackHeight - 4.0);
}

void ReelTimeline::SetZoom(double pixels_per_frame) {
  assert(std::isfinite(pixels_per_frame));
  zoom_ = std::clamp(pixels_per_frame, kMinZoom, kMaxZoom);
  assert(zoom_ > 0.0);
  update();
}

int ReelTimeline::LabelStep() const {
  assert(zoom_ > 0.0);
  assert(kMinLabelGap > 0.0);
  for (const int step : kLabelSteps) {
    const bool is_roomy = step * kFramesPerSecond * zoom_ >= kMinLabelGap;
    if (is_roomy) {
      return step;
    }
  }
  return kLabelSteps.back();
}

void ReelTimeline::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), theme::kFaceDark);
  PaintTracks(&painter);
  PaintJoints(&painter);
  PaintDrop(&painter);
  PaintRuler(&painter);
  PaintMarkers(&painter);
  PaintPlayhead(&painter);
  PaintBox(&painter);
  feedback_.Paint(&painter, QRectF(rect()));
}

void ReelTimeline::PaintRuler(QPainter* painter) const {
  assert(painter != nullptr);
  assert(managers_.playback != nullptr);
  painter->fillRect(QRectF(0, 0, width(), kRulerHeight), theme::kFace);
  const int step = LabelStep() * kFramesPerSecond;
  const int first = FrameAt(kHeaderWidth).index() / step * step;
  const int last = FrameAt(width()).index();
  for (int f = first; f <= last; f += step) {
    const double x = XOf(Frame(f));
    painter->setPen(theme::kShadow);
    painter->drawLine(QPointF(x, kRulerHeight - 6), QPointF(x, kRulerHeight));
    painter->setPen(theme::kTextOff);
    painter->drawText(QPointF(x + 3, kRulerHeight - 8), TimeText(Frame(f)));
  }
  // The corner names where the playhead is.
  const Frame now = managers_.playback->FrameOn(Timeline::kReel);
  painter->fillRect(QRectF(0, 0, kHeaderWidth, kRulerHeight),
                    theme::kFaceDark);
  painter->setPen(theme::kText);
  painter->drawText(QRectF(0, 0, kHeaderWidth, kRulerHeight),
                    Qt::AlignCenter, TimeText(now));
}

void ReelTimeline::PaintTracks(QPainter* painter) const {
  assert(painter != nullptr);
  const Reel& reel = managers_.history->current().reel;
  const int count = static_cast<int>(reel.tracks.size());
  assert(count <= kMaxAllTracks);
  const int sounds = SoundTrackCount(reel);
  for (int t = 0; t < count; ++t) {
    const double top = TrackTop(t);
    painter->fillRect(QRectF(kHeaderWidth, top, width(), kTrackHeight - 1),
                      t % 2 == 0 ? theme::kField : theme::kFaceDark);
    painter->fillRect(QRectF(0, top, kHeaderWidth - 1, kTrackHeight - 1),
                      theme::kFace);
    const bool is_sound = reel.tracks[static_cast<size_t>(t)].is_sound;
    if (is_sound) {
      PaintSoundHeader(painter, t);
      continue;
    }
    painter->setPen(theme::kTextOff);
    painter->drawText(QRectF(0, top, kHeaderWidth, kTrackHeight),
                      Qt::AlignCenter, tr("V%1").arg(t - sounds + 1));
  }
  painter->save();
  painter->setClipRect(QRectF(kHeaderWidth, kRulerHeight, width(), height()));
  for (const ReelTrack& track : reel.tracks) {
    for (const Clip& clip : track.clips) {
      PaintClipFx(painter, clip);
    }
  }
  PaintPuffs(painter);
  painter->restore();
}

void ReelTimeline::PaintClip(QPainter* painter, const Clip& clip) const {
  assert(painter != nullptr);
  assert(clip.length.index() >= 1);
  const QRectF box = ClipRect(clip.id);
  const bool is_sound = IsSoundClip(clip);
  if (is_sound) {
    PaintSoundBody(painter, clip, box);
  } else {
    PaintPictureBody(painter, clip, box);
  }
  const bool is_picked = managers_.selection->clips().contains(clip.id);
  painter->setPen(QPen(is_picked ? theme::kPick : theme::kShadow,
                       is_picked ? 2.0 : 1.0));
  painter->drawRect(box);
  // The ends trim: yellow, like every handle. A hovered one widens and
  // brightens, even on a clip that is not picked.
  const double start =
      HoverAmount(hover_.ScaleOf(GrabKind::kEdge, EdgeKey(clip.id, true)));
  const double end =
      HoverAmount(hover_.ScaleOf(GrabKind::kEdge, EdgeKey(clip.id, false)));
  const bool shows_ends = is_picked || start > 0.0 || end > 0.0;
  if (shows_ends) {
    const QColor lit_start = theme::kHandle.lighter(100 + int(40 * start));
    const QColor lit_end = theme::kHandle.lighter(100 + int(40 * end));
    painter->fillRect(
        QRectF(box.left(), box.top(), 3 + 3 * start, box.height()),
        lit_start);
    painter->fillRect(QRectF(box.right() - 3 - 3 * end, box.top(),
                             3 + 3 * end, box.height()),
                      lit_end);
  }
}

void ReelTimeline::PaintHatch(QPainter* painter, const Clip& clip,
                              const QRectF& box) const {
  assert(painter != nullptr);
  assert(clip.length.index() >= 1);
  // Frames with nothing to show or play (a shot cut shorter since, a
  // clip past its file's end) are hatched.
  const double shown =
      XOf(Frame(clip.start.index() +
                ShownLength(managers_.history->current(), clip).index()));
  const bool has_gap = shown < box.right();
  if (has_gap) {
    painter->fillRect(QRectF(shown, box.top(), box.right() - shown,
                             box.height()),
                      QBrush(theme::kShadow, Qt::BDiagPattern));
  }
}

void ReelTimeline::PaintPictureBody(QPainter* painter, const Clip& clip,
                                    const QRectF& box) const {
  assert(painter != nullptr);
  assert(!IsSoundClip(clip));
  const Project& project = managers_.history->current();
  const auto* video = std::get_if<VideoSource>(&clip.source);
  const Shot* shot =
      video != nullptr
          ? nullptr
          : FindShot(project, std::get<ShotSource>(clip.source).shot);
  const bool is_missing = video == nullptr && shot == nullptr;
  painter->fillRect(box, is_missing          ? kMissing
                         : video != nullptr ? kVideoClip
                                            : kShotClip);
  PaintHatch(painter, clip, box);
  const bool has_swatch = shot != nullptr;
  if (has_swatch) {
    // A see-through shot shows the checker instead of its color.
    const QRectF swatch(box.topLeft(), QSizeF(5, box.height()));
    if (shot->is_transparent) {
      PaintCheckerboard(painter, swatch, 2.5);
    } else {
      painter->fillRect(swatch, shot->background);
    }
  }
  const QString name = video != nullptr ? QFileInfo(video->path).fileName()
                       : is_missing     ? tr("Shot removed")
                                        : shot->name;
  painter->setPen(theme::kText);
  painter->drawText(box.adjusted(8, 3, -4, -3), Qt::AlignLeft | Qt::AlignTop,
                    name);
  // Keys a shortened shot keeps past its end, where the clip ends.
  const int hidden = shot != nullptr ? HiddenKeyFrames(*shot) : 0;
  const bool shows_hidden = hidden > 0 && clip.in.index() +
                                                  clip.length.index() >=
                                              shot->length.index();
  if (shows_hidden) {
    painter->setPen(theme::kTextOff);
    painter->drawText(box.adjusted(8, 3, -6, -3),
                      Qt::AlignRight | Qt::AlignBottom,
                      hidden == 1 ? tr("1 key hidden")
                                  : tr("%1 keys hidden").arg(hidden));
  }
}

QString ReelTimeline::EdgeKey(ClipId clip, bool is_start) {
  assert(clip.IsValid());
  assert(clip.value() > 0);
  return QStringLiteral("%1/%2").arg(clip.value()).arg(is_start ? 's' : 'e');
}

void ReelTimeline::leaveEvent(QEvent* event) {
  assert(event != nullptr);
  assert(hover_.kind() >= GrabKind::kNone);
  hover_.Set(GrabKind::kNone, QString());
  QWidget::leaveEvent(event);
}

void ReelTimeline::PaintDrop(QPainter* painter) const {
  assert(painter != nullptr);
  assert(drop_track_ >= -1);
  const bool is_dropping = drop_track_ >= 0;
  if (!is_dropping) {
    return;
  }
  const double left = XOf(drop_frame_);
  const QRectF box(left, TrackTop(drop_track_) + 2.0,
                   XOf(Frame(drop_frame_.index() + drop_length_.index())) -
                       left,
                   kTrackHeight - 4.0);
  painter->setPen(QPen(theme::kHandle, 1.0, Qt::DashLine));
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(box);
}

void ReelTimeline::PaintMarkers(QPainter* painter) const {
  assert(painter != nullptr);
  assert(managers_.history != nullptr);
  const Reel& reel = managers_.history->current().reel;
  for (const Frame marker : reel.markers) {
    const double x = XOf(marker);
    const bool is_shown = x >= kHeaderWidth && x <= width();
    if (!is_shown) {
      continue;
    }
    // A notch in the ruler and a faint line down through the tracks.
    painter->setPen(QPen(theme::kHandle, 1.0, Qt::DashLine));
    painter->drawLine(QPointF(x, kRulerHeight), QPointF(x, height()));
    const QPolygonF notch({QPointF(x - 4, 0), QPointF(x + 4, 0),
                           QPointF(x, kRulerHeight / 2)});
    painter->setPen(Qt::NoPen);
    painter->setBrush(theme::kHandle);
    painter->drawPolygon(notch);
  }
}

void ReelTimeline::PaintPlayhead(QPainter* painter) const {
  assert(painter != nullptr);
  assert(managers_.playback != nullptr);
  const double x = XOf(managers_.playback->FrameOn(Timeline::kReel)) +
                   settle_.offset() * zoom_;
  const bool is_shown = x >= kHeaderWidth;
  if (is_shown) {
    painter->setPen(QPen(theme::kPick, 1.0));
    painter->drawLine(QPointF(x, 0), QPointF(x, height()));
  }
}

void ReelTimeline::PaintBox(QPainter* painter) const {
  assert(painter != nullptr);
  assert(zoom_ > 0.0);
  const bool is_boxing = drag_ == Drag::kBox;
  if (!is_boxing) {
    return;
  }
  painter->setPen(QPen(theme::kPick, 1.0, Qt::DashLine));
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(QRectF(press_, box_to_).normalized());
}

}  // namespace snapper
