#ifndef SNAPPER_MODEL_WARP_MESH_H_
#define SNAPPER_MODEL_WARP_MESH_H_

#include <QPointF>
#include <QSize>

#include <array>
#include <memory>
#include <utility>
#include <vector>

namespace snapper {

// The most dots a shape mesh has, and the most triangles (a
// triangulation of n dots has fewer than 2n).
constexpr int kMaxMeshPoints = 256;
constexpr int kMaxMeshTriangles = 2 * kMaxMeshPoints;

// How close together a shape mesh's dots are.
enum class MeshDetail { kFew, kNormal, kMany };
constexpr int kMeshDetailCount = 3;

// Three dots of a mesh, by index.
using MeshTriangle = std::array<int, 3>;
// Two dots of a mesh joined by a triangle's side, lower index first.
using MeshEdge = std::pair<int, int>;

// Two dots with no way between them along the mesh are this many
// cells apart: further than any reach.
constexpr float kFarApart = 1.0e6f;

// A warp that hugs its drawing: dots along the drawing's visible
// outline and spread evenly inside it, joined into triangles, with no
// dot or triangle out where nothing is drawn. A file keeps the drawing
// size it was made over, its detail, the dots at rest (drawing pixels)
// and the triangles; everything else is worked out from those once,
// when the mesh is made (MakeWarpMesh), and shared by every copy.
struct WarpMesh final {
  QSize size;
  MeshDetail detail = MeshDetail::kNormal;
  std::vector<QPointF> points;
  std::vector<MeshTriangle> triangles;
  // Each side of a triangle once, sorted.
  std::vector<MeshEdge> edges;
  // The average side at rest, in drawing pixels: one cell of reach.
  double cell = 1.0;
  // How many cells apart each two dots are, walking along the sides
  // (so a pull never jumps a gap in the drawing): row by row, one row
  // per dot.
  std::vector<float> apart;
  // The dot nearest each dot's spot mirrored left to right across the
  // drawing.
  std::vector<int> mirror;

  bool operator==(const WarpMesh& other) const {
    return size == other.size && detail == other.detail &&
           points == other.points && triangles == other.triangles;
  }
};

// The mesh of these dots and triangles over a drawing of size, with
// everything worked out. Nothing when they do not make a mesh: no
// triangle, more than kMaxMeshPoints dots or kMaxMeshTriangles
// triangles, a corner that is not a dot, a triangle with a corner
// twice, a dot no triangle uses, or a number that is not finite.
std::shared_ptr<const WarpMesh> MakeWarpMesh(
    QSize size, MeshDetail detail, std::vector<QPointF> points,
    std::vector<MeshTriangle> triangles);

}  // namespace snapper

#endif  // SNAPPER_MODEL_WARP_MESH_H_
