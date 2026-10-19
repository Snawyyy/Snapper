#include "ui/link_box.h"

#include <algorithm>
#include <cassert>

#include "edit/history_manager.h"
#include "edit/link_manager.h"
#include "edit/selection_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/pose_controls.h"
#include "ui/problem.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The strength fader spans 0 to this; past it the cap rests at the end.
constexpr double kStrengthShown = 2.0;

}  // namespace

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
  unlink_.setToolTip(tr("The picked things stop following."));
  // Like the Pose box: whom it follows on a plate over a sunken frame,
  // with Unlink under it, then how strongly on a fader.
  layout_.setVerticalSpacing(theme::kRowGap);
  layout_.addRow(MakePlated(tr("Follows"), &follows_, this, &unlink_));
  layout_.addRow(MakeFaderRow(tr("Strength"), &strength_, &live_,
                              kStrengthShown, 1.0, this));
  MakeLive(&strength_, &live_, tr("Link strength"), this,
           [this] { Commit(); });
  connect(&unlink_, &QPushButton::clicked, this, [this] {
    emit Problem(ProblemOf(
        managers_.links->Unlink(managers_.selection->shot(), Picked())));
  });
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 2);
}

std::vector<LinkEnd> LinkBox::Picked() const {
  assert(managers_.selection != nullptr);
  std::vector<LinkEnd> ends;
  for (const Pick& pick : managers_.selection->picks()) {
    ends.push_back(LinkEnd{pick.layer});
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

std::vector<double> LinkBox::Strengths() const {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  std::vector<double> strengths;
  const Shot* shot =
      FindShot(managers_.history->current(), managers_.selection->shot());
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return strengths;
  }
  for (const LinkEnd& end : Picked()) {
    const Link* link = FindLink(*shot, end);
    const bool is_linked = link != nullptr;
    if (is_linked) {
      strengths.push_back(link->strength);
    }
  }
  return strengths;
}

bool LinkBox::IsLinked() const {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  return Shown() != nullptr;
}

void LinkBox::Refresh() {
  assert(managers_.links != nullptr);
  assert(managers_.selection != nullptr);
  const Link* link = Shown();
  const bool is_linked = link != nullptr;
  // Unlinked, the inspector hides the box, so there is nothing to show.
  if (!is_linked) {
    return;
  }
  follows_.setText(
      tr("Follows %1 from frame %2.")
          .arg(managers_.links->NameOf(managers_.selection->shot(),
                                       link->leader))
          .arg(link->from.index()));
  ShowNumber(&strength_, link->strength, Strengths());
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
