#include <cassert>

#include "io/doll_file.h"

namespace snapper {
namespace {

// The art piece's rig: the old one kept within what the art allows, or
// a fresh one jointed at its middle.
RigPiece FitPiece(const ArtPiece& art, const RigPiece* old, int stack,
                  ReconcileReport* report) {
  assert(report != nullptr);
  assert(stack >= 0);
  const bool is_new = old == nullptr;
  if (is_new) {
    report->added.append(art.name);
    return {art.name, QString(),
            QPointF(art.size.width() / 2.0, art.size.height() / 2.0), stack,
            -1, WarpGrid()};
  }
  RigPiece piece = *old;
  const int drawings = static_cast<int>(art.drawings.size());
  const bool is_default_gone = piece.default_drawing >= drawings;
  if (is_default_gone) {
    piece.default_drawing = -1;
  }
  return piece;
}

}  // namespace

Rig Reconcile(const DollArt& art, const Rig& rig, ReconcileReport* report) {
  assert(report != nullptr);
  assert(art.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  *report = ReconcileReport();
  Rig fitted;
  int stack = 0;
  for (const ArtPiece& piece : art.pieces) {
    fitted.pieces.push_back(
        FitPiece(piece, FindRig(rig, piece.name), stack, report));
    ++stack;
  }
  for (const RigPiece& old : rig.pieces) {
    const bool is_gone = FindRig(fitted, old.name) == nullptr;
    if (is_gone) {
      report->missing.append(old.name);
    }
  }
  // A piece whose parent is gone hangs from nothing rather than vanish.
  for (RigPiece& piece : fitted.pieces) {
    const bool is_orphan = !piece.parent.isEmpty() &&
                           !CanParent(fitted, piece.name, piece.parent);
    if (is_orphan) {
      piece.parent.clear();
    }
  }
  for (const IkChain& chain : rig.chains) {
    const bool is_whole = FindRig(fitted, chain.upper) != nullptr &&
                          FindRig(fitted, chain.lower) != nullptr;
    if (is_whole) {
      fitted.chains.push_back(chain);
    } else {
      report->dropped_chains.append(chain.name);
    }
  }
  assert(fitted.pieces.size() == art.pieces.size());
  return fitted;
}

}  // namespace snapper
