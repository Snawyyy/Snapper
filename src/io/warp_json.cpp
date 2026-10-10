#include "io/warp_json.h"

#include <QJsonArray>

#include <algorithm>
#include <array>
#include <cassert>
#include <vector>

namespace snapper {
namespace {

constexpr std::array<const char*, kMeshDetailCount> kDetailNames = {
    "few", "normal", "many"};

QJsonObject MeshToJson(const WarpMesh& mesh) {
  assert(mesh.points.size() <= static_cast<size_t>(kMaxMeshPoints));
  assert(mesh.triangles.size() <= static_cast<size_t>(kMaxMeshTriangles));
  QJsonArray points;
  for (const QPointF& point : mesh.points) {
    points.append(point.x());
    points.append(point.y());
  }
  QJsonArray triangles;
  for (const MeshTriangle& t : mesh.triangles) {
    for (const int corner : t) {
      triangles.append(corner);
    }
  }
  return QJsonObject{
      {"size", SizeToJson(mesh.size)},
      {"detail", EnumToJson(static_cast<int>(mesh.detail), kDetailNames)},
      {"points", points},
      {"triangles", triangles}};
}

std::shared_ptr<const WarpMesh> MeshFromJson(const QJsonObject& object,
                                             JsonIssues* issues) {
  assert(issues != nullptr);
  assert(kDetailNames.size() == static_cast<size_t>(kMeshDetailCount));
  const QJsonArray points = Bounded(object.value("points"),
                                    2 * kMaxMeshPoints, "mesh dots", issues);
  const QJsonArray corners =
      Bounded(object.value("triangles"), 3 * kMaxMeshTriangles,
              "mesh triangles", issues);
  std::vector<QPointF> dots;
  for (qsizetype i = 0; i + 1 < points.size(); i += 2) {
    dots.emplace_back(points.at(i).toDouble(), points.at(i + 1).toDouble());
  }
  std::vector<MeshTriangle> triangles;
  for (qsizetype i = 0; i + 2 < corners.size(); i += 3) {
    triangles.push_back({corners.at(i).toInt(-1), corners.at(i + 1).toInt(-1),
                         corners.at(i + 2).toInt(-1)});
  }
  const auto detail = static_cast<MeshDetail>(
      EnumFromJson(object.value("detail"), kDetailNames, issues));
  auto mesh = MakeWarpMesh(SizeFromJson(object.value("size")), detail,
                           std::move(dots), std::move(triangles));
  const bool is_broken = mesh == nullptr;
  if (is_broken) {
    issues->Note(QStringLiteral("a shape mesh is broken; its piece is not "
                                "warped"));
  }
  return mesh;
}

}  // namespace

void WarpToJson(const WarpGrid& grid, QJsonObject* piece) {
  assert(piece != nullptr);
  assert(grid.columns >= 0 && grid.rows >= 0);
  piece->insert("warp", QJsonArray{grid.columns, grid.rows});
  const bool is_mesh = grid.IsMesh();
  if (is_mesh) {
    piece->insert("mesh", MeshToJson(*grid.mesh));
  }
}

WarpGrid WarpFromJson(const QJsonObject& piece, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(kMaxWarpCells > 0);
  const bool is_mesh = piece.value("mesh").isObject();
  if (is_mesh) {
    return WarpGrid{0, 0, MeshFromJson(piece.value("mesh").toObject(),
                                       issues)};
  }
  const QJsonArray warp = piece.value("warp").toArray();
  return WarpGrid{std::clamp(warp.at(0).toInt(), 0, kMaxWarpCells),
                  std::clamp(warp.at(1).toInt(), 0, kMaxWarpCells)};
}

}  // namespace snapper
