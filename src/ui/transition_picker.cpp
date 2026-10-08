#include "ui/transition_picker.h"

#include <algorithm>
#include <cassert>

#include "edit/reel_manager.h"
#include "ui/form_helpers.h"

namespace snapper {
namespace {

// A swipe or fade picked over a cut starts this long, if it fits.
constexpr int kDefaultLength = 4;

}  // namespace

TransitionPicker::TransitionPicker(const Transition& now, Frame longest,
                                   QWidget* parent)
    : QDialog(parent),
      longest_(longest),
      layout_(this),
      buttons_(QDialogButtonBox::Ok | QDialogButtonBox::Cancel) {
  assert(longest_.index() >= 1);
  assert(static_cast<int>(now.kind) < kTransitionKindCount);
  setWindowTitle(tr("Transition"));
  kind_.setObjectName("transition_kind");
  length_.setObjectName("transition_length");
  for (int kind = 0; kind < kTransitionKindCount; ++kind) {
    kind_.addItem(TransitionName(static_cast<TransitionKind>(kind)));
  }
  kind_.setCurrentIndex(static_cast<int>(now.kind));
  length_.setRange(1, longest_.index());
  length_.setSuffix(tr(" frames"));
  const bool is_cut = now.kind == TransitionKind::kCut;
  length_.setValue(is_cut ? std::min(kDefaultLength, longest_.index())
                          : now.length.index());
  layout_.addRow(tr("Kind"), &kind_);
  layout_.addRow(tr("Length"), &length_);
  layout_.addRow(&buttons_);
  connect(&kind_, &QComboBox::currentIndexChanged, this, [this] { Show(); });
  connect(&buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(&buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Show();
}

Transition TransitionPicker::transition() const {
  assert(kind_.currentIndex() >= 0);
  assert(length_.value() >= 1);
  const auto kind = static_cast<TransitionKind>(kind_.currentIndex());
  const bool is_cut = kind == TransitionKind::kCut;
  return is_cut ? Transition() : Transition{kind, Frame(length_.value())};
}

void TransitionPicker::Show() {
  assert(kind_.count() == kTransitionKindCount);
  assert(longest_.index() >= 1);
  const bool is_cut = kind_.currentIndex() ==
                      static_cast<int>(TransitionKind::kCut);
  Explain(&length_, is_cut ? tr("A cut has no length.") : QString());
  const bool is_tight = longest_.index() == 1;
  length_.setToolTip(
      is_cut     ? tr("A cut has no length.")
      : is_tight ? tr("The clips are too short for a longer one.")
                 : tr("Frames the two clips are mixed over, before the "
                      "cut; at most %1 here.")
                       .arg(longest_.index()));
}

}  // namespace snapper
