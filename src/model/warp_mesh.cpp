#include "model/warp_mesh.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <queue>

namespace snapper {
namespace {

double Distance(QPointF a, QPointF b) {
  assert(std::isfinite(a.x() + a.y()));
  assert(std::isfinite(b.x() + b.y()));
  return std::hypot(a.x() - b.x(), a.y() - b.y());
}

// Whether every corner is a dot, no triangle repeats a corner, and
// every dot is a corner of some triangle.
bool IsWhole(size_t count, const std::vector<MeshTriangle>& triangles) {
  assert(count <= static_cast<size_t>(kMaxMeshPoints));
  assert(triangles.size() <= static_cast<size_t>(kMaxMeshTriangles));
  std::vector<bool> is_used(count, false);
  for (const MeshTriangle& t : triangles) {
    for (const int corner : t) {
      const bool is_dot = corner >= 0 && static_cast<size_t>(corner) < count;
      if (!is_dot) {
        return false;
      }
      is_used[static_cast<size_t>(corner)] = true;
    }
    const bool is_repeated = t[0] == t[1] || t[1] == t[2] || t[0] == t[2];
    if (is_repeated) {
      return false;
    }
  }
  return std::all_of(is_used.begin(), is_used.end(),
                     [](bool used) { return used; });
}

std::vector<MeshEdge> EdgesOf(const std::vector<MeshTriangle>& triangles) {
  assert(!triangles.empty());
  assert(triangles.size() <= static_cast<size_t>(kMaxMeshTriangles));
  std::vector<MeshEdge> edges;
  edges.reserve(triangles.size() * 3);
  for (const MeshTriangle& t : triangles) {
    for (size_t i = 0; i < 3; ++i) {
      const int a = t[i];
      const int b = t[(i + 1) % 3];
      edges.emplace_back(std::min(a, b), std::max(a, b));
    }
  }
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  return edges;
}

// Shortest walks along the sides from every dot, in cells.
std::vector<float> ApartOf(const WarpMesh& mesh) {
  assert(mesh.cell > 0.0);
  assert(!mesh.edges.empty());
  const size_t count = mesh.points.size();
  std::vector<std::vector<std::pair<int, float>>> next(count);
  for (const MeshEdge& edge : mesh.edges) {
    const auto a = static_cast<size_t>(edge.first);
    const auto b = static_cast<size_t>(edge.second);
    const auto cells = static_cast<float>(
        Distance(mesh.points[a], mesh.points[b]) / mesh.cell);
    next[a].emplace_back(edge.second, cells);
    next[b].emplace_back(edge.first, cells);
  }
  std::vector<float> apart(count * count, kFarApart);
  using Step = std::pair<float, int>;
  const size_t most_steps = 2 * mesh.edges.size() + 1;
  for (size_t from = 0; from < count; ++from) {
    float* row = &apart[from * count];
    std::priority_queue<Step, std::vector<Step>, std::greater<Step>> todo;
    row[from] = 0.0f;
    todo.emplace(0.0f, static_cast<int>(from));
    for (size_t step = 0; step < most_steps && !todo.empty(); ++step) {
      const auto [cells, at] = todo.top();
      todo.pop();
      const bool is_stale = cells > row[static_cast<size_t>(at)];
      if (is_stale) {
        continue;
      }
      for (const auto& [to, length] : next[static_cast<size_t>(at)]) {
        const float walked = cells + length;
        const bool is_shorter = walked < row[static_cast<size_t>(to)];
        if (is_shorter) {
          row[static_cast<size_t>(to)] = walked;
          todo.emplace(walked, to);
        }
      }
    }
  }
  return apart;
}

std::vector<int> MirrorOf(const WarpMesh& mesh) {
  assert(!mesh.points.empty());
  assert(mesh.points.size() <= static_cast<size_t>(kMaxMeshPoints));
  std::vector<int> mirror(mesh.points.size(), 0);
  for (size_t i = 0; i < mesh.points.size(); ++i) {
    const QPointF spot(mesh.size.width() - mesh.points[i].x(),
                       mesh.points[i].y());
    double best = -1.0;
    for (size_t j = 0; j < mesh.points.size(); ++j) {
      const double away = Distance(spot, mesh.points[j]);
      const bool is_nearer = best < 0.0 || away < best;
      if (is_nearer) {
        best = away;
        mirror[i] = static_cast<int>(j);
      }
    }
  }
  return mirror;
}

}  // namespace

std::shared_ptr<const WarpMesh> MakeWarpMesh(
    QSize size, MeshDetail detail, std::vector<QPointF> points,
    std::vector<MeshTriangle> triangles) {
  const bool is_sized =
      !points.empty() && !triangles.empty() &&
      points.size() <= static_cast<size_t>(kMaxMeshPoints) &&
      triangles.size() <= static_cast<size_t>(kMaxMeshTriangles) &&
      size.width() >= 0 && size.height() >= 0;
  const bool is_finite =
      std::all_of(points.begin(), points.end(), [](QPointF p) {
        return std::isfinite(p.x()) && std::isfinite(p.y());
      });
  const bool is_known = static_cast<int>(detail) >= 0 &&
                        static_cast<int>(detail) < kMeshDetailCount;
  const bool is_mesh = is_sized && is_finite && is_known &&
                       IsWhole(points.size(), triangles);
  if (!is_mesh) {
    return nullptr;
  }
  auto mesh = std::make_shared<WarpMesh>();
  mesh->size = size;
  mesh->detail = detail;
  mesh->points = std::move(points);
  mesh->triangles = std::move(triangles);
  mesh->edges = EdgesOf(mesh->triangles);
  double total = 0.0;
  for (const MeshEdge& edge : mesh->edges) {
    total += Distance(mesh->points[static_cast<size_t>(edge.first)],
                      mesh->points[static_cast<size_t>(edge.second)]);
  }
  mesh->cell = std::max(total / static_cast<double>(mesh->edges.size()), 1e-3);
  mesh->apart = ApartOf(*mesh);
  mesh->mirror = MirrorOf(*mesh);
  assert(mesh->apart.size() == mesh->points.size() * mesh->points.size());
  assert(mesh->mirror.size() == mesh->points.size());
  return mesh;
}

}  // namespace snapper
