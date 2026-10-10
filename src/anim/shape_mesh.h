#ifndef SNAPPER_ANIM_SHAPE_MESH_H_
#define SNAPPER_ANIM_SHAPE_MESH_H_

#include <QImage>

#include <memory>
#include <vector>

#include "model/warp_mesh.h"

namespace snapper {

// A pixel counts for a shape mesh from this alpha (0 to 255) up:
// anything a viewer could see, so the mesh never cuts off a soft edge.
inline constexpr int kMeshAlpha = 8;

// About how many dots a mesh of detail has.
int DotsFor(MeshDetail detail);

// The shape mesh of a piece over its drawings (all the same size; a
// swap shows any of them, so the mesh covers them all): the visible
// outline of every shape, holes included, grown a pixel or two and
// straightened, with dots along it, more dots spread evenly inside, and
// triangles joining them whose sides follow the outline. Every visible
// pixel is inside a triangle, and no triangle lies where nothing is
// drawn but in the thin margin round the outline. A drawing with too
// many specks or too wiggly an outline for kMaxMeshPoints gets fewer,
// farther dots, and in the end a plain box of triangles round what is
// drawn. Nothing when nothing is visible. The same drawings and detail
// always give the same mesh.
std::shared_ptr<const WarpMesh> BuildShapeMesh(
    const std::vector<QImage>& drawings, MeshDetail detail);

}  // namespace snapper

#endif  // SNAPPER_ANIM_SHAPE_MESH_H_
