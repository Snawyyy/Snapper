#include "ui/look_box.h"

#include <cassert>

#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "ui/form_helpers.h"
#include "ui/plated_box.h"
#include "ui/problem.h"

namespace snapper {
namespace {

// What each tile's tooltip says it does.
QString LookHelp(DrawingLook look) {
  assert(static_cast<int>(look) < kDrawingLookCount);
  assert(static_cast<int>(look) >= 0);
  switch (look) {
    case DrawingLook::kCrisp:
      return LookBox::tr("Reads the nearest pixel: for real pixel art. "
                         "Stretched parts show square steps.");
    case DrawingLook::kSmooth:
      return LookBox::tr("Upscales each drawing and blends between its "
                         "pixels, so bent, turned and scaled parts stay "
                         "smooth.");
    case DrawingLook::kSmoothFlat:
      return LookBox::tr("Smooth, but fills keep the drawing's own colors: "
                         "only the edges soften.");
  }
  return QString();
}

}  // namespace

LookBox::LookBox(const Managers& managers)
    : managers_(managers),
      knob_({tr("Crisp"), tr("Smooth"), tr("Flat")}),
      hard_edges_(tr("Hard edges")) {
  assert(managers_.IsComplete());
  assert(managers_.rig != nullptr);
  PlateBox(this, tr("Look"), &layout_);
  hard_edges_.setObjectName("hard_edges");
  row_.addWidget(&knob_, 1);
  row_.addWidget(&preview_, 0, Qt::AlignVCenter);
  layout_.addLayout(&row_);
  layout_.addWidget(&hard_edges_);
  connect(&knob_, &ModeKnob::Picked, this, [this](int look) {
    emit Problem(ProblemOf(
        managers_.rig->SetLook(doll_, static_cast<DrawingLook>(look))));
  });
  connect(&hard_edges_, &QAbstractButton::clicked, this, [this](bool is_on) {
    emit Problem(ProblemOf(managers_.rig->SetHardEdges(doll_, is_on)));
  });
  connect(managers_.history, &HistoryManager::Changed, this,
          &LookBox::Refresh);
  Refresh();
}

void LookBox::SetDoll(const QString& doll) {
  assert(doll.size() < 100000);
  doll_ = doll;
  Refresh();
}

void LookBox::Refresh() {
  assert(managers_.history != nullptr);
  assert(knob_.value() >= 0);
  const Doll* doll = doll_.isEmpty()
                         ? nullptr
                         : FindDoll(managers_.history->current(), doll_);
  const QString why_not =
      doll == nullptr ? tr("Import a doll first.") : QString();
  const DrawingLook look =
      doll == nullptr ? DrawingLook::kCrisp : doll->rig.look;
  Explain(&knob_, why_not, LookHelp(look));
  const bool is_crisp = look == DrawingLook::kCrisp;
  Explain(&hard_edges_,
          doll == nullptr ? why_not
          : is_crisp      ? tr("Crisp edges are already hard.")
                          : QString(),
          tr("Smooth edges are cut out instead of blended into what is "
             "behind: a cutout look."));
  const bool has_hard = doll != nullptr && doll->rig.hard_edges;
  knob_.SetValue(static_cast<int>(look));
  hard_edges_.setChecked(has_hard);
  preview_.Show(look, has_hard);
}

}  // namespace snapper
