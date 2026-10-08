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

}  // namespace snapper
