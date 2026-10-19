#include "ui/character_box.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>

#include "edit/character_pose_edits.h"
#include "edit/character_pose_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"
#include "ui/follow.h"
#include "ui/panel_kit.h"
#include "ui/playhead.h"

namespace snapper {
namespace {

// Sends a double click on a slider to reset, so a smart bone's turn goes
// back to rest the way every dial and fader does.
class SliderReset final : public QObject {
 public:
  SliderReset(QSlider* slider, std::function<void()> reset)
      : QObject(slider), reset_(std::move(reset)) {
    assert(slider != nullptr);
    assert(reset_ != nullptr);
    slider->installEventFilter(this);
  }

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    assert(event != nullptr);
    assert(watched != nullptr);
    if (event->type() == QEvent::MouseButtonDblClick) {
      reset_();
      return true;
    }
    return QObject::eventFilter(watched, event);
  }

 private:
  std::function<void()> reset_;
};

}  // namespace

CharacterBox::CharacterBox(const Managers& managers)
    : QGroupBox(QObject::tr("Character")), managers_(managers),
      layout_(this) {
  assert(managers_.IsComplete());
  assert(managers_.character_poses != nullptr);
  reset_.setText(tr("Reset pose"));
  reset_.setToolTip(tr("Key the rest pose at the playhead"));
  flip_.setText(tr("Flip"));
  flip_.setCheckable(true);
  flip_.setToolTip(tr("Mirror the character left to right"));
  auto* buttons = new QWidget(this);
  auto* row = new QHBoxLayout(buttons);
  row->setContentsMargins(0, 0, 0, 0);
  row->addWidget(&reset_);
  row->addWidget(&flip_);
  layout_.addRow(EmptyHint(tr("Drag a bone's yellow tip on the stage to "
                              "bend; Shift turns only that bone."),
                           this));
  layout_.addRow(buttons);
  connect(&reset_, &QPushButton::clicked, this, [this] {
    const auto spot = SpotOf(managers_);
    if (spot.has_value()) {
      Report(managers_.character_poses->ResetPose(
          spot->shot, managers_.selection->layer(), spot->local));
    }
  });
  connect(&flip_, &QPushButton::toggled, this, [this](bool is_on) {
    if (is_filling_) return;
    Report(managers_.stage->SetFlipped(
        managers_.selection->shot(), managers_.selection->layer(), is_on));
  });
  Follow(managers_, this);
  Refresh();
}

void CharacterBox::Report(const Result<void>& done) {
  assert(managers_.character_poses != nullptr);
  assert(managers_.selection != nullptr);
  emit Problem(done ? QString() : done.error().message);
}

const Character* CharacterBox::Shown() const {
  assert(managers_.character_poses != nullptr);
  assert(managers_.selection != nullptr);
  return managers_.character_poses->CharacterOf(managers_.selection->shot(),
                                                managers_.selection->layer());
}

void CharacterBox::Rebuild(const Character& character) {
  assert(IsSound(character));
  assert(managers_.character_poses != nullptr);
  for (QWidget* row : rows_) {
    layout_.removeRow(row);
  }
  rows_.clear();
  switches_.clear();
  smart_bones_.clear();
  for (const CharacterPart& part : character.parts) {
    if (part.kind != PartKind::kSwitch) {
      continue;
    }
    auto* choices = new QComboBox(this);
    choices->setToolTip(tr("What %1 shows from here on").arg(part.name));
    for (PartId child : ChildrenOf(character, part.id)) {
      choices->addItem(FindPart(character, child)->name, child.value());
    }
    const PartId id = part.id;
    connect(choices, &QComboBox::activated, this, [this, choices, id] {
      const auto spot = SpotOf(managers_);
      if (!spot.has_value() || is_filling_) return;
      const PartId child(choices->currentData().toInt());
      Report(managers_.character_poses->Pose(
          spot->shot, managers_.selection->layer(), spot->local,
          tr("Switch %1").arg(choices->currentText()),
          [id, child](const CharacterPose& pose) {
            return WithPick(pose, id, child);
          }));
    });
    layout_.addRow(part.name, choices);
    rows_.push_back(choices);
    switches_.push_back(part.id);
  }
  for (const CharacterPart& part : character.parts) {
    for (const Bone& bone : part.bones) {
      if (!bone.is_smart) {
        continue;
      }
      auto* slider = new QSlider(Qt::Horizontal, this);
      slider->setRange(-180, 180);
      slider->setToolTip(
          tr("Turns %1, which plays its action; double click for rest")
              .arg(bone.name));
      const BoneId id = bone.id;
      const QString name = bone.name;
      connect(slider, &QSlider::sliderPressed, this, [this, name] {
        scope_ = std::make_unique<EditScope>(managers_.history,
                                             tr("Turn %1").arg(name));
      });
      connect(slider, &QSlider::sliderReleased, this, [this] { scope_.reset(); });
      auto turn_to = [this, id](double turn) {
        const auto spot = SpotOf(managers_);
        const Character* shown = Shown();
        if (!spot.has_value() || shown == nullptr || is_filling_) return;
        Report(managers_.character_poses->Pose(
            spot->shot, managers_.selection->layer(), spot->local,
            tr("Turn smart bone"), [shown, id, turn](const CharacterPose& pose) {
              return WithBoneTurn(*shown, pose, id, turn);
            }));
      };
      connect(slider, &QSlider::valueChanged, this, turn_to);
      new SliderReset(slider, [turn_to] { turn_to(0.0); });
      layout_.addRow(bone.name, slider);
      rows_.push_back(slider);
      smart_bones_.push_back(bone.id);
    }
  }
}

void CharacterBox::Refresh() {
  assert(managers_.selection != nullptr);
  assert(managers_.character_poses != nullptr);
  const Character* character = Shown();
  if (character == nullptr) {
    return;
  }
  is_filling_ = true;
  const QString key = character->name + QString::number(character->next_id) +
                      QString::number(character->parts.size());
  if (key != built_for_ && scope_ == nullptr) {
    Rebuild(*character);
    built_for_ = key;
  }
  const auto spot = SpotOf(managers_);
  const CharacterPose pose =
      spot.has_value() ? managers_.character_poses->HandPose(
                             spot->shot, managers_.selection->layer(), spot->local)
                       : CharacterPose{};
  for (size_t i = 0; i < switches_.size(); ++i) {
    auto* choices = static_cast<QComboBox*>(rows_[i]);
    const CharacterPart* part = FindPart(*character, switches_[i]);
    const auto own = pose.parts.find(switches_[i]);
    const PartId pick = own != pose.parts.end() && own->second.pick.IsValid()
                            ? own->second.pick
                            : part != nullptr ? part->rest_pick : PartId();
    choices->setCurrentIndex(std::max(0, choices->findData(pick.value())));
  }
  for (size_t i = 0; i < smart_bones_.size(); ++i) {
    auto* slider = static_cast<QSlider*>(rows_[switches_.size() + i]);
    const auto own = pose.bones.find(smart_bones_[i]);
    const double turn = own == pose.bones.end() ? 0.0 : own->second.turn;
    if (!slider->isSliderDown()) {
      slider->setValue(static_cast<int>(std::lround(turn)));
    }
  }
  const Shot* shot = FindShot(managers_.history->current(), managers_.selection->shot());
  const LayerId focus = managers_.selection->layer();
  const Layer* layer =
      shot != nullptr && focus.IsValid() ? FindLayer(*shot, focus) : nullptr;
  const auto* cast =
      layer != nullptr ? std::get_if<CharacterLayer>(&layer->content) : nullptr;
  flip_.setChecked(cast != nullptr && cast->is_flipped);
  is_filling_ = false;
}

}  // namespace snapper
