#include "ui/sound_clip_panel.h"

#include <QDir>
#include <QFileInfo>

#include <cassert>
#include <cmath>
#include <variant>

#include "edit/history_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "edit/sound_manager.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/pose_controls.h"
#include "ui/problem.h"

namespace snapper {
namespace {

constexpr double kPercent = 100.0;
// The fade faders show up to this many seconds; the fields go further.
constexpr double kShownFadeSeconds = 2.0;
constexpr double kMaxFadeSeconds = 60.0;
enum What { kVolume = 0, kFadeIn = 1, kFadeOut = 2 };

Frame FramesOf(double seconds) {
  assert(std::isfinite(seconds));
  assert(kFramesPerSecond > 0);
  return Frame(static_cast<int>(std::lround(seconds * kFramesPerSecond)));
}

void SetUpSeconds(QDoubleSpinBox* box) {
  assert(box != nullptr);
  assert(kMaxFadeSeconds > 0.0);
  box->setRange(0.0, kMaxFadeSeconds);
  box->setDecimals(2);
  box->setSingleStep(1.0 / kFramesPerSecond);
  box->setSuffix(QStringLiteral(" s"));
}

}  // namespace

SoundClipPanel::SoundClipPanel(const Managers& managers)
    : managers_(managers), live_(managers.history), layout_(this) {
  assert(managers_.IsComplete());
  assert(managers_.reel != nullptr);
  layout_.setContentsMargins(0, 0, 0, 0);
  auto* body = MakeOwned<QWidget>(this);
  auto* column = MakeOwned<QVBoxLayout>(body);
  column->setContentsMargins(0, 0, 0, 0);
  name_.setWordWrap(true);
  column->addWidget(&name_);
  volume_.setRange(0.0, kMaxGain * kPercent);
  volume_.setDecimals(0);
  volume_.setSuffix(QStringLiteral(" %"));
  volume_.setToolTip(tr("How loud the clip plays: 100 % is as recorded, "
                        "up to +6 dB. Double-click the fader for 100 %."));
  SetUpSeconds(&fade_in_);
  SetUpSeconds(&fade_out_);
  fade_in_.setToolTip(tr("How long the clip takes to rise from silence."));
  fade_out_.setToolTip(tr("How long the clip takes to fall to silence."));
  column->addWidget(MakeFaderRow(tr("Volume"), &volume_, &live_,
                                 kMaxGain * kPercent, kPercent, body));
  column->addWidget(MakeFaderRow(tr("Fade in"), &fade_in_, &live_,
                                 kShownFadeSeconds, 0.0, body));
  column->addWidget(MakeFaderRow(tr("Fade out"), &fade_out_, &live_,
                                 kShownFadeSeconds, 0.0, body));
  layout_.addWidget(PlatedSection(tr("Sound clip"), body, this));
  layout_.addStretch(1);
  MakeLive(&volume_, &live_, tr("Change clip volume"), this,
           [this] { Commit(Picked(), kVolume); });
  MakeLive(&fade_in_, &live_, tr("Change fade in"), this,
           [this] { Commit(Picked(), kFadeIn); });
  MakeLive(&fade_out_, &live_, tr("Change fade out"), this,
           [this] { Commit(Picked(), kFadeOut); });
  Refresh();
}

std::vector<ClipId> SoundClipPanel::Picked() const {
  assert(managers_.selection != nullptr);
  assert(managers_.history != nullptr);
  const Reel& reel = managers_.history->current().reel;
  std::vector<ClipId> clips;
  for (const ClipId id : managers_.selection->clips()) {
    const Clip* clip = ClipOf(reel, id);
    const bool is_sound = clip != nullptr && IsSoundClip(*clip);
    if (is_sound) {
      clips.push_back(id);
    }
  }
  return clips;
}

void SoundClipPanel::Commit(const std::vector<ClipId>& clips, int what) {
  assert(managers_.reel != nullptr);
  assert(what >= kVolume && what <= kFadeOut);
  ReelManager& reel = *managers_.reel;
  for (const ClipId clip : clips) {
    const Result<void> done =
        what == kVolume ? reel.SetClipVolume(clip, volume_.value() / kPercent)
        : what == kFadeIn ? reel.SetFadeIn(clip, FramesOf(fade_in_.value()))
                          : reel.SetFadeOut(clip, FramesOf(fade_out_.value()));
    Report(ProblemOf(done));
  }
}

void SoundClipPanel::Refresh() {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  const std::vector<ClipId> clips = Picked();
  const bool has_clip = !clips.empty();
  const QString why_not =
      has_clip ? QString() : tr("Pick a sound clip on the reel.");
  for (QWidget* box : {static_cast<QWidget*>(&volume_),
                       static_cast<QWidget*>(&fade_in_),
                       static_cast<QWidget*>(&fade_out_)}) {
    Explain(box, why_not);
  }
  if (!has_clip) {
    name_.setText(tr("No sound clip picked."));
    return;
  }
  const Clip& first = *ClipOf(managers_.history->current().reel, clips[0]);
  const auto& source = std::get<SoundSource>(first.source);
  const QString length = TimeText(first.length);
  name_.setText(clips.size() == 1
                    ? tr("%1\n%2 long").arg(QFileInfo(source.path).fileName(),
                                            length)
                    : tr("%1 sound clips").arg(clips.size()));
  name_.setToolTip(QDir::toNativeSeparators(source.path));
  ShowNumber(&volume_, first.volume * kPercent);
  ShowNumber(&fade_in_,
             static_cast<double>(first.fade_in.index()) / kFramesPerSecond);
  ShowNumber(&fade_out_,
             static_cast<double>(first.fade_out.index()) / kFramesPerSecond);
}

}  // namespace snapper
