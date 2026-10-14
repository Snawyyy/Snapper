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
#include "edit/record_manager.h"
#include "edit/selection_manager.h"
#include "edit/shot_timing.h"
#include "edit/sound_manager.h"
#include "render/reel_text.h"
#include "ui/checkerboard.h"
#include "ui/form_helpers.h"
#include "ui/theme.h"
#include "ui/timeline_ruler.h"

namespace snapper {
namespace {

constexpr double kDefaultZoom = 2.0;
constexpr QColor kShotClip{0x4b, 0x5a, 0x8f};
constexpr QColor kVideoClip{0x2f, 0x77, 0x6a};
constexpr QColor kMissing{0x7d, 0x2c, 0x2c};
constexpr QColor kTextClip{0x7a, 0x4f, 0x8c};

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
      scale_(this, kHeaderWidth, kDefaultZoom),
      step_mode_(managers.playback, this),
      track_scroll_(Qt::Vertical, this) {
  assert(managers_.IsComplete());
  step_mode_.setObjectName("step_mode");
  step_mode_.setGeometry(
      StepModeSwitch::CornerRect(static_cast<int>(kHeaderWidth)));
  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(true);
  setAcceptDrops(true);
  // The ruler and two tracks always show; more scroll.
  setMinimumHeight(static_cast<int>(kRulerHeight + 2 * kTrackHeight));
  track_scroll_.setSingleStep(static_cast<int>(kTrackHeight / 2));
  track_scroll_.setFocusPolicy(Qt::NoFocus);
  connect(&track_scroll_, &QScrollBar::valueChanged, this,
          [this] { update(); });
  connect(&hover_, &HandleHover::Changed, this, [this] { update(); });
  connect(&feedback_, &DragFeedback::Changed, this, [this] { update(); });
  connect(&effects_, &ChangeEffects::Changed, this, [this] { update(); });
  connect(&watcher_, &ChangeWatcher::Stepped, this, &ReelTimeline::ShowStep);
  connect(&settle_, &PlayheadSettle::Changed, this, [this] { update(); });
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, [this] {
    settle_.MoveTo(managers_.playback->FrameOn(Timeline::kReel),
                   managers_.playback->IsPlaying());
  });
  const auto update_me = [this] { update(); };
  // Only a change of tracks moves the scroll range; frames just repaint.
  connect(managers_.history, &HistoryManager::Changed, this,
          &ReelTimeline::FitScrollBar);
  connect(managers_.history, &HistoryManager::Changed, this,
          &ReelTimeline::SyncScale);
  connect(managers_.selection, &SelectionManager::Changed, this, update_me);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          update_me);
  // Waveforms show as their files finish loading.
  const bool has_sounds = managers_.sounds != nullptr;
  if (has_sounds) {
    connect(managers_.sounds, &SoundManager::SoundsChanged, this, update_me);
  }
  // The armed track's R lights up.
  const bool has_record = managers_.record != nullptr;
  if (has_record) {
    connect(managers_.record, &RecordManager::StateChanged, this, update_me);
  }
  SyncScale();
  FitScrollBar();
  assert(acceptDrops());
}

void ReelTimeline::SyncScale() {
  assert(managers_.history != nullptr);
  assert(zoom() > 0.0);
  scale_.SetLength(ReelLength(managers_.history->current()).index());
}

double ReelTimeline::TracksHeight() const {
  assert(managers_.history != nullptr);
  const int count =
      static_cast<int>(managers_.history->current().reel.tracks.size());
  assert(count <= kMaxAllTracks);
  return count * kTrackHeight;
}

void ReelTimeline::FitScrollBar() {
  assert(kRulerHeight > 0.0);
  assert(kTrackHeight > 0.0);
  const double room = std::max(0.0, height() - kRulerHeight);
  const int over = static_cast<int>(std::ceil(TracksHeight() - room));
  const bool is_over = over > 0;
  track_scroll_.setRange(0, std::max(over, 0));
  track_scroll_.setPageStep(static_cast<int>(room));
  const int bar = track_scroll_.sizeHint().width();
  track_scroll_.setGeometry(width() - bar, static_cast<int>(kRulerHeight),
                            bar, std::max(0, height() -
                                                 static_cast<int>(kRulerHeight)));
  track_scroll_.setVisible(is_over);
  update();
}

void ReelTimeline::resizeEvent(QResizeEvent* event) {
  assert(event != nullptr);
  assert(height() >= 0);
  QWidget::resizeEvent(event);
  FitScrollBar();
}

double ReelTimeline::XOf(Frame frame) const {
  assert(zoom() > 0.0);
  assert(frame.index() >= 0);
  return scale_.XOf(frame.index());
}

Frame ReelTimeline::FrameAt(double x) const {
  assert(zoom() > 0.0);
  assert(std::isfinite(x));
  return Frame(std::max(0, scale_.FrameAt(x)));
}

double ReelTimeline::TrackTop(int track) const {
  assert(track >= 0);
  const int count =
      static_cast<int>(managers_.history->current().reel.tracks.size());
  assert(track < count);
  // The top track is drawn on top, as it shows on top.
  return kRulerHeight + (count - 1 - track) * kTrackHeight -
         track_scroll_.value();
}

int ReelTimeline::TrackAt(double y) const {
  assert(std::isfinite(y));
  const int count =
      static_cast<int>(managers_.history->current().reel.tracks.size());
  assert(count <= kMaxAllTracks);
  const int row = static_cast<int>(std::floor(
      (y + track_scroll_.value() - kRulerHeight) / kTrackHeight));
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
  assert(pixels_per_frame > 0.0);
  scale_.SetPixelsPerFrame(pixels_per_frame);
  assert(zoom() > 0.0);
  update();
}

void ReelTimeline::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), theme::kFaceDark);
  PaintTracks(&painter);
  PaintJoints(&painter);
  PaintDrop(&painter);
  PaintFrameRuler(&painter, scale_, width(), false);
  PaintMarked(&painter);
  PaintMarkers(&painter);
  PaintPlayhead(&painter);
  PaintBox(&painter);
  feedback_.Paint(&painter, QRectF(rect()));
}

void ReelTimeline::PaintTracks(QPainter* painter) const {
  assert(painter != nullptr);
  const Reel& reel = managers_.history->current().reel;
  const int count = static_cast<int>(reel.tracks.size());
  assert(count <= kMaxAllTracks);
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
                      Qt::AlignCenter, TrackName(t));
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
  const bool is_text = IsTextClip(clip);
  if (is_text) {
    PaintTextBody(painter, clip, box);
    return;
  }
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

void ReelTimeline::PaintTextBody(QPainter* painter, const Clip& clip,
                                 const QRectF& box) const {
  assert(painter != nullptr);
  assert(IsTextClip(clip));
  const Project& project = managers_.history->current();
  const auto& text = std::get<TextSource>(clip.source);
  painter->fillRect(box, kTextClip);
  // The look's fill color as a swatch, as a shot shows its background.
  painter->fillRect(QRectF(box.topLeft(), QSizeF(5, box.height())),
                    LookOf(project.reel, clip).letters.fill);
  const QString first = text.text.section(QLatin1Char('\n'), 0, 0);
  painter->setPen(theme::kText);
  painter->drawText(box.adjusted(8, 3, -4, -3), Qt::AlignLeft | Qt::AlignTop,
                    first.isEmpty() ? tr("(no words)") : first);
  // A cue wrapping past two lines is hard to read in time.
  const bool is_cue = FindClip(project.reel, clip.id).track ==
                      SubtitleTrack(project.reel);
  const bool is_long = is_cue && ReelTextLines(project, clip) > 2;
  if (is_long) {
    painter->setPen(theme::kHandle);
    painter->drawText(box.adjusted(8, 3, -6, -3),
                      Qt::AlignRight | Qt::AlignBottom, tr("! 3+ lines"));
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
  const Frame now = managers_.playback->FrameOn(Timeline::kReel);
  const double x = XOf(now) + settle_.offset() * zoom();
  PaintTimelinePlayhead(
      painter, x, now.index(),
      QRectF(kHeaderWidth, 0, width() - kHeaderWidth, height()));
}

void ReelTimeline::PaintMarked(QPainter* painter) const {
  assert(painter != nullptr);
  assert(marked_track_ >= -1);
  const int count =
      static_cast<int>(managers_.history->current().reel.tracks.size());
  const bool is_shown = marked_.IsValid() && marked_track_ >= 0 &&
                        marked_track_ < count;
  if (!is_shown) {
    return;
  }
  const double left = std::max(XOf(marked_.start), kHeaderWidth);
  const double right = XOf(marked_.end);
  if (right <= left) {
    return;
  }
  QColor band = theme::kHandle;
  band.setAlpha(60);
  painter->save();
  painter->setClipRect(QRectF(kHeaderWidth, 0, width(), height()));
  painter->fillRect(QRectF(left, kRulerHeight - 5.0, right - left, 5.0),
                    theme::kHandle);
  const double top = TrackTop(marked_track_);
  const bool is_visible = top + kTrackHeight > kRulerHeight;
  if (is_visible) {
    const QRectF row(left, std::max(top, kRulerHeight), right - left,
                     top + kTrackHeight - std::max(top, kRulerHeight));
    painter->fillRect(row, band);
    painter->setPen(QPen(theme::kHandle, 1.0, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(row.adjusted(0.5, 0.5, -0.5, -0.5));
  }
  painter->restore();
}

void ReelTimeline::PaintBox(QPainter* painter) const {
  assert(painter != nullptr);
  assert(zoom() > 0.0);
  const bool is_boxing = drag_ == Drag::kBox;
  if (!is_boxing) {
    return;
  }
  painter->setPen(QPen(theme::kPick, 1.0, Qt::DashLine));
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(QRectF(press_, box_to_).normalized());
}

}  // namespace snapper
