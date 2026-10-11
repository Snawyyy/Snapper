#include "ui/collider_box.h"

#include <algorithm>
#include <cassert>

#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"

namespace snapper {

namespace {

// What the list says about a collider piece.
QString NoteFor(const QString& piece, bool has_others) {
  assert(!piece.isEmpty());
  assert(piece.size() < 100000);
  return has_others
             ? ColliderBox::tr(
                   "Pushes the warp dots of these pieces out of its "
                   "outline, front or back, so they never pass through it "
                   "(two hands with colliders push each other apart); what "
                   "overlaps as drawn stays. Untick one to let it through.")
             : ColliderBox::tr("%1 is the doll's only piece, so it pushes "
                               "nothing.").arg(piece);
}

}  // namespace

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
    const bool is_other = other.name != piece;
    if (is_other) {
      in_front.push_back(other.name);
    }
  }
  note_.setText(NoteFor(piece, !in_front.empty()));
  for (const QString& name : in_front) {
    auto tick = std::make_unique<QCheckBox>(name);
    const bool is_spared =
        std::find(rig->collider.spared.begin(), rig->collider.spared.end(),
                  name) != rig->collider.spared.end();
    tick->setChecked(!is_spared);
    const RigPiece* other = FindRig(found->rig, name);
    const bool has_grid = other != nullptr && other->warp.IsOn();
    Explain(tick.get(),
            has_grid ? QString()
                     : tr("%1 has no warp grid, so nothing of it can be "
                          "pushed. Give it a grid in the rig first.")
                           .arg(name),
            tr("Whether %1's warp dots are pushed out of %2.")
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
