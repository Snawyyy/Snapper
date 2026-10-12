#include "ui/look_box.h"

#include <QSignalBlocker>

#include <cassert>

#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "ui/form_helpers.h"
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
    : QGroupBox(tr("Look")), managers_(managers), layout_(this) {
  assert(managers_.IsComplete());
  assert(managers_.rig != nullptr);
  for (int i = 0; i < kDrawingLookCount; ++i) {
    const auto look = static_cast<DrawingLook>(i);
    QPushButton& tile = tiles_[static_cast<size_t>(i)];
    tile.setText(LookName(look));
    tile.setCheckable(true);
    tile.setMinimumHeight(36);
    tile.setToolTip(LookHelp(look));
    group_.addButton(&tile, i);
    tiles_row_.addWidget(&tile);
  }
  group_.setExclusive(true);
  hard_edges_.setText(tr("Hard edges"));
  hard_edges_.setToolTip(tr("Smooth edges are cut out instead of blended "
                            "into what is behind: a cutout look."));
  layout_.addLayout(&tiles_row_);
  layout_.addWidget(&hard_edges_);
  connect(&group_, &QButtonGroup::idClicked, this, [this](int id) {
    emit Problem(ProblemOf(
        managers_.rig->SetLook(doll_, static_cast<DrawingLook>(id))));
  });
  connect(&hard_edges_, &QCheckBox::clicked, this, [this](bool is_on) {
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
  assert(group_.buttons().size() == kDrawingLookCount);
  const Doll* doll = doll_.isEmpty()
                         ? nullptr
                         : FindDoll(managers_.history->current(), doll_);
  const QString why_not =
      doll == nullptr ? tr("Import a doll first.") : QString();
  for (int i = 0; i < kDrawingLookCount; ++i) {
    Explain(&tiles_[static_cast<size_t>(i)], why_not,
            LookHelp(static_cast<DrawingLook>(i)));
  }
  const bool is_crisp =
      doll == nullptr || doll->rig.look == DrawingLook::kCrisp;
  Explain(&hard_edges_,
          doll == nullptr ? why_not
          : is_crisp      ? tr("Crisp edges are already hard.")
                          : QString(),
          tr("Smooth edges are cut out instead of blended into what is "
             "behind: a cutout look."));
  if (doll == nullptr) {
    return;
  }
  const QSignalBlocker quiet(group_);
  tiles_[static_cast<size_t>(doll->rig.look)].setChecked(true);
  hard_edges_.setChecked(doll->rig.hard_edges);
}

}  // namespace snapper
