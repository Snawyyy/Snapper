#include "ui/link_box.h"

#include <algorithm>
#include <cassert>

#include "edit/history_manager.h"
#include "edit/link_manager.h"
#include "edit/selection_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"

namespace snapper {

LinkBox::LinkBox(const Managers& managers)
    : QGroupBox(tr("Link")), managers_(managers), layout_(this),
      live_(managers.history) {
  assert(managers_.IsComplete());
  follows_.setWordWrap(true);
  follows_.setObjectName("follows");
  SetUpNumber(&strength_, Number::kScale);
  strength_.setMaximum(kMaxLinkStrength);
  strength_.setObjectName("strength");
  strength_.setToolTip(tr("How much of the movement it copies: 1 the "
                          "same, 0.5 half, 0 none, 2 twice as much."));
  unlink_.setText(tr("Unlink"));
  unlink_.setObjectName("unlink");
  layout_.addRow(&follows_);
  layout_.addRow(tr("Strength"), &strength_);
  layout_.addRow(&unlink_);
  MakeLive(&strength_, &live_, tr("Link strength"), this,
           [this] { Commit(); });
  connect(&unlink_, &QPushButton::clicked, this, [this] {
    emit Problem(ProblemOf(
        managers_.links->Unlink(managers_.selection->shot(), Picked())));
  });
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 3);
}

std::vector<LinkEnd> LinkBox::Picked() const {
  assert(managers_.selection != nullptr);
  std::vector<LinkEnd> ends;
  for (const Pick& pick : managers_.selection->picks()) {
    ends.push_back(LinkEnd{pick.layer, pick.piece});
  }
  // The focused layer's picks come first.
  std::ranges::stable_partition(ends, [this](const LinkEnd& end) {
    return end.layer == managers_.selection->layer();
  });
  assert(ends.size() == managers_.selection->picks().size());
  return ends;
}

const Link* LinkBox::Shown() const {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  const Shot* shot =
      FindShot(managers_.history->current(), managers_.selection->shot());
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return nullptr;
  }
  for (const LinkEnd& end : Picked()) {
    const Link* link = FindLink(*shot, end);
    const bool is_linked = link != nullptr;
    if (is_linked) {
      return link;
    }
  }
  return nullptr;
}

void LinkBox::Refresh() {
  assert(managers_.links != nullptr);
  assert(managers_.selection != nullptr);
  const Link* link = Shown();
  const bool is_linked = link != nullptr;
  const QString why_not =
      is_linked ? QString()
                      : tr("Right-click something on the stage to link the "
                           "picked things' movement to it.");
  Explain(&strength_, why_not);
  Explain(&unlink_, why_not);
  if (!is_linked) {
    follows_.setText(tr("Follows nothing."));
    return;
  }
  follows_.setText(
      tr("Follows %1 from frame %2.")
          .arg(managers_.links->NameOf(managers_.selection->shot(),
                                       link->leader))
          .arg(link->from.index()));
  ShowNumber(&strength_, link->strength);
}

void LinkBox::Commit() {
  assert(managers_.links != nullptr);
  assert(managers_.selection != nullptr);
  const Link* link = Shown();
  const bool is_linked = link != nullptr;
  if (!is_linked) {
    return;
  }
  const double delta = strength_.value() - link->strength;
  emit Problem(ProblemOf(managers_.links->ShiftStrength(
      managers_.selection->shot(), Picked(), delta)));
}

}  // namespace snapper
