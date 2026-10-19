// ReelTimeline's sound: sound clips' waveforms, volume lines and fades,
// and the sound tracks' headers (Mute, Solo, arm, fader).

#include <QFileInfo>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "edit/history_manager.h"
#include "edit/record_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "edit/sound_manager.h"
#include "ui/drag_text.h"
#include "ui/deck_paint.h"
#include "ui/grab.h"
#include "ui/reel_timeline.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr QColor kMissingSound{0x7d, 0x2c, 0x2c};
// The fade handles, square, on the clip's top corners.
constexpr double kFadeHandle = 8.0;
// How near the volume line a press grabs it.
constexpr double kLineReach = 4.0;

}  // namespace

QRectF ReelTimeline::MuteRect(int track) const {
  assert(track >= 0);
  assert(kHeaderWidth > 40.0);
  return QRectF(5.0, TrackTop(track) + 20.0, 24.0, 13.0);
}

QRectF ReelTimeline::SoloRect(int track) const {
  assert(track >= 0);
  assert(kHeaderWidth > 40.0);
  return QRectF(32.0, TrackTop(track) + 20.0, 24.0, 13.0);
}

QRectF ReelTimeline::ArmRect(int track) const {
  assert(track >= 0);
  assert(kHeaderWidth > 40.0);
  return QRectF(kHeaderWidth - 20.0, TrackTop(track) + 3.0, 15.0, 14.0);
}

QRectF ReelTimeline::FaderRect(int track) const {
  assert(track >= 0);
  assert(kHeaderWidth > 40.0);
  return QRectF(5.0, TrackTop(track) + 37.0, kHeaderWidth - 13.0, 5.0);
}

double ReelTimeline::VolumeY(const QRectF& box, double gain) {
  assert(std::isfinite(gain));
  assert(box.height() > 4.0);
  return box.bottom() - 2.0 - GainToLevel(gain) * (box.height() - 4.0);
}

QString ReelTimeline::TrackName(int track) const {
  assert(track >= 0);
  const Reel& reel = managers_.history->current().reel;
  assert(track < static_cast<int>(reel.tracks.size()));
  const ReelTrack& row = reel.tracks[static_cast<size_t>(track)];
  const int sounds = SoundTrackCount(reel);
  if (row.is_subtitle) {
    return tr("Subs");
  }
  if (!row.is_sound) {
    return tr("V%1").arg(track - sounds + 1);
  }
  return row.name.isEmpty() ? tr("A%1").arg(track + 1) : row.name;
}

void ReelTimeline::PaintSoundHeader(QPainter* painter, int track) const {
  assert(painter != nullptr);
  assert(track >= 0);
  const ReelTrack& row =
      managers_.history->current().reel.tracks[static_cast<size_t>(track)];
  assert(row.is_sound);
  const double top = TrackTop(track);
  deck::PaintLabel(painter, QRectF(2.0, top + 1.0, kHeaderWidth - 24.0, 18.0),
                   TrackName(track), false, row.is_muted);
  // Boom box buttons: black plastic keys whose letter lights up.
  const auto button = [painter](const QRectF& box, const QString& text,
                                bool is_on, const QColor& lit) {
    painter->fillRect(box, is_on ? deck::kPlasticDark : deck::kPlasticLight);
    painter->setPen(deck::kPlasticDark);
    painter->drawLine(box.bottomLeft(), box.bottomRight());
    painter->setPen(is_on ? lit : deck::kLabelShut);
    painter->drawText(box, Qt::AlignCenter, text);
  };
  button(MuteRect(track), tr("M"), row.is_muted, theme::kPick);
  button(SoloRect(track), tr("S"), row.is_solo, theme::kHandle);
  const bool can_arm = managers_.record != nullptr;
  if (can_arm) {
    button(ArmRect(track), tr("R"),
           managers_.record->armed_track() == track, theme::kPick);
  }
  // The fader: a sunken slot filled to the track's volume.
  const QRectF slot = FaderRect(track);
  deck::PaintDisplay(painter, slot);
  painter->fillRect(QRectF(slot.left(), slot.top(),
                           slot.width() * GainToLevel(row.volume),
                           slot.height()),
                    deck::kGlowDim);
  const double knob = slot.left() + slot.width() * GainToLevel(row.volume);
  painter->fillRect(QRectF(knob - 1.5, slot.top() - 2.0, 3.0,
                           slot.height() + 4.0),
                    theme::kHandle);
}

void ReelTimeline::PaintSoundBody(QPainter* painter, const Clip& clip,
                                  const QRectF& box) const {
  assert(painter != nullptr);
  assert(IsSoundClip(clip));
  const Reel& reel = managers_.history->current().reel;
  const auto& source = std::get<SoundSource>(clip.source);
  const SoundManager* sounds = managers_.sounds;
  const bool is_missing =
      sounds != nullptr && !sounds->ErrorOf(source.path).isEmpty();
  // A sound clip is a strip of display glass with its wave glowing on it.
  deck::PaintDisplay(painter, box);
  if (is_missing) {
    painter->fillRect(box, kMissingSound);
  }
  PaintHatch(painter, clip, box);
  // Sound under a muted track still shows, dimmed.
  const int track = FindClip(reel, clip.id).track;
  const bool is_heard = track >= 0 && IsTrackHeard(reel, track);
  QColor wave = deck::kGlow;
  wave.setAlphaF(is_heard ? 0.85f : 0.3f);
  const double middle = box.center().y();
  const double half = box.height() / 2.0 - 3.0;
  const double left = std::max(box.left(), kHeaderWidth);
  const double right = std::min(box.right(), static_cast<double>(width()));
  const double per_pixel = 1.0 / (zoom() * kFramesPerSecond);
  const double fade_in = clip.fade_in.index() * zoom();
  const double fade_out = clip.fade_out.index() * zoom();
  const bool has_wave = sounds != nullptr && !is_missing;
  // The display's center line, so a quiet stretch still reads as sound.
  painter->setPen(deck::kGlowDim);
  painter->drawLine(QPointF(left, middle), QPointF(right, middle));
  painter->setPen(wave);
  for (double x = left; has_wave && x < right; x += 1.0) {
    const double into = (x - box.left()) / zoom();
    const double seconds = (clip.in.index() + into) / kFramesPerSecond;
    // The wave shows the volume line and fades, so a duck shows.
    const double fade =
        std::min({1.0, (x - box.left()) / std::max(fade_in, 1e-9),
                  (box.right() - x) / std::max(fade_out, 1e-9)});
    const double gain = clip.volume * std::clamp(fade, 0.0, 1.0);
    const double level =
        std::min(1.0, sounds->PeakBetween(source.path, seconds,
                                          seconds + per_pixel) *
                          gain) *
        half;
    painter->drawLine(QPointF(x, middle - level), QPointF(x, middle + level));
  }
  const bool has_label_room = box.width() > 10.0;
  if (has_label_room) {
    deck::PaintGlowText(
        painter,
        QRectF(box.left() + 6, box.bottom() - 17, box.width() - 10, 14),
        is_missing ? tr("%1 (find file)").arg(QFileInfo(source.path).fileName())
                   : QFileInfo(source.path).fileName(),
        is_heard);
  }
  // The volume line, sloping down over the fades.
  const double y = VolumeY(box, clip.volume);
  QPainterPath line(QPointF(box.left(), box.bottom() - 2.0));
  line.lineTo(box.left() + fade_in, y);
  line.lineTo(box.right() - fade_out, y);
  line.lineTo(box.right(), box.bottom() - 2.0);
  painter->setPen(QPen(theme::kHandle, 1.0));
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(line);
  const bool is_picked = managers_.selection->clips().contains(clip.id);
  if (is_picked) {
    painter->fillRect(QRectF(box.left() + fade_in, box.top(), kFadeHandle,
                             kFadeHandle),
                      theme::kHandle);
    painter->fillRect(QRectF(box.right() - fade_out - kFadeHandle, box.top(),
                             kFadeHandle, kFadeHandle),
                      theme::kHandle);
  }
}

ReelTimeline::Drag ReelTimeline::SoundDragFor(ClipId clip,
                                              QPointF at) const {
  assert(clip.IsValid());
  assert(managers_.history != nullptr);
  const Clip* found = ClipOf(managers_.history->current().reel, clip);
  const bool is_sound = found != nullptr && IsSoundClip(*found);
  if (!is_sound) {
    return Drag::kNone;
  }
  const QRectF box = ClipRect(clip);
  const double fade_in = found->fade_in.index() * zoom();
  const double fade_out = found->fade_out.index() * zoom();
  // Handles look small and grab big.
  const QRectF in_handle(box.left() + fade_in - kFadeHandle / 2.0,
                         box.top() - 2.0, kFadeHandle * 2.0,
                         kFadeHandle * 1.5);
  const QRectF out_handle(box.right() - fade_out - kFadeHandle * 1.5,
                          box.top() - 2.0, kFadeHandle * 2.0,
                          kFadeHandle * 1.5);
  if (in_handle.contains(at)) {
    return Drag::kFadeIn;
  }
  if (out_handle.contains(at)) {
    return Drag::kFadeOut;
  }
  const bool is_on_line =
      std::abs(at.y() - VolumeY(box, found->volume)) <= kLineReach &&
      at.x() > box.left() + kGrabReach && at.x() < box.right() - kGrabReach;
  return is_on_line ? Drag::kVolume : Drag::kNone;
}

bool ReelTimeline::PressSoundHeader(QPointF at) {
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  assert(managers_.reel != nullptr);
  const int track = TrackAt(at.y());
  const bool is_header = track >= 0 && at.x() < kHeaderWidth;
  const Reel& reel = managers_.history->current().reel;
  const bool is_sound =
      is_header && reel.tracks[static_cast<size_t>(track)].is_sound;
  if (!is_sound) {
    return false;
  }
  const ReelTrack& row = reel.tracks[static_cast<size_t>(track)];
  if (MuteRect(track).contains(at)) {
    Report(ProblemOf(managers_.reel->SetTrackMuted(track, !row.is_muted)));
    return true;
  }
  if (SoloRect(track).contains(at)) {
    Report(ProblemOf(managers_.reel->SetTrackSolo(track, !row.is_solo)));
    return true;
  }
  const bool is_arm =
      managers_.record != nullptr && ArmRect(track).contains(at);
  if (is_arm) {
    Report(ProblemOf(managers_.record->ToggleArm(track)));
    update();
    return true;
  }
  const bool is_fader = FaderRect(track).adjusted(-2, -4, 2, 4).contains(at);
  if (!is_fader) {
    return true;
  }
  drag_ = Drag::kFader;
  drag_track_ = track;
  press_ = at;
  has_moved_ = false;
  drag_playhead_.Begin();
  // A press alone changes nothing; the drag sets the volume.
  scope_ = std::make_unique<EditScope>(managers_.history,
                                       tr("Change track volume"));
  return true;
}

bool ReelTimeline::ResetSoundAt(QPointF at) {
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  assert(managers_.reel != nullptr);
  const int track = TrackAt(at.y());
  const Reel& reel = managers_.history->current().reel;
  const bool is_sound_track =
      track >= 0 && reel.tracks[static_cast<size_t>(track)].is_sound;
  if (!is_sound_track) {
    return false;
  }
  const bool is_fader =
      at.x() < kHeaderWidth &&
      FaderRect(track).adjusted(-2, -4, 2, 4).contains(at);
  if (is_fader) {
    Report(ProblemOf(managers_.reel->SetTrackVolume(track, 1.0)));
    return true;
  }
  const ClipId clip = ClipAt(at);
  const bool is_line =
      clip.IsValid() && SoundDragFor(clip, at) == Drag::kVolume;
  if (is_line) {
    Report(ProblemOf(managers_.reel->SetClipVolume(clip, 1.0)));
    return true;
  }
  return false;
}

void ReelTimeline::StartSoundDrag(ClipId clip, Drag drag, QPointF at) {
  assert(clip.IsValid());
  assert(drag == Drag::kVolume || drag == Drag::kFadeIn ||
         drag == Drag::kFadeOut);
  drag_ = drag;
  dragged_ = clip;
  press_ = at;
  has_moved_ = false;
  drag_playhead_.Begin();
  const QString label = drag == Drag::kVolume   ? tr("Change volume")
                        : drag == Drag::kFadeIn ? tr("Fade in")
                                                : tr("Fade out");
  scope_ = std::make_unique<EditScope>(managers_.history, label);
}

void ReelTimeline::FollowSound(QPointF at) {
  assert(scope_ != nullptr);
  assert(managers_.reel != nullptr);
  const bool is_fader = drag_ == Drag::kFader;
  if (is_fader) {
    const QRectF slot = FaderRect(drag_track_);
    const double gain = LevelToGain((at.x() - slot.left()) / slot.width());
    Report(ProblemOf(managers_.reel->SetTrackVolume(drag_track_, gain)));
    feedback_.Update(press_, at, GainText(gain), false);
    return;
  }
  const Clip* clip = ClipOf(managers_.history->before().reel, dragged_);
  if (clip == nullptr) {
    return;
  }
  has_moved_ = true;
  const QRectF box = ClipRect(dragged_);
  const bool is_volume = drag_ == Drag::kVolume;
  if (is_volume) {
    const double level = (box.bottom() - 2.0 - at.y()) / (box.height() - 4.0);
    const double gain = LevelToGain(level);
    Report(ProblemOf(managers_.reel->SetClipVolume(dragged_, gain)));
    feedback_.Update(press_, at, GainText(gain), false);
    return;
  }
  // Fades snap to whole frames, dragged inward from the corners.
  const bool is_in = drag_ == Drag::kFadeIn;
  const double inward = is_in ? at.x() - box.left() : box.right() - at.x();
  const Frame frames(static_cast<int>(std::lround(std::max(inward, 0.0) /
                                                  zoom())));
  Report(ProblemOf(is_in ? managers_.reel->SetFadeIn(dragged_, frames)
                         : managers_.reel->SetFadeOut(dragged_, frames)));
  feedback_.Update(press_, at, FramesText(frames.index()), false);
}

void ReelTimeline::AddSoundTrackEntries(QMenu* menu, int track) {
  assert(menu != nullptr);
  assert(track >= 0);
  const ReelTrack& row =
      managers_.history->current().reel.tracks[static_cast<size_t>(track)];
  if (!row.is_sound) {
    return;
  }
  menu->addSeparator();
  QAction* mute = menu->addAction(tr("Mute"));
  mute->setCheckable(true);
  mute->setChecked(row.is_muted);
  const bool is_muted = row.is_muted;
  connect(mute, &QAction::triggered, this, [this, track, is_muted] {
    Report(ProblemOf(managers_.reel->SetTrackMuted(track, !is_muted)));
  });
  QAction* solo = menu->addAction(tr("Solo"));
  solo->setCheckable(true);
  solo->setChecked(row.is_solo);
  const bool is_solo = row.is_solo;
  connect(solo, &QAction::triggered, this, [this, track, is_solo] {
    Report(ProblemOf(managers_.reel->SetTrackSolo(track, !is_solo)));
  });
  const bool can_arm = managers_.record != nullptr;
  if (can_arm) {
    QAction* arm = menu->addAction(tr("Arm for recording"));
    arm->setCheckable(true);
    arm->setChecked(managers_.record->armed_track() == track);
    arm->setToolTip(tr("Takes from Rec go onto this track."));
    connect(arm, &QAction::triggered, this, [this, track] {
      Report(ProblemOf(managers_.record->ToggleArm(track)));
      update();
    });
  }
}

void ReelTimeline::PreferPicked() {
  assert(managers_.selection != nullptr);
  const auto& picked = managers_.selection->clips();
  const bool is_one = picked.size() == 1;
  if (is_one && managers_.sounds != nullptr) {
    managers_.sounds->PreferClip(*picked.begin());
  }
}

}  // namespace snapper
