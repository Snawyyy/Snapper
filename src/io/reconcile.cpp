#include <algorithm>
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
  // Drawings made in Snapper come after Krita's.
  const int drawings = static_cast<int>(art.drawings.size() +
                                        piece.own_drawings.size());
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
  fitted.krita_file = rig.krita_file;
  fitted.added_colors = rig.added_colors;
  // Groups, dials and skins name pieces and drawings; ones that went
  // missing are skipped where they are used (the piece shows its
  // default), never deleted here.
  fitted.dot_groups = rig.dot_groups;
  fitted.dials = rig.dials;
  fitted.skins = rig.skins;
  fitted.look = rig.look;
  fitted.hard_edges = rig.hard_edges;
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

void AddOwnDrawings(Doll* doll) {
  assert(doll != nullptr);
  assert(doll->art.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  for (ArtPiece& art : doll->art.pieces) {
    const RigPiece* rig = FindRig(doll->rig, art.name);
    const bool has_own = rig != nullptr && !rig->own_drawings.empty();
    if (!has_own) {
      continue;
    }
    for (const OwnDrawing& own : rig->own_drawings) {
      const bool is_listed =
          std::find(art.drawings.begin(), art.drawings.end(), own.file) !=
          art.drawings.end();
      const bool is_full =
          art.drawings.size() >= static_cast<size_t>(kMaxPieceDrawings);
      if (is_listed || is_full) {
        continue;
      }
      // Krita's drawings keep their names (or none, for old exports).
      art.layer_names.resize(art.drawings.size());
      art.drawings.push_back(own.file);
      art.layer_names.push_back(own.name);
    }
    assert(art.layer_names.size() == art.drawings.size());
  }
}

QStringList RemovedDrawings(const DollArt& before, const DollArt& after) {
  assert(before.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(after.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  QStringList removed;
  for (const ArtPiece& old : before.pieces) {
    const auto now = std::find_if(
        after.pieces.begin(), after.pieces.end(),
        [&old](const ArtPiece& piece) { return piece.name == old.name; });
    if (now == after.pieces.end()) {
      continue;
    }
    // An old export had no names, so only the count can be compared.
    const bool is_by_count = old.layer_names.empty();
    const int now_count = static_cast<int>(now->drawings.size());
    QStringList kept;
    for (int i = 0; i < now_count; ++i) {
      kept.append(DrawingName(*now, i));
    }
    for (int i = 0; i < static_cast<int>(old.drawings.size()); ++i) {
      const QString name = DrawingName(old, i);
      const bool is_gone =
          is_by_count ? i >= now_count : !kept.contains(name);
      if (is_gone) {
        removed.append(old.name + QStringLiteral(": ") + name);
      }
    }
  }
  return removed;
}

}  // namespace snapper
