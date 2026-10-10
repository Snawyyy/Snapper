#include "edit/mesh_edits.h"

#include <cassert>
#include <memory>
#include <variant>

#include "anim/shape_mesh.h"
#include "base/text.h"

namespace snapper {
namespace {

// Every key of piece on every layer showing doll, its warp carried
// from old to fresh.
Project WithCarriedKeys(Project project, const QString& doll,
                        const QString& piece, const WarpGrid& old,
                        const WarpGrid& fresh, QSize size) {
  assert(!doll.isEmpty() && !piece.isEmpty());
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  for (auto& shared : project.shots) {
    Shot shot = *shared;
    bool is_changed = false;
    for (Layer& layer : shot.layers) {
      auto* posed = std::get_if<DollLayer>(&layer.content);
      const bool is_match = posed != nullptr && posed->doll == doll &&
                            posed->pieces.contains(piece);
      if (!is_match) {
        continue;
      }
      for (Key<PiecePose>& key : posed->pieces.at(piece).keys) {
        const bool has_warp = !key.value.warp.empty();
        if (has_warp) {
          key.value.warp =
              CarryOffsets(old, size, key.value.warp, fresh, size);
          is_changed = true;
        }
      }
    }
    if (is_changed) {
      shared = std::make_shared<const Shot>(std::move(shot));
    }
  }
  return project;
}

}  // namespace

std::vector<QImage> DrawingsOf(const Doll& doll, const QString& piece) {
  assert(!piece.isEmpty());
  const ArtPiece* art = FindArt(doll, piece);
  std::vector<QImage> drawings;
  const int count = art != nullptr ? static_cast<int>(art->drawings.size())
                                   : 0;
  assert(count <= kMaxPieceDrawings);
  for (int i = 0; i < count; ++i) {
    QImage drawing(DrawingPath(doll, piece, i));
    const bool is_read = !drawing.isNull();
    if (is_read) {
      drawings.push_back(std::move(drawing));
    }
  }
  return drawings;
}

Result<WarpGrid> ShapeMeshOf(const Doll& doll, const QString& piece,
                             MeshDetail detail) {
  assert(!piece.isEmpty());
  assert(static_cast<int>(detail) >= 0 &&
         static_cast<int>(detail) < kMeshDetailCount);
  const std::vector<QImage> drawings = DrawingsOf(doll, piece);
  const bool has_drawings = !drawings.empty();
  if (!has_drawings) {
    return std::unexpected(Error{
        Tr("The drawings of %1 can't be read, so it can't be meshed.")
            .arg(piece)});
  }
  auto mesh = BuildShapeMesh(drawings, detail);
  const bool is_made = mesh != nullptr;
  if (!is_made) {
    return std::unexpected(Error{
        Tr("There is nothing visible in %1 to bend.").arg(piece)});
  }
  return WarpGrid{0, 0, std::move(mesh)};
}

Result<Project> WithWarp(const Project& project, const QString& doll,
                         const QString& piece, const WarpGrid& fresh,
                         CarryCount* count) {
  assert(count != nullptr);
  assert(!piece.isEmpty());
  const Doll* found = FindDoll(project, doll);
  const RigPiece* rig = found != nullptr ? FindRig(found->rig, piece)
                                         : nullptr;
  const bool is_piece = rig != nullptr;
  if (!is_piece) {
    return std::unexpected(
        Error{Tr("%1 has no piece called %2.").arg(doll, piece)});
  }
  const QSize size = ArtSizeOf(*found, piece);
  const WarpGrid old = rig->warp;
  Doll changed = *found;
  *count += CarryDots(*rig, size, fresh, size, FindRig(&changed.rig, piece));
  Project next = project;
  next.dolls[doll] = std::make_shared<const Doll>(std::move(changed));
  return WithCarriedKeys(std::move(next), doll, piece, old, fresh, size);
}

Project WithFreshMeshes(const Project& project, const QString& doll,
                        CarryCount* count, QStringList* remeshed) {
  assert(count != nullptr && remeshed != nullptr);
  assert(!doll.isEmpty());
  Project next = project;
  const Doll* found = FindDoll(project, doll);
  const bool is_doll = found != nullptr;
  if (!is_doll) {
    return next;
  }
  for (const RigPiece& rig : found->rig.pieces) {
    const bool is_mesh = rig.warp.IsMesh();
    if (!is_mesh) {
      continue;
    }
    const Result<WarpGrid> fresh =
        ShapeMeshOf(*found, rig.name, rig.warp.mesh->detail);
    const bool is_new = fresh.has_value() && !(*fresh == rig.warp);
    if (!is_new) {
      continue;
    }
    Result<Project> changed = WithWarp(next, doll, rig.name, *fresh, count);
    if (changed) {
      next = std::move(*changed);
      remeshed->append(rig.name);
    }
  }
  return next;
}

}  // namespace snapper
