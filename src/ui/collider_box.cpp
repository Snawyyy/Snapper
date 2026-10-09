#include "ui/collider_box.h"

#include <algorithm>
#include <cassert>

#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "ui/problem.h"

namespace snapper {

ColliderBox::ColliderBox(const Managers& managers)
    : QGroupBox(tr("Collider")), managers_(managers), layout_(this) {
  assert(managers_.IsComplete());
  setObjectName("collider_box");
  note_.setWordWrap(true);
  layout_.addWidget(&note_);
  setVisible(false);
  assert(layout_.count() == 1);
}

void ColliderBox::Show(const QString& doll, const QString& piece) {
  assert(managers_.history != nullptr);
  assert(doll.size() < 100000 && piece.size() < 100000);
  doll_ = doll;
  piece_ = piece;
  const Doll* found = doll.isEmpty()
                          ? nullptr
                          : FindDoll(managers_.history->current(), doll);
  const RigPiece* rig = found != nullptr && !piece.isEmpty()
                            ? FindRig(found->rig, piece)
                            : nullptr;
  const bool is_collider = rig != nullptr && rig->collider.IsOn();
  setVisible(is_collider);
  // A tick may be the one whose click led here: let it finish first.
  for (std::unique_ptr<QCheckBox>& old : ticks_) {
    old->hide();
    layout_.removeWidget(old.get());
    old.release()->deleteLater();
  }
  ticks_.clear();
  if (!is_collider) {
    return;
  }
  std::vector<QString> in_front;
  for (const RigPiece& other : found->rig.pieces) {
    const bool is_front = other.order > rig->order;
    if (is_front) {
      in_front.push_back(other.name);
    }
  }
  note_.setText(in_front.empty()
                    ? tr("No piece is drawn in front of %1, so it pushes "
                         "nothing.").arg(piece)
                    : tr("Pushes the cloth (physics dots) of these pieces "
                         "drawn in front of %1. Untick one to let it "
                         "through.").arg(piece));
  for (const QString& name : in_front) {
    auto tick = std::make_unique<QCheckBox>(name);
    const bool is_spared =
        std::find(rig->collider.spared.begin(), rig->collider.spared.end(),
                  name) != rig->collider.spared.end();
    tick->setChecked(!is_spared);
    tick->setToolTip(tr("Whether %1's physics dots are pushed out of %2.")
                         .arg(name, piece));
    connect(tick.get(), &QCheckBox::clicked, this, [this, name](bool on) {
      emit Problem(ProblemOf(managers_.rig->SetColliderPushes(
          doll_, piece_, name, on)));
    });
    layout_.addWidget(tick.get());
    ticks_.push_back(std::move(tick));
  }
}

}  // namespace snapper
