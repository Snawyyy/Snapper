// RigPanel's showing of the rig, and adding IK chains.

#include <QSignalBlocker>

#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>

#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "edit/skin_manager.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"
#include "ui/rig_panel.h"
#include "ui/patch_cord_delegate.h"
#include "ui/rig_tree_delegate.h"

namespace snapper {
namespace {

// The chain list grows to this many cords before it scrolls.
constexpr int kMaxCordRows = 6;

// One row of the piece tree: a piece, how deep it hangs, which branch
// lines above it run on past it, and whether it is its parent's last.
struct TreeRow final {
  QString name;
  int depth = 0;
  qulonglong lines = 0;
  bool is_last = false;
};

// The rig's pieces in tree order: each piece right under the one it
// hangs from, its children after it, roots in the rig's order.
std::vector<TreeRow> TreeOrder(const Rig& rig) {
  assert(rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  std::vector<TreeRow> rows;
  std::vector<TreeRow> stack;
  const auto children_of = [&rig](const QString& parent) {
    std::vector<QString> kids;
    for (const RigPiece& piece : rig.pieces) {
      const bool is_child = piece.parent == parent;
      const bool is_orphan = parent.isEmpty() && !piece.parent.isEmpty() &&
                             FindRig(rig, piece.parent) == nullptr;
      if (is_child || is_orphan) {
        kids.push_back(piece.name);
      }
    }
    return kids;
  };
  const std::vector<QString> roots = children_of(QString());
  for (size_t i = roots.size(); i > 0; --i) {
    stack.push_back(TreeRow{roots[i - 1], 0, 0, i == roots.size()});
  }
  for (int guard = 0; guard < kMaxDollPieces && !stack.empty(); ++guard) {
    const TreeRow row = stack.back();
    stack.pop_back();
    rows.push_back(row);
    const std::vector<QString> kids = children_of(row.name);
    // The line of this row's own elbow runs on while it has a later
    // sibling: its children carry that bit.
    const bool carries = row.depth > 0 && !row.is_last &&
                         row.depth - 1 < kRigMaxDepth;
    const qulonglong lines =
        row.lines | (carries ? (1ULL << (row.depth - 1)) : 0ULL);
    for (size_t k = kids.size(); k > 0; --k) {
      stack.push_back(
          TreeRow{kids[k - 1], row.depth + 1, lines, k == kids.size()});
    }
  }
  assert(rows.size() <= rig.pieces.size());
  return rows;
}

}  // namespace

void RigPanel::PickPiece(const QString& piece) {
  assert(managers_.IsComplete());
  assert(piece.size() < 100000);
  SetPick(piece.isEmpty() ? QStringList() : QStringList{piece}, piece);
}

void RigPanel::SetPick(const QStringList& picked, const QString& focus) {
  assert(picked.size() <= kMaxDollPieces);
  assert(focus.size() < 100000);
  picked_ = picked;
  piece_ = focus;
  const QSignalBlocker quiet(pieces_);
  pieces_.clearSelection();
  for (int i = 0; i < pieces_.count(); ++i) {
    QListWidgetItem* item = pieces_.item(i);
    const QString name = item->text().trimmed();
    // Through the selection model: under PickList the list is
    // NoSelection, where an item's own setSelected does nothing.
    if (picked.contains(name)) {
      pieces_.selectionModel()->select(pieces_.model()->index(i, 0),
                                       QItemSelectionModel::Select);
    }
    const bool is_focus = name == focus;
    if (is_focus) {
      pieces_.setCurrentItem(item, QItemSelectionModel::NoUpdate);
    }
  }
  RefreshPiece();
}

void RigPanel::Refresh() {
  assert(managers_.IsComplete());
  const Project& project = managers_.history->current();
  const bool is_gone = !doll_.isEmpty() && FindDoll(project, doll_) == nullptr;
  if (is_gone) {
    doll_.clear();
    piece_.clear();
    picked_.clear();
  }
  const bool has_none = doll_.isEmpty() && !project.dolls.empty();
  if (has_none) {
    doll_ = project.dolls.begin()->first;
    emit DollPicked(doll_);
  }
  Explain(&save_rig_, doll_.isEmpty() ? tr("Click a doll in the library.")
                                      : QString());
  skins_.SetDoll(doll_);
  look_.SetDoll(doll_);
  RefreshPieces();
  assert(doll_.isEmpty() || FindDoll(project, doll_) != nullptr);
}

void RigPanel::RefreshPieces() {
  assert(managers_.IsComplete());
  const Doll* doll = doll_.isEmpty()
                         ? nullptr
                         : FindDoll(managers_.history->current(), doll_);
  assert(managers_.history != nullptr);
  {
    const QSignalBlocker quiet(pieces_);
    pieces_.clear();
    const bool has_doll = doll != nullptr;
    if (has_doll) {
      for (const TreeRow& row : TreeOrder(doll->rig)) {
        auto item = std::make_unique<QListWidgetItem>(row.name);
        item->setData(kRigDepthRole, row.depth);
        item->setData(kRigLinesRole, row.lines);
        item->setData(kRigLastRole, row.is_last);
        // The list owns it.
        pieces_.addItem(item.release());
      }
      FitPieceList();
    }
  }
  SetPick(picked_, piece_);
  RefreshChains();
}

void RigPanel::ShowOrderAndRest(const Doll& doll, const RigPiece& focus) {
  assert(!piece_.isEmpty());
  assert(managers_.history != nullptr);
  std::vector<double> orders;
  std::vector<double> rests;
  for (const QString& name : Picked()) {
    const RigPiece* piece = FindRig(doll.rig, name);
    const bool is_found = piece != nullptr;
    if (is_found) {
      orders.push_back(piece->order);
      rests.push_back(piece->rest_rotation);
    }
  }
  ShowNumber(&order_, focus.order, orders);
  ShowNumber(&rest_, focus.rest_rotation, rests);
}

void RigPanel::RefreshPiece() {
  assert(joint_form_ != nullptr);
  assert(managers_.IsComplete());
  const Doll* doll = doll_.isEmpty()
                         ? nullptr
                         : FindDoll(managers_.history->current(), doll_);
  const RigPiece* rig =
      doll != nullptr && !piece_.isEmpty() ? FindRig(doll->rig, piece_)
                                           : nullptr;
  const ArtPiece* art = rig != nullptr ? FindArt(*doll, piece_) : nullptr;
  collider_box_.Show(doll_, piece_, skins_.skin());
  ShowWarp(rig);
  const QString why_not = rig != nullptr ? QString()
                                         : tr("Pick a piece first.");
  for (QWidget* field : {static_cast<QWidget*>(&parent_),
                         static_cast<QWidget*>(&order_),
                         static_cast<QWidget*>(&rest_),
                         static_cast<QWidget*>(&drawing_),
                         static_cast<QWidget*>(&warp_on_),
                         static_cast<QWidget*>(&keep_shape_),
                         static_cast<QWidget*>(&stays_crisp_),
                         static_cast<QWidget*>(&add_chain_)}) {
    Explain(field, why_not);
  }
  const QString no_twin = managers_.rig->WhyNoCopy(doll_, piece_);
  Explain(&copy_piece_, no_twin);
  Explain(&copy_side_, no_twin);
  Explain(&mirror_nodes_, managers_.rig->WhyNoMirror(doll_, piece_));
  const bool has_piece = rig != nullptr && art != nullptr;
  ShowOnlyRelevant(doll != nullptr, has_piece ? rig : nullptr);
  if (!has_piece) {
    return;
  }
  const QSignalBlocker quiet_parent(parent_);
  const QSignalBlocker quiet_drawing(drawing_);
  parent_.clear();
  parent_.addItem(tr("(nothing: a root piece)"));
  for (const RigPiece& other : doll->rig.pieces) {
    const bool can_hang = CanParent(doll->rig, piece_, other.name);
    if (can_hang) {
      parent_.addItem(other.name);
    }
  }
  parent_.setCurrentIndex(rig->parent.isEmpty()
                              ? 0
                              : std::max(0, parent_.findText(rig->parent)));
  keep_shape_.setChecked(rig->keeps_shape);
  stays_crisp_.setChecked(rig->stays_crisp);
  ShowOrderAndRest(*doll, *rig);
  drawing_.clear();
  const QString skin = skins_.skin();
  const bool is_skin = !skin.isEmpty();
  if (is_skin) {
    // Changing a skin: what the piece shows in it.
    drawing_.addItem(tr("Not in %1").arg(skin));
    drawing_.addItem(tr("None (hidden)"));
  } else {
    drawing_.addItem(tr("As in Krita"));
  }
  for (const QString& file : art->drawings) {
    drawing_.addItem(file);
  }
  const int in_skin =
      is_skin ? managers_.skins->DrawingIn(doll_, skin, piece_) : 0;
  drawing_.setCurrentIndex(!is_skin                 ? rig->default_drawing + 1
                           : in_skin == kSkinUnset  ? 0
                           : in_skin == kSkinHidden ? 1
                                                    : in_skin + 2);
  drawing_.setToolTip(is_skin ? tr("What the picked pieces show while %1 is "
                                   "worn.")
                                    .arg(skin)
                              : QString());
  Explain(&add_chain_, rig->parent.isEmpty()
                           ? tr("A chain needs the piece to hang from one.")
                           : QString());
}

void RigPanel::ShowOnlyRelevant(bool has_doll, const RigPiece* rig) {
  assert(joint_form_ != nullptr && warp_form_ != nullptr);
  assert(warp_form_->rowCount() == 4);
  const bool has_piece = rig != nullptr;
  const bool has_warp = has_piece && rig->warp.IsOn();
  const bool is_grid = has_warp && !rig->warp.IsMesh();
  joint_section_->setVisible(has_piece);
  warp_section_->setVisible(has_piece);
  // Rows of the warp: Bend, Detail, Cells, Convert. Detail makes and
  // remakes a shape mesh; cells and Convert are for an old grid only.
  warp_form_->setRowVisible(1, has_piece && !is_grid);
  warp_form_->setRowVisible(2, is_grid);
  warp_form_->setRowVisible(3, is_grid);
  // The Collider box shows itself while the piece is one (Show).
  for (QWidget* part : {static_cast<QWidget*>(&look_),
                        static_cast<QWidget*>(&skins_),
                        static_cast<QWidget*>(&pieces_),
                        static_cast<QWidget*>(&save_rig_),
                        chains_section_}) {
    part->setVisible(has_doll);
  }
}

void RigPanel::ShowWarp(const RigPiece* rig) {
  assert(managers_.rig != nullptr);
  assert(warp_form_ != nullptr);
  const QSignalBlocker quiet_columns(warp_columns_);
  const QSignalBlocker quiet_rows(warp_rows_);
  const QSignalBlocker quiet_detail(detail_);
  const bool has_warp = rig != nullptr && rig->warp.IsOn();
  warp_on_.setChecked(has_warp);
  const bool is_grid = has_warp && !rig->warp.IsMesh();
  // Off keeps the last sizes and detail ready for switching back on.
  if (is_grid) {
    warp_columns_.setValue(rig->warp.columns);
    warp_rows_.setValue(rig->warp.rows);
  }
  const bool is_mesh = has_warp && rig->warp.IsMesh();
  if (is_mesh) {
    detail_.setCurrentIndex(static_cast<int>(rig->warp.mesh->detail));
  }
  const QString no_cells =
      rig == nullptr ? tr("Pick a piece first.")
      : is_grid      ? QString()
      : is_mesh ? tr("A shape mesh places its dots itself; set its Detail "
                     "instead.")
                : tr("Tick \"Bend\" first.");
  Explain(&warp_columns_, no_cells);
  Explain(&warp_rows_, no_cells);
  Explain(&detail_, managers_.rig->WhyNoDetail(doll_, piece_));
  Explain(&convert_, managers_.rig->WhyNoConvert(doll_, piece_));
}

void RigPanel::RefreshChains() {
  assert(managers_.IsComplete());
  const Doll* doll = doll_.isEmpty()
                         ? nullptr
                         : FindDoll(managers_.history->current(), doll_);
  const QString picked =
      chains_.currentItem() != nullptr ? chains_.currentItem()->text()
                                       : QString();
  {
    const QSignalBlocker quiet(chains_);
    chains_.clear();
    const bool has_doll = doll != nullptr;
    if (has_doll) {
      for (const IkChain& chain : doll->rig.chains) {
        auto item = std::make_unique<QListWidgetItem>(chain.name);
        item->setData(kChainUpperRole, chain.upper);
        item->setData(kChainLowerRole, chain.lower);
        item->setData(kChainClockwiseRole, chain.bends_clockwise);
        item->setToolTip(chain.name);
        // The list owns it.
        chains_.addItem(item.release());
        const bool is_picked = chain.name == picked;
        if (is_picked) {
          chains_.setCurrentRow(chains_.count() - 1);
        }
      }
    }
  }
  const QString no_chain = chains_.currentItem() != nullptr
                               ? QString()
                               : tr("Pick an IK chain first.");
  Explain(&flip_chain_, no_chain);
  Explain(&remove_chain_, no_chain);
  const bool has_chains = chains_.count() > 0;
  chains_.setVisible(has_chains);
  no_chains_->setVisible(!has_chains);
  chains_.setFixedHeight(std::clamp(chains_.count(), 1, kMaxCordRows) *
                             PatchCordDelegate::RowHeight() +
                         2 * chains_.frameWidth());
  assert(chains_.count() >= 0);
}

void RigPanel::AddChain() {
  assert(managers_.IsComplete());
  const Doll* doll = FindDoll(managers_.history->current(), doll_);
  const RigPiece* rig =
      doll != nullptr ? FindRig(doll->rig, piece_) : nullptr;
  const ArtPiece* art = doll != nullptr ? FindArt(*doll, piece_) : nullptr;
  const bool is_ready = rig != nullptr && art != nullptr;
  if (!is_ready) {
    return;
  }
  // The tip starts at the far end of the piece from its joint, which is
  // where a hand or foot usually is.
  const QPointF tip(rig->pivot.x() < art->size.width() / 2.0
                        ? art->size.width()
                        : 0.0,
                    art->size.height() / 2.0);
  const IkChain chain{tr("IK %1").arg(piece_), rig->parent, piece_, tip,
                      true};
  emit Problem(ProblemOf(managers_.rig->AddChain(doll_, chain)));
  assert(managers_.rig != nullptr);
}

}  // namespace snapper
