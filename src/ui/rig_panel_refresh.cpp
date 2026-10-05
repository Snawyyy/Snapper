// RigPanel's showing of the rig, and adding IK chains.

#include <QSignalBlocker>

#include <cassert>

#include "edit/history_manager.h"
#include "edit/rig_manager.h"
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
  RefreshPieces();
  assert(doll_.isEmpty() || FindDoll(project, doll_) != nullptr);
}

void RigPanel::RefreshPieces() {
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

void RigPanel::RefreshPiece() {
  const Doll* doll = doll_.isEmpty()
                         ? nullptr
                         : FindDoll(managers_.history->current(), doll_);
  const RigPiece* rig =
      doll != nullptr && !piece_.isEmpty() ? FindRig(doll->rig, piece_)
                                           : nullptr;
  const ArtPiece* art = rig != nullptr ? FindArt(*doll, piece_) : nullptr;
  const QString why_not = rig != nullptr ? QString()
                                         : tr("Pick a piece first.");
  for (QWidget* field : {static_cast<QWidget*>(&parent_),
                         static_cast<QWidget*>(&order_),
                         static_cast<QWidget*>(&rest_),
                         static_cast<QWidget*>(&drawing_),
                         static_cast<QWidget*>(&warp_on_),
                         static_cast<QWidget*>(&add_chain_)}) {
    Explain(field, why_not);
  }
  const QString no_twin = managers_.rig->WhyNoCopy(doll_, piece_);
  Explain(&copy_piece_, no_twin);
  Explain(&copy_side_, no_twin);
  const bool has_piece = rig != nullptr && art != nullptr;
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
  {
    const QSignalBlocker quiet_order(order_);
    const QSignalBlocker quiet_columns(warp_columns_);
    const QSignalBlocker quiet_rows(warp_rows_);
    order_.setValue(rig->order);
    const bool has_grid = rig->warp.IsOn();
    warp_on_.setChecked(has_grid);
    // Off keeps the last sizes ready for switching back on.
    if (has_grid) {
      warp_columns_.setValue(rig->warp.columns);
      warp_rows_.setValue(rig->warp.rows);
    }
  }
  ShowNumber(&rest_, rig->rest_rotation);
  drawing_.clear();
  drawing_.addItem(tr("As in Krita"));
  for (const QString& file : art->drawings) {
    drawing_.addItem(file);
  }
  drawing_.setCurrentIndex(rig->default_drawing + 1);
  const QString no_grid =
      rig->warp.IsOn() ? QString() : tr("Tick \"Bend with a grid\" first.");
  Explain(&warp_columns_, no_grid);
  Explain(&warp_rows_, no_grid);
  Explain(&add_chain_, rig->parent.isEmpty()
                           ? tr("A chain needs the piece to hang from one.")
                           : QString());
}

void RigPanel::RefreshChains() {
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
