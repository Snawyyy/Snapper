#include "ui/timeline_view.h"

#include <QMenu>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/master_timeline.h"
#include "edit/change_diff.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reference_manager.h"
#include "edit/selection_manager.h"
#include "edit/sound_manager.h"
#include "media/audio_clip.h"
#include "ui/form_helpers.h"
#include "ui/paint_clip.h"
#include "ui/playhead.h"
#include "ui/theme.h"
#include "ui/timeline_layout.h"
#include "ui/timeline_ruler.h"

namespace snapper {

TimelineView::TimelineView(const Managers& managers)
    : managers_(managers), step_mode_(managers.playback, this),
      chips_(managers, this),
      scale_(this, kNameWidth, kDefaultFrameWidth),
      hover_(managers.motion, this),
      feedback_(managers.motion, this),
      watcher_(managers.history, this),
      effects_(managers.motion, this),
      settle_(managers.motion, managers.playback->FrameOn(Timeline::kShots),
              this),
      drag_playhead_(managers.playback) {
  assert(managers_.IsComplete());
  step_mode_.setObjectName("step_mode");
  step_mode_.setGeometry(StepModeSwitch::CornerRect(kNameWidth));
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  connect(&hover_, &HandleHover::Changed, this, [this] { update(); });
  connect(&feedback_, &DragFeedback::Changed, this, [this] { update(); });
  connect(&effects_, &ChangeEffects::Changed, this, [this] { update(); });
  connect(&watcher_, &ChangeWatcher::Stepped, this, &TimelineView::ShowStep);
  connect(&settle_, &PlayheadSettle::Changed, this, [this] { update(); });
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          [this] {
            settle_.MoveTo(managers_.playback->FrameOn(Timeline::kShots),
                           managers_.playback->IsPlaying());
          });
  setMinimumHeight(kRulerHeight + kWaveHeight + 3 * kRowHeight);
  connect(&chips_, &KeyChipBar::Problem, this, &TimelineView::Problem);
  // Playing pages the view along until it is scrolled by hand.
  connect(managers_.playback, &PlaybackManager::PlayingChanged, this,
          [this](bool is_playing) {
            if (is_playing) {
              scale_.BeginFollow();
            }
            FollowPlayhead();
            ShowChips();
            update();
          });
  const auto update_me = [this] {
    SyncScale();
    FollowPlayhead();
    ShowChips();
    update();
  };
  connect(managers_.history, &HistoryManager::Changed, this, update_me);
  connect(managers_.selection, &SelectionManager::Changed, this, update_me);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          update_me);
  connect(managers_.playback, &PlaybackManager::SongChanged, this,
          update_me);
  // The wave shows the reel's sound once it loads, and follows the clip
  // the shot borrows it from.
  const bool has_sounds = managers_.sounds != nullptr;
  if (has_sounds) {
    connect(managers_.sounds, &SoundManager::SoundsChanged, this,
            update_me);
  }
  const bool has_references = managers_.references != nullptr;
  if (has_references) {
    connect(managers_.references, &ReferenceManager::Changed, this,
            update_me);
    setAcceptDrops(true);
  }
  SyncScale();
  assert(FrameWidth() > 0.0);
}

void TimelineView::SyncScale() {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  const Shot* shot =
      FindShot(managers_.history->current(), managers_.selection->shot());
  const bool has_shot = shot != nullptr;
  scale_.SetLength(has_shot ? shot->length.index() : 1);
}

void TimelineView::FollowPlayhead() {
  assert(managers_.playback != nullptr);
  assert(FrameWidth() > 0.0);
  const auto here = PlayheadHere();
  const bool is_playing_here = managers_.playback->IsPlaying() &&
                               managers_.playback->timeline() ==
                                   Timeline::kShots;
  const bool can_follow = is_playing_here && here.has_value();
  const bool is_paged = can_follow && scale_.Follow(here->index());
  if (is_paged) {
    update();
  }
}

void TimelineView::ShowStep(const Project& before, bool is_travel) {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  KeyChanges changes = DiffKeys(before, managers_.history->current(),
                                managers_.selection->shot());
  // A drag already showed the keys moving.
  if (is_landing_drag_) {
    changes.moved.clear();
  }
  effects_.ShowKeys(changes, is_travel);
}

std::optional<Frame> TimelineView::FrameAt(double x) const {
  assert(FrameWidth() > 0.0);
  assert(std::isfinite(x));
  const bool is_frames = x >= kNameWidth;
  if (!is_frames) {
    return std::nullopt;
  }
  return Frame(scale_.FrameAt(x));
}

double TimelineView::XOf(Frame frame) const {
  assert(FrameWidth() > 0.0);
  assert(frame.index() >= 0);
  return scale_.XOf(frame.index());
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
  PaintReferences(&painter);
  draft_.Paint(&painter);
  PaintRows(&painter, rows, false);
  PaintReferenceMarks(&painter);
  PaintPlayhead(&painter);
  if (is_boxing_) {
    painter.setPen(QPen(theme::kPick, 1.0, Qt::DashLine));
    painter.setBrush(QColor(theme::kPick.red(), theme::kPick.green(),
                            theme::kPick.blue(), 40));
    painter.drawRect(QRectF(box_from_, box_to_).normalized());
  }
  feedback_.Paint(&painter, QRectF(rect()));
}

void TimelineView::PaintRuler(QPainter* painter) const {
  assert(painter != nullptr);
  const Shot* shot =
      FindShot(managers_.history->current(), managers_.selection->shot());
  assert(shot != nullptr);
  // Beyond the shot's end is dimmed: keys there never play. The ruler
  // dims its own band; numbers sit over the frame's middle, where the
  // keys are.
  const double end = std::max<double>(XOf(shot->length), kNameWidth);
  painter->fillRect(QRectF(end, kRulerHeight, width() - end,
                           height() - kRulerHeight),
                    theme::kFaceDark);
  PaintFrameRuler(painter, scale_, width(), true);
}

void TimelineView::PaintGrid(QPainter* painter) const {
  assert(painter != nullptr);
  assert(FrameWidth() > 0.0);
  const int first = std::max(0, FrameAt(kNameWidth)->index());
  const int last = FrameAt(width())->index();
  const int step = FrameStepFor(FrameWidth(), kMinLabelGap);
  const double top = kRulerHeight + kWaveHeight;
  const PaintClip keep_in(painter, FrameArea(top));
  for (int f = first; f <= last; ++f) {
    const bool is_second = f % kFramesPerSecond == 0;
    const bool is_labelled = f % step == 0;
    // Every frame faintly when there is room, numbered frames a little
    // more, seconds most.
    const bool is_drawn =
        is_second || is_labelled || FrameWidth() >= kMinGridWidth;
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

const Clip* TimelineView::SoundClip() const {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = managers_.selection->shot();
  const bool can_borrow = managers_.sounds != nullptr && shot.IsValid();
  if (!can_borrow) {
    return nullptr;
  }
  const Project& project = managers_.history->current();
  return ClipOf(project.reel, managers_.sounds->SoundClipOf(project, shot));
}

void TimelineView::PickSoundClip(QPoint where) {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = managers_.selection->shot();
  const bool can_borrow = managers_.sounds != nullptr && shot.IsValid();
  if (!can_borrow) {
    return;
  }
  const Project& project = managers_.history->current();
  const Clip* now = SoundClip();
  QMenu menu(this);
  for (const ClipId id : managers_.sounds->ClipsOf(project, shot)) {
    const Clip* clip = ClipOf(project.reel, id);
    QAction* item =
        menu.addAction(tr("Sound from clip at %1").arg(TimeText(clip->start)));
    item->setCheckable(true);
    item->setChecked(clip == now);
    connect(item, &QAction::triggered, this,
            [this, id] { managers_.sounds->PreferClip(id); });
  }
  const bool is_empty = menu.isEmpty();
  if (!is_empty) {
    menu.exec(where);
  }
}

void TimelineView::PaintWave(QPainter* painter) const {
  assert(painter != nullptr);
  assert(managers_.playback != nullptr);
  // What the shot plays: the reel's sound under its clip, or the song.
  const auto song = managers_.playback->ShotsSound();
  const Project& project = managers_.history->current();
  const int index = ShotIndex(project, managers_.selection->shot());
  const bool has_wave = song != nullptr && index >= 0;
  const QRectF band(kNameWidth, kRulerHeight, width() - kNameWidth,
                    kWaveHeight);
  painter->fillRect(band, theme::kFaceDark);
  painter->fillRect(QRectF(0, kRulerHeight, kNameWidth, kWaveHeight),
                    theme::kFace);
  const Clip* clip = SoundClip();
  painter->setPen(clip != nullptr ? theme::kText : theme::kTextOff);
  painter->drawText(QRectF(6, kRulerHeight, kNameWidth - 8, kWaveHeight),
                    Qt::AlignVCenter | Qt::AlignLeft,
                    clip != nullptr
                        ? tr("From clip at %1").arg(TimeText(clip->start))
                    : has_wave ? tr("Song")
                               : tr("No song"));
  const bool has_clip = clip != nullptr;
  if (has_clip) {
    // Frames the clip leaves out are silent: shaded, so it is clear why.
    const double from = std::max<double>(XOf(clip->in), kNameWidth);
    const double to = std::max<double>(
        XOf(Frame(clip->in.index() + clip->length.index())), kNameWidth);
    const QBrush shade(theme::kTextOff, Qt::BDiagPattern);
    painter->fillRect(
        QRectF(kNameWidth, kRulerHeight, from - kNameWidth, kWaveHeight),
        shade);
    painter->fillRect(
        QRectF(to, kRulerHeight, std::max(width() - to, 0.0), kWaveHeight),
        shade);
  }
  if (!has_wave) {
    return;
  }
  const double start = SecondsAtFrame(ShotStart(project, index));
  const double per_pixel = 1.0 / (FrameWidth() * kFramesPerSecond);
  const double middle = band.center().y();
  painter->setPen(theme::kPick.darker(130));
  for (int x = kNameWidth; x < width(); ++x) {
    const double at = start + scale_.FrameAtX(x) / kFramesPerSecond;
    const double half =
        PeakBetween(*song, at, at + per_pixel) * (kWaveHeight / 2.0 - 1.0);
    painter->drawLine(QPointF(x, middle - half), QPointF(x, middle + half));
  }
}

void TimelineView::PaintPlayhead(QPainter* painter) const {
  assert(painter != nullptr);
  assert(FrameWidth() > 0.0);
  const auto here = PlayheadHere();
  if (!here) {
    return;
  }
  // On the frame's middle, where its keys are.
  const double x =
      XOf(*here) + FrameWidth() / 2.0 + settle_.offset() * FrameWidth();
  PaintTimelinePlayhead(painter, x, here->index(), FrameArea(0.0));
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
  const double top = RowsTop();
  const int row = static_cast<int>(std::floor((y - top) / kRowHeight));
  const bool is_row = y >= top && row < static_cast<int>(count);
  return is_row ? row : -1;
}

std::optional<Frame> TimelineView::KeyNear(const TimelineRow& row,
                                           double x) const {
  assert(std::isfinite(x));
  assert(row.keys.size() == row.is_eased.size());
  // Wide frames let a click anywhere in the frame's half reach its key.
  const double reach = std::max(FrameWidth() / 2.0, kGrabReach);
  std::vector<QPointF> spots;
  for (const Frame key : row.keys) {
    spots.emplace_back(XOf(key) + FrameWidth() / 2.0, 0.0);
  }
  const int nearest = NearestGrab(spots, QPointF(x, 0.0), reach);
  const bool is_hit = nearest >= 0;
  return is_hit ? std::optional<Frame>(row.keys[static_cast<size_t>(nearest)])
                : std::nullopt;
}

QString TimelineView::KeyName(size_t row, Frame frame) {
  assert(frame.index() >= 0);
  assert(row < 100000);
  return QStringLiteral("%1/%2").arg(row).arg(frame.index());
}

void TimelineView::UpdateHover(QPointF point) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(key_drag_ == nullptr);
  const Project& project = managers_.history->current();
  const auto rows = TimelineRows(project, managers_.selection->shot());
  const bool is_in_rows = point.y() >= RowsTop();
  const int index = is_in_rows ? RowAt(point.y(), rows.size()) : -1;
  const bool is_row = index >= 0;
  const auto key = is_row ? KeyNear(rows[static_cast<size_t>(index)],
                                    point.x())
                          : std::nullopt;
  if (!key) {
    ClearHover();
    return;
  }
  hover_.Set(GrabKind::kKey, KeyName(static_cast<size_t>(index), *key));
  setCursor(CursorFor(GrabKind::kKey));
}

void TimelineView::ClearHover() {
  assert(managers_.history != nullptr);
  hover_.Set(GrabKind::kNone, QString());
  unsetCursor();
  assert(hover_.kind() == GrabKind::kNone);
}

void TimelineView::leaveEvent(QEvent* event) {
  assert(event != nullptr);
  assert(hover_.kind() >= GrabKind::kNone);
  ClearHover();
  QWidget::leaveEvent(event);
}

}  // namespace snapper
