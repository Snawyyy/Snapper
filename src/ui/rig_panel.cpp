#include "ui/rig_panel.h"

#include <cassert>

#include "edit/doll_library_manager.h"
#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"

namespace snapper {

RigPanel::RigPanel(const Managers& managers)
    : managers_(managers),
      layout_(this),
      chains_title_(tr("IK chains")),
      add_chain_(tr("Add for piece")),
      flip_chain_(tr("Flip bend")),
      remove_chain_(tr("Remove")),
      save_rig_(tr("Save rig to library")),
      hint_(tr("Drag a yellow joint to move it; drag a square to move an "
               "IK tip.")) {
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
  SetUpNumber(&rest_, -360, 360, 1, tr(" deg"));
  rest_.setToolTip(tr("How the piece is turned when not posed."));
  for (QSpinBox* side : {&warp_columns_, &warp_rows_}) {
    side->setRange(0, kMaxWarpCells);
    side->setSpecialValueText(tr("off"));
  }
  form_.addRow(tr("Hangs from"), &parent_);
  form_.addRow(tr("Draw order"), &order_);
  form_.addRow(tr("Rest turn"), &rest_);
  form_.addRow(tr("Default drawing"), &drawing_);
  form_.addRow(tr("Warp columns"), &warp_columns_);
  form_.addRow(tr("Warp rows"), &warp_rows_);
  chain_buttons_.addWidget(&add_chain_);
  chain_buttons_.addWidget(&flip_chain_);
  chain_buttons_.addWidget(&remove_chain_);
  layout_.addWidget(&dolls_);
  layout_.addWidget(&pieces_, 2);
  layout_.addLayout(&form_);
  layout_.addWidget(&chains_title_);
  layout_.addWidget(&chains_, 1);
  layout_.addLayout(&chain_buttons_);
  layout_.addWidget(&save_rig_);
  layout_.addWidget(&hint_);
  assert(form_.rowCount() == 6);
}

void RigPanel::Wire() {
  assert(managers_.IsComplete());
  assert(form_.rowCount() == 6);
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
  connect(&order_, &QSpinBox::editingFinished, this, [this, rig] {
    emit Problem(ProblemOf(rig->SetOrder(doll_, piece_, order_.value())));
  });
  connect(&rest_, &QDoubleSpinBox::editingFinished, this, [this, rig] {
    emit Problem(
        ProblemOf(rig->SetRestRotation(doll_, piece_, rest_.value())));
  });
  connect(&drawing_, &QComboBox::activated, this, [this, rig](int index) {
    emit Problem(ProblemOf(rig->SetDefaultDrawing(doll_, piece_, index - 1)));
  });
  for (QSpinBox* side : {&warp_columns_, &warp_rows_}) {
    connect(side, &QSpinBox::editingFinished, this, [this, rig] {
      // A grid needs both sides; one at zero turns warping off.
      const bool is_off =
          warp_columns_.value() == 0 || warp_rows_.value() == 0;
      const WarpGrid grid = is_off ? WarpGrid()
                                   : WarpGrid{warp_columns_.value(),
                                              warp_rows_.value()};
      emit Problem(ProblemOf(rig->SetWarpGrid(doll_, piece_, grid)));
    });
  }
  WireChains();
}

void RigPanel::WireChains() {
  RigManager* rig = managers_.rig;
  assert(rig != nullptr);
  assert(managers_.library != nullptr);
  connect(&add_chain_, &QPushButton::clicked, this, &RigPanel::AddChain);
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
