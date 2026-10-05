#include "ui/rig_panel.h"

#include <cassert>

#include "edit/doll_library_manager.h"
#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "ui/form_helpers.h"
#include "ui/live_edit.h"
#include "ui/problem.h"

namespace snapper {

RigPanel::RigPanel(const Managers& managers)
    : managers_(managers),
      layout_(this),
      live_(managers.history),
      chains_title_(tr("IK chains")),
      add_chain_(tr("Add for piece")),
      flip_chain_(tr("Flip bend")),
      remove_chain_(tr("Remove")),
      copy_piece_(tr("Copy to other side")),
      copy_side_(tr("Copy whole side")),
      save_rig_(tr("Save rig to library")),
      hint_(tr("Drag a yellow joint to move it. Double-click a joint, then "
               "click the part it hangs from. Drag a square to move an IK "
               "tip.")) {
  assert(managers_.IsComplete());
  BuildLayout();
  Wire();
  Refresh();
  assert(layout_.count() > 0);
}

void RigPanel::BuildLayout() {
  assert(layout_.count() == 0);
  layout_.setContentsMargins(4, 4, 4, 4);
  hint_.setWordWrap(true);
  order_.setRange(-1000, 1000);
  order_.setToolTip(tr("Higher draws in front."));
  SetUpNumber(&rest_, Number::kDegrees);
  rest_.setToolTip(tr("How the piece is turned when not posed."));
  warp_columns_.setPrefix(tr("across "));
  warp_rows_.setPrefix(tr("down "));
  for (QSpinBox* side : {&warp_columns_, &warp_rows_}) {
    side->setRange(1, kMaxWarpCells);
    side->setValue(kDefaultWarpCells);
  }
  warp_on_.setText(tr("Bend with a grid"));
  warp_on_.setToolTip(tr("Lets you push spots of the drawing, like a "
                         "crease in a shirt."));
  AddTitle(&form_, &joint_title_, tr("Joint"));
  form_.addRow(tr("Hangs from"), &parent_);
  form_.addRow(tr("Rest turn"), &rest_);
  form_.addRow(tr("Draw order"), &order_);
  form_.addRow(tr("Drawing"), &drawing_);
  AddTitle(&form_, &warp_title_, tr("Warp"));
  form_.addRow(QString(), &warp_on_);
  AddPair(&form_, tr("Cells"), &warp_row_, &warp_columns_, &warp_rows_);
  chain_buttons_.addWidget(&add_chain_);
  chain_buttons_.addWidget(&flip_chain_);
  chain_buttons_.addWidget(&remove_chain_);
  layout_.addWidget(&dolls_);
  layout_.addWidget(&pieces_, 2);
  layout_.addLayout(&form_);
  mirror_buttons_.addWidget(&copy_piece_);
  mirror_buttons_.addWidget(&copy_side_);
  layout_.addLayout(&mirror_buttons_);
  layout_.addWidget(&chains_title_);
  layout_.addWidget(&chains_, 1);
  layout_.addLayout(&chain_buttons_);
  layout_.addWidget(&save_rig_);
  layout_.addWidget(&hint_);
  assert(form_.rowCount() == 8);
}

void RigPanel::Wire() {
  assert(managers_.IsComplete());
  assert(form_.rowCount() == 8);
  RigManager* rig = managers_.rig;
  connect(&dolls_, &QComboBox::textActivated, this, [this](const QString& d) {
    doll_ = d;
    piece_.clear();
    emit DollPicked(doll_);
    Refresh();
  });
  connect(&pieces_, &QListWidget::currentTextChanged, this,
          [this](const QString& text) {
            piece_ = text.trimmed();
            emit PiecePicked(piece_);
            RefreshPiece();
          });
  connect(&parent_, &QComboBox::activated, this, [this, rig](int index) {
    const QString parent = index == 0 ? QString() : parent_.itemText(index);
    emit Problem(ProblemOf(rig->SetParent(doll_, piece_, parent)));
  });
  MakeLive(&order_, &live_, tr("Restack"), this, [this, rig] {
    emit Problem(ProblemOf(rig->SetOrder(doll_, piece_, order_.value())));
  });
  MakeLive(&rest_, &live_, tr("Rest turn"), this, [this, rig] {
    emit Problem(
        ProblemOf(rig->SetRestRotation(doll_, piece_, rest_.value())));
  });
  connect(&drawing_, &QComboBox::activated, this, [this, rig](int index) {
    emit Problem(ProblemOf(rig->SetDefaultDrawing(doll_, piece_, index - 1)));
  });
  const auto set_grid = [this, rig] {
    const bool is_on = warp_on_.isChecked();
    const WarpGrid grid = is_on ? WarpGrid{warp_columns_.value(),
                                           warp_rows_.value()}
                                : WarpGrid();
    emit Problem(ProblemOf(rig->SetWarpGrid(doll_, piece_, grid)));
  };
  connect(&warp_on_, &QCheckBox::clicked, this, set_grid);
  for (QSpinBox* side : {&warp_columns_, &warp_rows_}) {
    MakeLive(side, &live_, tr("Warp grid"), this, set_grid);
  }
  WireChains();
}

void RigPanel::WireChains() {
  RigManager* rig = managers_.rig;
  assert(rig != nullptr);
  assert(managers_.library != nullptr);
  connect(&add_chain_, &QPushButton::clicked, this, &RigPanel::AddChain);
  connect(&copy_piece_, &QPushButton::clicked, this, [this, rig] {
    emit Problem(ProblemOf(rig->CopyToOtherSide(doll_, piece_, false)));
  });
  connect(&copy_side_, &QPushButton::clicked, this, [this, rig] {
    emit Problem(ProblemOf(rig->CopyToOtherSide(doll_, piece_, true)));
  });
  connect(&flip_chain_, &QPushButton::clicked, this, [this, rig] {
    const Doll* doll = FindDoll(managers_.history->current(), doll_);
    const IkChain* chain =
        doll != nullptr && chains_.currentItem() != nullptr
            ? FindChain(doll->rig, chains_.currentItem()->text())
            : nullptr;
    const bool is_picked = chain != nullptr;
    if (is_picked) {
      emit Problem(ProblemOf(rig->SetChainBend(doll_, chain->name,
                                               !chain->bends_clockwise)));
    }
  });
  connect(&remove_chain_, &QPushButton::clicked, this, [this, rig] {
    const QListWidgetItem* item = chains_.currentItem();
    const bool is_picked = item != nullptr;
    if (is_picked) {
      emit Problem(ProblemOf(rig->RemoveChain(doll_, item->text())));
    }
  });
  connect(&save_rig_, &QPushButton::clicked, this, [this] {
    emit Problem(ProblemOf(managers_.library->SaveRig(doll_)));
  });
  connect(&chains_, &QListWidget::currentRowChanged, this,
          &RigPanel::RefreshChains);
  connect(managers_.history, &HistoryManager::Changed, this,
          &RigPanel::Refresh);
}

}  // namespace snapper
