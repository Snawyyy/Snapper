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

std::vector<QString> RigPanel::Picked() const {
  assert(picked_.size() <= kMaxDollPieces);
  std::vector<QString> picked(picked_.begin(), picked_.end());
  const bool has_none = picked.empty() && !piece_.isEmpty();
  if (has_none) {
    picked.push_back(piece_);
  }
  return picked;
}

const RigPiece* RigPanel::Focus() const {
  const Doll* doll = doll_.isEmpty()
                         ? nullptr
                         : FindDoll(managers_.history->current(), doll_);
  assert(managers_.history != nullptr);
  return doll != nullptr && !piece_.isEmpty() ? FindRig(doll->rig, piece_)
                                              : nullptr;
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
  // Named so tests and screen readers find them by what they are.
  warp_on_.setObjectName("warp_on");
  keep_shape_.setObjectName("keep_shape");
  warp_on_.setText(tr("Bend with a grid"));
  warp_on_.setToolTip(tr("Lets you push spots of the drawing, like a "
                         "crease in a shirt: pick the piece on the stage "
                         "and drag its grid's dots."));
  keep_shape_.setText(tr("Keep shape when leaning"));
  keep_shape_.setToolTip(tr("Leaning or swivelling the doll still moves "
                            "and sizes this piece, but never squashes it. "
                            "Good for heads."));
  AddTitle(&form_, &joint_title_, tr("Joint"));
  form_.addRow(tr("Hangs from"), &parent_);
  form_.addRow(tr("Rest turn"), &rest_);
  form_.addRow(tr("Draw order"), &order_);
  form_.addRow(tr("Drawing"), &drawing_);
  form_.addRow(QString(), &keep_shape_);
  AddTitle(&form_, &warp_title_, tr("Warp"));
  form_.addRow(QString(), &warp_on_);
  AddPair(&form_, tr("Cells"), &warp_row_, &warp_columns_, &warp_rows_);
  BuildMotion();
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
  assert(form_.rowCount() == 12);
}

void RigPanel::Wire() {
  assert(managers_.IsComplete());
  assert(form_.rowCount() == 12);
  RigManager* rig = managers_.rig;
  connect(&dolls_, &QComboBox::textActivated, this, [this](const QString& d) {
    doll_ = d;
    piece_.clear();
    picked_.clear();
    emit DollPicked(doll_);
    Refresh();
  });
  // Shift picks a run of pieces, Ctrl adds or drops one.
  pieces_.setSelectionMode(QAbstractItemView::ExtendedSelection);
  connect(&pieces_, &QListWidget::itemSelectionChanged, this, [this] {
    QStringList picked;
    for (const QListWidgetItem* item : pieces_.selectedItems()) {
      picked.append(item->text().trimmed());
    }
    const QListWidgetItem* current = pieces_.currentItem();
    const QString focus = current != nullptr && current->isSelected()
                              ? current->text().trimmed()
                              : (picked.isEmpty() ? QString() : picked[0]);
    picked_ = picked;
    piece_ = focus;
    emit PickChanged(picked_, piece_);
    RefreshPiece();
  });
  connect(&parent_, &QComboBox::activated, this, [this, rig](int index) {
    const QString parent = index == 0 ? QString() : parent_.itemText(index);
    emit Problem(ProblemOf(rig->SetParentAll(doll_, Picked(), parent)));
  });
  MakeLive(&order_, &live_, tr("Restack"), this, [this, rig] {
    const RigPiece* focus = Focus();
    const bool has_focus = focus != nullptr;
    if (has_focus) {
      emit Problem(ProblemOf(rig->ShiftOrderAll(
          doll_, Picked(), order_.value() - focus->order)));
    }
  });
  MakeLive(&rest_, &live_, tr("Rest turn"), this, [this, rig] {
    const RigPiece* focus = Focus();
    const bool has_focus = focus != nullptr;
    if (has_focus) {
      emit Problem(ProblemOf(rig->ShiftRestAll(
          doll_, Picked(), rest_.value() - focus->rest_rotation)));
    }
  });
  connect(&drawing_, &QComboBox::activated, this, [this, rig](int index) {
    emit Problem(ProblemOf(rig->SetDefaultDrawing(doll_, piece_, index - 1)));
  });
  const auto set_grid = [this, rig] {
    const bool is_on = warp_on_.isChecked();
    const WarpGrid grid = is_on ? WarpGrid{warp_columns_.value(),
                                           warp_rows_.value()}
                                : WarpGrid();
    emit Problem(ProblemOf(rig->SetWarpAll(doll_, Picked(), grid)));
  };
  connect(&warp_on_, &QCheckBox::clicked, this, set_grid);
  connect(&keep_shape_, &QCheckBox::clicked, this, [this, rig](bool is_on) {
    emit Problem(ProblemOf(rig->SetKeepShapeAll(doll_, Picked(), is_on)));
  });
  for (QSpinBox* side : {&warp_columns_, &warp_rows_}) {
    MakeLive(side, &live_, tr("Warp grid"), this, set_grid);
  }
  WireMotion();
  WireChains();
}

void RigPanel::WireChains() {
  RigManager* rig = managers_.rig;
  assert(rig != nullptr);
  assert(managers_.library != nullptr);
  connect(&add_chain_, &QPushButton::clicked, this, &RigPanel::AddChain);
  connect(&copy_piece_, &QPushButton::clicked, this, [this, rig] {
    emit Problem(ProblemOf(rig->CopyAllToOtherSide(doll_, Picked())));
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
