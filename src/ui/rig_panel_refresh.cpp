// RigPanel's showing of the rig, and adding IK chains.

#include <QSignalBlocker>

#include <cassert>
#include <vector>

#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "edit/skin_manager.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"
#include "ui/rig_panel.h"

namespace snapper {
namespace {

// How deep a piece hangs, for indenting the tree.
int DepthOf(const Rig& rig, const QString& piece) {
  assert(!piece.isEmpty());
  assert(rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  int depth = 0;
  const RigPiece* at = FindRig(rig, piece);
  for (int i = 0; i < kMaxDollPieces && at != nullptr; ++i) {
    const bool is_root = at->parent.isEmpty();
    if (is_root) {
      break;
    }
    at = FindRig(rig, at->parent);
    ++depth;
  }
  return depth;
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
    item->setSelected(picked.contains(name));
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
  {
    const QSignalBlocker quiet(dolls_);
    dolls_.clear();
    for (const auto& [name, doll] : project.dolls) {
      dolls_.addItem(name);
    }
    dolls_.setCurrentText(doll_);
  }
  Explain(&save_rig_, doll_.isEmpty() ? tr("Import a doll first.")
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
      for (const RigPiece& piece : doll->rig.pieces) {
        pieces_.addItem(QString(DepthOf(doll->rig, piece.name) * 2, ' ') +
                        piece.name);
      }
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
  assert(form_.rowCount() == 12);
  assert(managers_.IsComplete());
  const Doll* doll = doll_.isEmpty()
                         ? nullptr
                         : FindDoll(managers_.history->current(), doll_);
  const RigPiece* rig =
      doll != nullptr && !piece_.isEmpty() ? FindRig(doll->rig, piece_)
                                           : nullptr;
  const ArtPiece* art = rig != nullptr ? FindArt(*doll, piece_) : nullptr;
  collider_box_.Show(doll_, piece_);
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
  assert(form_.rowCount() == 12);
  const bool has_piece = rig != nullptr;
  const bool has_warp = has_piece && rig->warp.IsOn();
  const bool is_grid = has_warp && !rig->warp.IsMesh();
  // Rows of form_: the joint (0 to 6), then the warp (7 to 11).
  for (int row = 0; row <= 8; ++row) {
    form_.setRowVisible(row, has_piece);
  }
  // Detail makes and remakes a shape mesh; cells and Convert are for an
  // old grid only.
  form_.setRowVisible(9, has_piece && !is_grid);
  form_.setRowVisible(10, is_grid);
  form_.setRowVisible(11, is_grid);
  collider_box_.setVisible(has_piece);
  for (QWidget* button : {static_cast<QWidget*>(&copy_piece_),
                          static_cast<QWidget*>(&copy_side_),
                          static_cast<QWidget*>(&mirror_nodes_)}) {
    button->setVisible(has_piece);
  }
  for (QWidget* part : {static_cast<QWidget*>(&look_),
                        static_cast<QWidget*>(&skins_),
                        static_cast<QWidget*>(&pieces_),
                        static_cast<QWidget*>(&chains_title_),
                        static_cast<QWidget*>(&chains_),
                        static_cast<QWidget*>(&add_chain_),
                        static_cast<QWidget*>(&flip_chain_),
                        static_cast<QWidget*>(&remove_chain_),
                        static_cast<QWidget*>(&save_rig_)}) {
    part->setVisible(has_doll);
  }
}

void RigPanel::ShowWarp(const RigPiece* rig) {
  assert(managers_.rig != nullptr);
  assert(form_.rowCount() == 12);
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
        chains_.addItem(chain.name);
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
