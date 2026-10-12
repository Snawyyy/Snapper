#include "model/doll.h"

#include <QDir>

#include <algorithm>
#include <cassert>

namespace snapper {
namespace {

// The item called name in items, or nullptr.
template <typename T>
const T* FindNamed(const std::vector<T>& items, const QString& name) {
  assert(!name.isEmpty());
  assert(items.size() <= static_cast<size_t>(kMaxDollPieces));
  const auto found =
      std::find_if(items.begin(), items.end(),
                   [&name](const T& item) { return item.name == name; });
  return found == items.end() ? nullptr : &*found;
}

}  // namespace

const ArtPiece* FindArt(const Doll& doll, const QString& name) {
  assert(doll.art.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(!name.isEmpty());
  return FindNamed(doll.art.pieces, name);
}

QString DrawingName(const ArtPiece& piece, int index) {
  assert(index >= 0);
  assert(index < static_cast<int>(piece.drawings.size()));
  const size_t at = static_cast<size_t>(index);
  const bool is_named =
      at < piece.layer_names.size() && !piece.layer_names[at].isEmpty();
  return is_named ? piece.layer_names[at]
                  : QStringLiteral("%1 %2").arg(piece.name).arg(index + 1);
}

int FindDrawing(const ArtPiece& piece, const QString& name) {
  assert(piece.drawings.size() < 100000);
  assert(name.size() < 100000);
  const int count = static_cast<int>(piece.drawings.size());
  const bool is_named = !name.isEmpty();
  for (int i = 0; is_named && i < count; ++i) {
    if (DrawingName(piece, i) == name) {
      return i;
    }
  }
  return -1;
}

QSize ArtSizeOf(const Doll& doll, const QString& name) {
  const ArtPiece* art = FindArt(doll, name);
  assert(art == nullptr || art->size.width() >= 0);
  assert(!name.isEmpty());
  return art != nullptr ? art->size : QSize();
}

const RigPiece* FindRig(const Rig& rig, const QString& name) {
  assert(rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(!name.isEmpty());
  return FindNamed(rig.pieces, name);
}

RigPiece* FindRig(Rig* rig, const QString& name) {
  assert(rig != nullptr);
  assert(!name.isEmpty());
  const RigPiece* piece = FindRig(static_cast<const Rig&>(*rig), name);
  return const_cast<RigPiece*>(piece);
}

const PointMotion* FindMotion(const RigPiece& rig, int point) {
  assert(rig.point_motions.size() <= static_cast<size_t>(kMaxWarpPoints));
  assert(point >= -1);
  const auto found =
      std::find_if(rig.point_motions.begin(), rig.point_motions.end(),
                   [point](const PointMotion& m) { return m.point == point; });
  return found == rig.point_motions.end() ? nullptr : &*found;
}

double ReachOf(const RigPiece& rig, int point) {
  assert(rig.warp_reach.size() <= static_cast<size_t>(kMaxWarpPoints));
  assert(point >= 0);
  const bool has_reach =
      static_cast<int>(rig.warp_reach.size()) == rig.warp.PointCount() &&
      point < rig.warp.PointCount();
  return has_reach ? rig.warp_reach[static_cast<size_t>(point)] : 0.0;
}

const IkChain* FindChain(const Rig& rig, const QString& name) {
  assert(rig.chains.size() <= static_cast<size_t>(kMaxIkChains));
  assert(!name.isEmpty());
  return FindNamed(rig.chains, name);
}

bool CanParent(const Rig& rig, const QString& child, const QString& parent) {
  assert(!child.isEmpty());
  assert(rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const bool is_root = parent.isEmpty();
  if (is_root) {
    return FindRig(rig, child) != nullptr;
  }
  const bool are_known =
      FindRig(rig, child) != nullptr && FindRig(rig, parent) != nullptr;
  if (!are_known) {
    return false;
  }
  // Walk up from parent; meeting child means a loop. A tree is at most
  // kMaxDollPieces deep.
  QString at = parent;
  for (int depth = 0; depth <= kMaxDollPieces; ++depth) {
    const bool is_loop = at == child;
    if (is_loop) {
      return false;
    }
    const RigPiece* piece = FindRig(rig, at);
    const bool is_top = piece == nullptr || piece->parent.isEmpty();
    if (is_top) {
      return true;
    }
    at = piece->parent;
  }
  return false;
}

QString DrawingPath(const Doll& doll, const QString& piece, int drawing) {
  assert(!piece.isEmpty());
  assert(drawing >= -1);
  const ArtPiece* art = FindArt(doll, piece);
  const RigPiece* rig = FindRig(doll.rig, piece);
  const bool is_known = art != nullptr && !art->drawings.empty();
  if (!is_known) {
    return QString();
  }
  int index = drawing;
  const bool is_default = index < 0;
  if (is_default) {
    const bool has_rig_default = rig != nullptr && rig->default_drawing >= 0;
    index = has_rig_default ? rig->default_drawing : art->default_drawing;
  }
  const bool is_in_range =
      index >= 0 && index < static_cast<int>(art->drawings.size());
  if (!is_in_range) {
    return QString();
  }
  return QDir(doll.folder).filePath(art->drawings[static_cast<size_t>(index)]);
}

const OwnDrawing* OwnDrawingAt(const Doll& doll, const QString& piece,
                               int index) {
  assert(!piece.isEmpty());
  assert(index >= -1);
  const ArtPiece* art = FindArt(doll, piece);
  const RigPiece* rig = FindRig(doll.rig, piece);
  const bool is_known = art != nullptr && rig != nullptr && index >= 0 &&
                        index < static_cast<int>(art->drawings.size());
  if (!is_known) {
    return nullptr;
  }
  const QString& file = art->drawings[static_cast<size_t>(index)];
  const auto found = std::find_if(
      rig->own_drawings.begin(), rig->own_drawings.end(),
      [&file](const OwnDrawing& own) { return own.file == file; });
  return found == rig->own_drawings.end() ? nullptr : &*found;
}

PieceDots DotsOf(const RigPiece& piece, LayerId layer) {
  assert(piece.layer_dots.size() < 1000000);
  assert(layer.value() >= 0);
  const auto own = piece.layer_dots.find(layer);
  const bool has_own = own != piece.layer_dots.end();
  return has_own ? own->second
                 : PieceDots{piece.warp_reach, piece.drag_nodes,
                             piece.point_motions};
}

const std::vector<DragNode>& DragNodesOf(const RigPiece& piece, LayerId layer) {
  assert(piece.layer_dots.size() < 1000000);
  assert(layer.value() >= 0);
  const auto own = piece.layer_dots.find(layer);
  const bool has_own = own != piece.layer_dots.end();
  return has_own ? own->second.drag_nodes : piece.drag_nodes;
}

void ShowDots(const PieceDots& dots, RigPiece* piece) {
  assert(piece != nullptr);
  assert(dots.drag_nodes.size() <= static_cast<size_t>(kMaxWarpPoints));
  piece->warp_reach = dots.warp_reach;
  piece->drag_nodes = dots.drag_nodes;
  piece->point_motions = dots.point_motions;
}

RigPiece PieceFor(const RigPiece& piece, LayerId layer) {
  assert(layer.value() >= 0);
  assert(piece.layer_dots.size() < 1000000);
  RigPiece shown = piece;
  ShowDots(DotsOf(piece, layer), &shown);
  shown.layer_dots.clear();
  return shown;
}

DollFor::DollFor(const Doll& doll, LayerId layer)
    : shared_(&doll) {
  assert(layer.value() >= 0);
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const bool is_own = std::any_of(
      doll.rig.pieces.begin(), doll.rig.pieces.end(),
      [layer](const RigPiece& piece) {
        return piece.layer_dots.contains(layer);
      });
  if (!is_own) {
    return;
  }
  own_ = std::make_unique<Doll>(doll);
  for (RigPiece& piece : own_->rig.pieces) {
    piece = PieceFor(piece, layer);
  }
}

}  // namespace snapper
