#include "ui/shot_clip_panel.h"

#include <QFormLayout>
#include <QSignalBlocker>

#include <algorithm>
#include <cassert>
#include <variant>

#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "edit/shot_manager.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/problem.h"

namespace snapper {
namespace {

constexpr int kMaxShotFrames = 100000;

}  // namespace

ShotClipPanel::ShotClipPanel(const Managers& managers)
    : managers_(managers),
      live_(managers.history),
      layout_(this),
      hold_(tr("Hold")),
      stretch_(tr("Stretch")),
      open_(tr("Open on the Pose tab")) {
  assert(managers_.IsComplete());
  assert(managers_.reel != nullptr);
  layout_.setContentsMargins(0, 0, 0, 0);
  auto* shot_body = MakeOwned<QWidget>(this);
  QFormLayout* form = SettingsForm(shot_body);
  form->addRow(tr("Name"), &name_);
  length_.setRange(1, kMaxShotFrames);
  length_.setSuffix(tr(" frames"));
  form->addRow(tr("Length"), &length_);
  form->addRow(QString(), &seconds_);
  form->addRow(&open_);
  layout_.addWidget(PlatedSection(tr("Shot clip"), shot_body, this));
  auto* edges_body = MakeOwned<QWidget>(this);
  auto* edges_column = MakeOwned<QVBoxLayout>(edges_body);
  edges_column->setContentsMargins(0, 0, 0, 0);
  for (QPushButton* tile : {&hold_, &stretch_}) {
    tile->setCheckable(true);
    tile->setFocusPolicy(Qt::NoFocus);
  }
  hold_.setToolTip(tr("Dragging a shot clip's edge adds or takes away "
                      "frames; new frames hold the last pose."));
  stretch_.setToolTip(tr("Dragging a shot clip's edge moves every key in "
                         "proportion, so the action plays faster or "
                         "slower."));
  edges_column->addWidget(TileRow({&hold_, &stretch_}, edges_body));
  layout_.addWidget(PlatedSection(tr("Edges"), edges_body, this));
  layout_.addStretch(1);
  connect(&name_, &QLineEdit::editingFinished, this, &ShotClipPanel::Rename);
  MakeLiveNamed(
      &length_, &live_, this,
      [this] {
        const ClipId clip = PickedClip();
        return managers_.reel->ResizeLabel({clip}, clip);
      },
      [this] { Resize(); });
  connect(&open_, &QPushButton::clicked, this, &ShotClipPanel::Open);
  const auto set_mode = [this](EdgeMode mode) {
    managers_.reel->SetEdgeMode(mode);
    Refresh();
    emit EdgeModeChanged();
  };
  connect(&hold_, &QPushButton::clicked, this,
          [set_mode] { set_mode(EdgeMode::kHold); });
  connect(&stretch_, &QPushButton::clicked, this,
          [set_mode] { set_mode(EdgeMode::kStretch); });
  Refresh();
}

ClipId ShotClipPanel::PickedClip() const {
  assert(managers_.selection != nullptr);
  assert(managers_.history != nullptr);
  const auto& picked = managers_.selection->clips();
  const bool is_one = picked.size() == 1;
  if (!is_one) {
    return ClipId();
  }
  const ClipId clip = *picked.begin();
  return ShotOfClip(clip).IsValid() ? clip : ClipId();
}

ShotId ShotClipPanel::ShotOfClip(ClipId clip) const {
  assert(managers_.history != nullptr);
  assert(clip.value() >= 0);
  const Clip* found = ClipOf(managers_.history->before().reel, clip);
  const auto* shot =
      found != nullptr ? std::get_if<ShotSource>(&found->source) : nullptr;
  return shot != nullptr ? shot->shot : ShotId();
}

void ShotClipPanel::Rename() {
  assert(managers_.shots != nullptr);
  assert(managers_.history != nullptr);
  const ClipId clip = PickedClip();
  const ShotId shot = clip.IsValid() ? ShotOfClip(clip) : ShotId();
  const Shot* found =
      shot.IsValid() ? FindShot(managers_.history->current(), shot) : nullptr;
  const bool is_new_name =
      found != nullptr && !name_.text().trimmed().isEmpty() &&
      name_.text() != found->name;
  if (is_new_name) {
    Report(ProblemOf(managers_.shots->Rename(shot, name_.text().trimmed())));
  }
}

void ShotClipPanel::Resize() {
  assert(managers_.reel != nullptr);
  assert(managers_.history != nullptr);
  const ClipId clip = PickedClip();
  const Project& start = managers_.history->before();
  const Clip* found = clip.IsValid() ? ClipOf(start.reel, clip) : nullptr;
  const Shot* shot = found != nullptr
                         ? FindShot(start, std::get<ShotSource>(found->source)
                                               .shot)
                         : nullptr;
  if (shot == nullptr) {
    return;
  }
  // The clip's end moves by the change, as an end drag would move it.
  const int delta = length_.value() - shot->length.index();
  const Frame edge(std::max(found->start.index() + 1,
                            found->end().index() + delta));
  const auto resized =
      managers_.reel->ResizeShots({clip}, clip, false, edge, false);
  Report(ProblemOf(resized));
  const bool is_stopped = resized.has_value() && !resized->stopped.isEmpty();
  if (is_stopped) {
    Report(resized->stopped);
  }
}

void ShotClipPanel::Open() {
  assert(managers_.playback != nullptr);
  assert(managers_.history != nullptr);
  const ClipId clip = PickedClip();
  const Clip* found =
      clip.IsValid() ? ClipOf(managers_.history->current().reel, clip)
                     : nullptr;
  if (found == nullptr) {
    return;
  }
  const Frame now = managers_.playback->FrameOn(Timeline::kReel);
  const bool is_inside = found->start <= now && now < found->end();
  const Frame inside(is_inside ? found->in.index() + now.index() -
                                     found->start.index()
                               : found->in.index());
  emit OpenShotAsked(ShotOfClip(clip), inside);
}

void ShotClipPanel::Refresh() {
  assert(managers_.reel != nullptr);
  assert(managers_.history != nullptr);
  const bool is_stretch = managers_.reel->edge_mode() == EdgeMode::kStretch;
  hold_.setChecked(!is_stretch);
  stretch_.setChecked(is_stretch);
  const ClipId clip = PickedClip();
  const ShotId id = clip.IsValid() ? ShotOfClip(clip) : ShotId();
  const Shot* shot =
      id.IsValid() ? FindShot(managers_.history->current(), id) : nullptr;
  const QString why_not =
      shot != nullptr ? QString()
      : clip.IsValid() ? tr("That clip's shot was removed.")
                       : tr("Pick one shot clip to name or time its shot.");
  Explain(&name_, why_not);
  Explain(&length_, why_not,
          tr("The shot's length. Changing it resizes the shot as dragging "
             "the clip's end does: by the Edges mode, with every full clip "
             "of it following."));
  Explain(&open_, why_not,
          tr("Opens the shot on the Pose tab at the playhead (or "
             "double-click the clip)."));
  if (shot == nullptr) {
    seconds_.clear();
    return;
  }
  const bool is_typing = name_.hasFocus();
  if (!is_typing) {
    name_.setText(shot->name);
  }
  ShowNumber(&length_, shot->length.index());
  seconds_.setText(tr("%1 long").arg(TimeText(shot->length)));
}

}  // namespace snapper
