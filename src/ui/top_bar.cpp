#include "ui/top_bar.h"

#include <cassert>
#include <memory>

#include "edit/history_manager.h"
#include "ui/hover_fade.h"

namespace snapper {
namespace {

constexpr int kBarGap = 14;
constexpr int kStepMaxWidth = 190;
constexpr int kStepMinWidth = 64;

}  // namespace

TopBar::TopBar(const Managers& managers)
    : managers_(managers),
      layout_(this),
      tabs_(managers.motion),
      undo_(tr("Undo"), managers.motion),
      redo_(tr("Redo"), managers.motion) {
  assert(managers_.IsComplete());
  layout_.setContentsMargins(4, 4, 4, 2);
  layout_.setSpacing(2);
  tabs_.addTab(tr("Video"));
  tabs_.addTab(tr("Pose"));
  tabs_.addTab(tr("Comp"));
  tabs_.addTab(tr("Rig"));
  tabs_.setObjectName(QStringLiteral("modes"));
  layout_.addWidget(&tabs_);
  layout_.addSpacing(kBarGap);
  BuildSteps();
  layout_.addStretch();
  assert(tabs_.count() == 4);
}

void TopBar::BuildSteps() {
  assert(managers_.history != nullptr);
  assert(layout_.count() > 0);
  undo_.setObjectName(QStringLiteral("undo"));
  redo_.setObjectName(QStringLiteral("redo"));
  AddButton(&undo_, tr("Nothing to undo"));
  AddButton(&redo_, tr("Nothing to redo"));
  for (StepButton* step : {&undo_, &redo_}) {
    step->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    step->setMinimumWidth(kStepMinWidth);
    step->setMaximumWidth(kStepMaxWidth);
  }
  connect(&undo_, &QPushButton::clicked, this, &TopBar::UndoRequested);
  connect(&redo_, &QPushButton::clicked, this, &TopBar::RedoRequested);
}

void TopBar::AddButton(QPushButton* button, const QString& tip) {
  assert(button != nullptr);
  assert(!tip.isEmpty());
  button->setFocusPolicy(Qt::NoFocus);
  button->setToolTip(tip);
  // The button owns it.
  std::make_unique<HoverFade>(button, managers_.motion).release();
  layout_.addWidget(button);
}

void TopBar::ShowUndo(const StepFace& face) {
  assert(!face.text.isEmpty());
  assert(managers_.history != nullptr);
  undo_.ShowFace(face);
}

void TopBar::ShowRedo(const StepFace& face) {
  assert(!face.text.isEmpty());
  assert(managers_.history != nullptr);
  redo_.ShowFace(face);
}

}  // namespace snapper
