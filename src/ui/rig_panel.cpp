#include "ui/rig_panel.h"

#include <algorithm>
#include <cassert>

#include "edit/doll_library_manager.h"
#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "edit/skin_manager.h"
#include "ui/form_helpers.h"
#include "ui/live_edit.h"
#include "ui/panel_kit.h"
#include "ui/pick_list.h"
#include "ui/problem.h"
#include "ui/theme.h"
#include "ui/toast_manager.h"

namespace snapper {
namespace {

// The piece tree shows at least and at most this many rows.
constexpr int kMinTreeRows = 8;
constexpr int kMaxTreeRows = 28;

}  // namespace

RigPanel::RigPanel(const Managers& managers)
    : managers_(managers),
      layout_(this),
      live_(managers.history),
      convert_(tr("Convert to shape mesh")),
      add_chain_(tr("Add for piece")),
      flip_chain_(tr("Flip bend")),
      remove_chain_(tr("Remove")),
      collider_box_(managers),
      skins_(managers),
      look_(managers),
      copy_piece_(tr("Copy to other side")),
      copy_side_(tr("Copy whole side")),
      mirror_nodes_(tr("Mirror rig to other side")),
      save_rig_(tr("Save rig to library")),
      hint_(tr("Drag a yellow joint to move it. Double-click a joint, then "
               "click the part it hangs from. Drag a square to move an IK "
               "tip.")) {
  assert(managers_.IsComplete());
  BuildLayout();
  connect(&collider_box_, &ColliderBox::Problem, this, &RigPanel::Problem);
  connect(&skins_, &SkinRigBox::Problem, this, &RigPanel::Problem);
  connect(&look_, &LookBox::Problem, this, &RigPanel::Problem);
  connect(&skins_, &SkinRigBox::SkinPicked, this, [this](const QString& s) {
    emit SkinPicked(s);
    RefreshPiece();
  });
  Wire();
  Refresh();
  assert(layout_.count() > 0);
}

std::vector<QString> RigPanel::Picked() const {
  assert(managers_.IsComplete());
  assert(picked_.size() <= kMaxDollPieces);
  std::vector<QString> picked(picked_.begin(), picked_.end());
  const bool has_none = picked.empty() && !piece_.isEmpty();
  if (has_none) {
    picked.push_back(piece_);
  }
  return picked;
}

const RigPiece* RigPanel::Focus() const {
  assert(managers_.IsComplete());
  const Doll* doll = doll_.isEmpty()
                         ? nullptr
                         : FindDoll(managers_.history->current(), doll_);
  assert(managers_.history != nullptr);
  return doll != nullptr && !piece_.isEmpty() ? FindRig(doll->rig, piece_)
                                              : nullptr;
}

void RigPanel::SetUpWarpFields() {
  assert(layout_.count() == 0);
  assert(kDefaultWarpCells <= kMaxWarpCells);
  for (QSpinBox* side : {&warp_columns_, &warp_rows_}) {
    side->setRange(1, kMaxWarpCells);
    side->setValue(kDefaultWarpCells);
  }
  // Named so tests and screen readers find them by what they are.
  warp_on_.setObjectName("warp_on");
  warp_on_.setText(tr("Bend"));
  warp_on_.setToolTip(tr("Lets you push spots of the drawing, like a "
                         "crease in a shirt: dots hug the drawing's "
                         "outline and fill it evenly. Pick the piece on "
                         "the stage and drag its dots."));
  detail_.setObjectName("mesh_detail");
  detail_.addItems({tr("Few"), tr("Normal"), tr("Many")});
  detail_.setCurrentIndex(static_cast<int>(MeshDetail::kNormal));
  detail_.setToolTip(tr("How many dots the mesh has. Changing it makes "
                        "the mesh again; dots keep their settings where "
                        "another dot is near."));
  convert_.setObjectName("convert_mesh");
  convert_.setToolTip(tr("Swaps this old rectangular grid for a shape "
                         "mesh that hugs the drawing, keeping its dots' "
                         "settings and keys where it can."));
}

void RigPanel::BuildLayout() {
  assert(layout_.count() == 0);
  layout_.setContentsMargins(4, 4, 4, 4);
  hint_.setWordWrap(true);
  order_.setRange(-1000, 1000);
  order_.setToolTip(tr("Higher draws in front."));
  SetUpNumber(&rest_, Number::kDegrees);
  rest_.setToolTip(tr("How the piece is turned when not posed."));
  SetUpWarpFields();
  keep_shape_.setObjectName("keep_shape");
  keep_shape_.setText(tr("Keep shape when leaning"));
  keep_shape_.setToolTip(tr("Leaning or swivelling the doll still moves "
                            "and sizes this piece, but never squashes it. "
                            "Good for heads."));
  layout_.setSpacing(theme::kRowGap);
  // Doll: its pieces, and saving its rig. Which doll is open is picked
  // in the library (OpenDoll).
  // The tree draws its own rows; its height follows the doll
  // (FitPieceList), and the panel scrolls past it.
  pieces_.setItemDelegate(&tree_rows_);
  pieces_.viewport()->setAttribute(Qt::WA_Hover);
  pieces_.setFixedHeight(kMinTreeRows * RigTreeDelegate::RowHeight());
  auto* doll_body = MakeOwned<QWidget>(this);
  auto* doll_column = MakeOwned<QVBoxLayout>(doll_body);
  doll_column->setContentsMargins(0, 0, 0, 0);
  doll_column->addWidget(&pieces_);
  doll_column->addWidget(&save_rig_);
  doll_section_ = PlatedSection(tr("Doll"), doll_body, this);
  // Joint: how the focused piece hangs and shows, and its mirroring.
  auto* joint_body = MakeOwned<QWidget>(this);
  auto* joint_column = MakeOwned<QVBoxLayout>(joint_body);
  joint_column->setContentsMargins(0, 0, 0, 0);
  auto* joint_host = MakeOwned<QWidget>(joint_body);
  joint_form_ = SettingsForm(joint_host);
  joint_form_->addRow(tr("Hangs from"), &parent_);
  AddAngleRow(joint_form_, tr("Rest turn"), &rest_, &live_);
  AddNumberRow(joint_form_, tr("Draw order"), &order_, &live_);
  joint_form_->addRow(tr("Drawing"), &drawing_);
  joint_form_->addRow(QString(), &keep_shape_);
  stays_crisp_.setObjectName("stays_crisp");
  stays_crisp_.setText(tr("Keep crisp"));
  stays_crisp_.setToolTip(tr("Draws this piece Crisp whatever the doll's "
                             "look: for a pixel-art screen and the like."));
  joint_form_->addRow(QString(), &stays_crisp_);
  joint_column->addWidget(joint_host);
  mirror_buttons_.addWidget(&copy_piece_);
  mirror_buttons_.addWidget(&copy_side_);
  mirror_buttons_.addWidget(&mirror_nodes_);
  mirror_nodes_.setToolTip(tr("Copies this piece's drag and physics dots and "
                              "its collider to its other-side twin, "
                              "mirrored."));
  joint_column->addLayout(&mirror_buttons_);
  joint_section_ = PlatedSection(tr("Joint"), joint_body, this);
  // Warp: bending the focused piece's drawing.
  auto* warp_host = MakeOwned<QWidget>(this);
  warp_form_ = SettingsForm(warp_host);
  warp_form_->addRow(QString(), &warp_on_);
  warp_form_->addRow(tr("Detail"), &detail_);
  AddNumberPair(warp_form_, tr("Cells"), &warp_row_,
                {tr("across"), &warp_columns_}, {tr("down"), &warp_rows_},
                &live_);
  warp_form_->addRow(QString(), &convert_);
  warp_section_ = PlatedSection(tr("Warp"), warp_host, this);
  // IK chains of the doll.
  chain_buttons_.addWidget(&add_chain_);
  chain_buttons_.addWidget(&flip_chain_);
  chain_buttons_.addWidget(&remove_chain_);
  // Chains are patch cords between two pieces; the list is as tall as
  // its cords, and an empty patch bay says how to plug one in.
  chains_.setItemDelegate(&cords_);
  chains_.setFixedHeight(PatchCordDelegate::RowHeight());
  auto* chains_body = MakeOwned<QWidget>(this);
  auto* chains_column = MakeOwned<QVBoxLayout>(chains_body);
  chains_column->setContentsMargins(0, 0, 0, 0);
  no_chains_ = EmptyHint(tr("No chains plugged in yet. Pick a hand or a "
                            "foot, then Add for piece: its arm or leg "
                            "bends to follow it."),
                         chains_body);
  chains_column->addWidget(no_chains_);
  chains_column->addWidget(&chains_);
  chains_column->addLayout(&chain_buttons_);
  chains_section_ = PlatedSection(tr("IK chains"), chains_body, this);
  // The tree; the skin being changed, as it decides what the Drawing
  // picker below sets; what is set on the picked piece; then the
  // doll-wide settings set once and left, IK chains, used least, last.
  layout_.addWidget(&hint_);
  layout_.addWidget(doll_section_);
  layout_.addWidget(&skins_);
  layout_.addWidget(joint_section_);
  layout_.addWidget(warp_section_);
  layout_.addWidget(&collider_box_);
  layout_.addWidget(&look_);
  layout_.addWidget(chains_section_);
  layout_.addStretch(1);
  // The piece list picks by the one pick rule; pieces are not deleted
  // from here, so Delete asks nothing of it.
  PickList::Install(&pieces_);
  assert(joint_form_->rowCount() == 6);
  assert(warp_form_->rowCount() == 4);
}

void RigPanel::FitPieceList() {
  assert(kMinTreeRows > 0 && kMaxTreeRows >= kMinTreeRows);
  assert(pieces_.count() >= 0);
  // Tall enough for the whole doll, so the tree rarely scrolls inside a
  // panel that scrolls too.
  const int rows = std::clamp(pieces_.count(), kMinTreeRows, kMaxTreeRows);
  pieces_.setFixedHeight(rows * RigTreeDelegate::RowHeight() +
                         2 * pieces_.frameWidth());
}

void RigPanel::OpenDoll(const QString& doll) {
  assert(managers_.IsComplete());
  assert(!doll.isEmpty());
  const bool is_known = FindDoll(managers_.history->current(), doll) != nullptr;
  const bool is_new = is_known && doll != doll_;
  if (!is_new) {
    return;
  }
  doll_ = doll;
  piece_.clear();
  picked_.clear();
  emit DollPicked(doll_);
  Refresh();
}

void RigPanel::Wire() {
  assert(managers_.IsComplete());
  assert(joint_form_ != nullptr && warp_form_ != nullptr);
  RigManager* rig = managers_.rig;
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
    const QString skin = skins_.skin();
    if (skin.isEmpty()) {
      emit Problem(
          ProblemOf(rig->SetDefaultDrawing(doll_, piece_, index - 1)));
      return;
    }
    // The first two entries are "Not in the skin" and "None (hidden)".
    const int drawing = index == 0   ? kSkinUnset
                        : index == 1 ? kSkinHidden
                                     : index - 2;
    emit Problem(ProblemOf(
        managers_.skins->SetDrawing(doll_, skin, Picked(), drawing)));
  });
  WireWarp();
  connect(&keep_shape_, &QCheckBox::clicked, this, [this, rig](bool is_on) {
    emit Problem(ProblemOf(rig->SetKeepShapeAll(doll_, Picked(), is_on)));
  });
  connect(&stays_crisp_, &QCheckBox::clicked, this, [this, rig](bool is_on) {
    emit Problem(ProblemOf(rig->SetStaysCrispAll(doll_, Picked(), is_on)));
  });
  WireChains();
}

void RigPanel::WireWarp() {
  RigManager* rig = managers_.rig;
  assert(rig != nullptr);
  assert(managers_.toasts != nullptr);
  connect(&warp_on_, &QCheckBox::clicked, this, [this, rig](bool is_on) {
    if (is_on) {
      MakeShapeMesh();
      return;
    }
    emit Problem(ProblemOf(rig->SetWarpAll(doll_, Picked(), WarpGrid())));
  });
  connect(&detail_, &QComboBox::activated, this, &RigPanel::MakeShapeMesh);
  connect(&convert_, &QPushButton::clicked, this, &RigPanel::MakeShapeMesh);
  // Only an old grid's cells are set here; a new warp is a shape mesh.
  const auto set_grid = [this, rig] {
    emit Problem(ProblemOf(rig->SetWarpAll(
        doll_, Picked(), WarpGrid{warp_columns_.value(), warp_rows_.value()})));
  };
  for (QSpinBox* side : {&warp_columns_, &warp_rows_}) {
    MakeLive(side, &live_, tr("Warp grid"), this, set_grid);
  }
}

void RigPanel::MakeShapeMesh() {
  assert(managers_.toasts != nullptr);
  assert(detail_.currentIndex() >= 0);
  const Result<CarryCount> made = managers_.rig->SetShapeMeshAll(
      doll_, Picked(), static_cast<MeshDetail>(detail_.currentIndex()));
  managers_.toasts->Finish(
      ProblemOf(made),
      made.has_value() ? RigManager::MeshDoneText(*made) : QString());
  Refresh();
}

void RigPanel::WireChains() {
  RigManager* rig = managers_.rig;
  assert(rig != nullptr);
  assert(managers_.library != nullptr);
  connect(&add_chain_, &QPushButton::clicked, this, &RigPanel::AddChain);
  connect(&copy_piece_, &QPushButton::clicked, this, [this, rig] {
    emit Problem(ProblemOf(rig->CopyAllToOtherSide(doll_, Picked())));
  });
  connect(&mirror_nodes_, &QPushButton::clicked, this, [this, rig] {
    emit Problem(ProblemOf(rig->MirrorNodes(doll_, piece_)));
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
    const QString problem = ProblemOf(managers_.library->SaveRig(doll_));
    emit Problem(problem);
    const bool is_saved = problem.isEmpty();
    if (is_saved) {
      managers_.toasts->Cheer(tr("Saved the rig of %1 to the library.")
                                  .arg(doll_));
    }
  });
  connect(&chains_, &QListWidget::currentRowChanged, this,
          &RigPanel::RefreshChains);
  connect(managers_.history, &HistoryManager::Changed, this,
          &RigPanel::Refresh);
}

}  // namespace snapper
