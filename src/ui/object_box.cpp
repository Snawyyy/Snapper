#include "ui/object_box.h"

#include <QSignalBlocker>

#include <algorithm>
#include <cassert>

#include "edit/object_manager.h"
#include "edit/selection_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/playhead.h"
#include "ui/problem.h"

namespace snapper {

ObjectBox::ObjectBox(const Managers& managers)
    : QGroupBox(tr("Object")), managers_(managers), layout_(this) {
  assert(managers_.IsComplete());
  assert(managers_.objects != nullptr);
  mode_.addItem(tr("Loop"));
  mode_.addItem(tr("Once"));
  mode_.addItem(tr("Ping-pong"));
  mode_.addItem(tr("Hold"));
  assert(mode_.count() == kPlayModeCount);
  mode_.setObjectName("play_mode");
  mode_.setToolTip(tr("How an animated picture plays. Hold stays on the "
                      "keyed drawing, for picking a frame by hand."));
  speed_.setRange(0, kMaxFramesPerDrawing);
  speed_.setSpecialValueText(tr("Own timing"));
  speed_.setSuffix(tr(" frames each"));
  speed_.setObjectName("frames_per_drawing");
  speed_.setToolTip(tr("Shot frames each picture frame shows for: 2 is on "
                       "twos. Own timing keeps the GIF's."));
  start_.setRange(0, kMaxFrame);
  start_.setObjectName("play_start");
  start_.setToolTip(tr("The shot frame it starts playing on."));
  offset_.setRange(0, kMaxObjectFrames * 2);
  offset_.setObjectName("play_offset");
  offset_.setToolTip(tr("How many frames in it starts, so two copies do "
                        "not flicker in step."));
  holds_.setWordWrap(true);
  holds_.setObjectName("holds");
  drop_.setText(tr("Drop here"));
  drop_.setObjectName("drop");
  layout_.addRow(tr("Play"), &mode_);
  layout_.addRow(tr("Speed"), &speed_);
  layout_.addRow(tr("Start"), &start_);
  layout_.addRow(tr("Offset"), &offset_);
  layout_.addRow(&holds_);
  layout_.addRow(&drop_);
  connect(&mode_, &QComboBox::activated, this, [this] { Commit(); });
  for (QSpinBox* box : {&speed_, &start_, &offset_}) {
    connect(box, &QSpinBox::valueChanged, this, [this] { Commit(); });
  }
  connect(&drop_, &QPushButton::clicked, this, [this] {
    const auto spot = SpotOf(managers_);
    if (spot) {
      emit Problem(ProblemOf(
          managers_.objects->Drop(spot->shot, Picked(), spot->local)));
    }
  });
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 6);
}

std::vector<LayerId> ObjectBox::Picked() const {
  assert(managers_.selection != nullptr);
  assert(managers_.objects != nullptr);
  std::vector<LayerId> picked = managers_.objects->ObjectLayers(
      managers_.selection->shot(), managers_.selection->PickedLayers());
  std::ranges::stable_partition(picked, [this](LayerId layer) {
    return layer == managers_.selection->layer();
  });
  return picked;
}

void ObjectBox::Refresh() {
  assert(managers_.objects != nullptr);
  assert(managers_.selection != nullptr);
  const std::vector<LayerId> picked = Picked();
  const bool has_object = !picked.empty();
  // Without an object the inspector hides the box.
  if (!has_object) {
    return;
  }
  const ShotId shot = managers_.selection->shot();
  const ObjectPlay play = managers_.objects->PlayOf(shot, picked.front());
  const bool is_animated =
      managers_.objects->FrameCount(shot, picked.front()) >= 2;
  {
    const QSignalBlocker quiet_mode(mode_);
    const QSignalBlocker quiet_speed(speed_);
    const QSignalBlocker quiet_start(start_);
    const QSignalBlocker quiet_offset(offset_);
    mode_.setCurrentIndex(static_cast<int>(play.mode));
    speed_.setValue(play.frames_per_drawing);
    start_.setValue(play.start.index());
    offset_.setValue(play.offset);
  }
  const QString still =
      is_animated ? QString() : tr("A still picture has no frames to play.");
  for (QWidget* control : {static_cast<QWidget*>(&mode_),
                           static_cast<QWidget*>(&speed_),
                           static_cast<QWidget*>(&start_),
                           static_cast<QWidget*>(&offset_)}) {
    // Each keeps its own help in a property, so a still picture's reason
    // never replaces it for good.
    const bool has_help = control->property("help").isValid();
    if (!has_help) {
      control->setProperty("help", control->toolTip());
    }
    Explain(control, still, control->property("help").toString());
  }
  const QStringList track = managers_.objects->HoldTrack(shot, picked.front());
  holds_.setText(track.isEmpty()
                     ? tr("Not held. Right click a doll's piece on the "
                          "stage to hold it with that.")
                     : tr("Holds (frame: who):\n%1").arg(track.join("\n")));
  const auto spot = SpotOf(managers_);
  Explain(&drop_, spot ? managers_.objects->WhyNoDrop(shot, picked,
                                                      spot->local)
                       : tr("Move the playhead into the shot first."));
}

void ObjectBox::Commit() {
  assert(managers_.objects != nullptr);
  assert(managers_.selection != nullptr);
  ObjectPlay play;
  play.mode = static_cast<PlayMode>(
      std::clamp(mode_.currentIndex(), 0, kPlayModeCount - 1));
  play.frames_per_drawing = speed_.value();
  play.start = Frame(start_.value());
  play.offset = offset_.value();
  emit Problem(ProblemOf(managers_.objects->SetPlay(
      managers_.selection->shot(), Picked(), play)));
}

}  // namespace snapper
