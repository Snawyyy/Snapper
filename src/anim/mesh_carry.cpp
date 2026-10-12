#include "anim/mesh_carry.h"

#include <algorithm>
#include <cassert>

#include "anim/warp.h"
#include "anim/warp_shape.h"

namespace snapper {
namespace {

// spot over a drawing of from moved onto one of size, stretching along.
QPointF Stretched(QPointF spot, QSize from, QSize size) {
  assert(from.width() >= 0 && from.height() >= 0);
  assert(size.width() >= 0 && size.height() >= 0);
  const double x = from.width() > 0
                       ? size.width() / static_cast<double>(from.width())
                       : 1.0;
  const double y = from.height() > 0
                       ? size.height() / static_cast<double>(from.height())
                       : 1.0;
  return QPointF(spot.x() * x, spot.y() * y);
}

std::vector<QPointF> Stretched(std::vector<QPointF> spots, QSize from,
                               QSize size) {
  assert(spots.size() <= static_cast<size_t>(kMaxWarpPoints));
  assert(size.width() >= 0);
  for (QPointF& spot : spots) {
    spot = Stretched(spot, from, size);
  }
  return spots;
}

// Whether point of rig has anything set on it.
bool HasSettings(const RigPiece& rig, int point) {
  assert(point >= 0);
  assert(rig.drag_nodes.size() <= static_cast<size_t>(kMaxWarpPoints));
  const bool has_reach =
      static_cast<size_t>(point) < rig.warp_reach.size() &&
      rig.warp_reach[static_cast<size_t>(point)] > 0.0;
  const bool has_node =
      std::any_of(rig.drag_nodes.begin(), rig.drag_nodes.end(),
                  [point](const DragNode& n) { return n.point == point; });
  return has_reach || has_node || FindMotion(rig, point) != nullptr;
}

// Moves point's settings of from onto dot of to.
void Move(const RigPiece& from, int point, int dot, RigPiece* to) {
  assert(to != nullptr);
  assert(point >= 0 && dot >= 0);
  const bool has_reach = static_cast<size_t>(point) < from.warp_reach.size();
  if (has_reach) {
    to->warp_reach[static_cast<size_t>(dot)] =
        from.warp_reach[static_cast<size_t>(point)];
  }
  for (const DragNode& node : from.drag_nodes) {
    const bool is_here = node.point == point;
    if (is_here) {
      DragNode moved = node;
      moved.point = dot;
      to->drag_nodes.push_back(moved);
    }
  }
  const PointMotion* motion = FindMotion(from, point);
  const bool is_moving = motion != nullptr;
  if (is_moving) {
    PointMotion moved = *motion;
    moved.point = dot;
    to->point_motions.push_back(moved);
  }
}

// Cloth covers an area, not a dot: every new dot left free whose
// nearest old dot (within reach) was a physics node becomes one too,
// so a finer warp keeps the whole patch of cloth swinging.
void FillCloth(const RigPiece& from, const std::vector<QPointF>& old,
               const std::vector<QPointF>& rest, double reach,
               std::vector<bool>* is_taken, RigPiece* to) {
  assert(is_taken != nullptr && to != nullptr);
  assert(is_taken->size() == rest.size());
  for (size_t i = 0; i < rest.size(); ++i) {
    const bool is_free = !(*is_taken)[i];
    const int nearest = is_free ? NearestPoint(old, rest[i], reach) : -1;
    const auto node =
        std::find_if(from.drag_nodes.begin(), from.drag_nodes.end(),
                     [nearest](const DragNode& n) {
                       return n.point == nearest &&
                              n.kind != NodeKind::kDrag;
                     });
    const bool is_cloth = nearest >= 0 && node != from.drag_nodes.end();
    if (is_cloth) {
      DragNode copy = *node;
      copy.point = static_cast<int>(i);
      to->drag_nodes.push_back(copy);
      (*is_taken)[i] = true;
    }
  }
}

}  // namespace

namespace {

// CarryDots for one set of settings: the fields of from, no layers.
CarryCount CarryOne(const RigPiece& from, QSize from_size,
                    const WarpGrid& fresh, QSize size, RigPiece* to) {
  assert(to != nullptr);
  assert(from.warp.PointCount() <= kMaxWarpPoints);
  *to = from;
  to->warp = fresh;
  to->warp_reach.assign(static_cast<size_t>(fresh.PointCount()), 0.0);
  to->drag_nodes.clear();
  to->point_motions.clear();
  CarryCount count;
  const std::vector<QPointF> old = RestPoints(from_size, from.warp);
  const std::vector<QPointF> rest = RestPoints(size, fresh);
  const bool is_on = fresh.IsOn();
  const QSizeF cell = is_on ? CellSize(fresh, size) : QSizeF();
  // A coarse old grid's dots sit far apart (and off the drawing at its
  // corners), so its own cell counts too.
  const bool was_on = from.warp.IsOn();
  const QSizeF old_cell = was_on ? CellSize(from.warp, size) : QSizeF();
  const double reach = std::max({cell.width(), cell.height(),
                                 old_cell.width(), old_cell.height()});
  std::vector<bool> is_taken(rest.size(), false);
  for (size_t i = 0; i < old.size(); ++i) {
    const int point = static_cast<int>(i);
    const bool has_settings = HasSettings(from, point);
    if (!has_settings) {
      continue;
    }
    const int dot =
        NearestPoint(rest, Stretched(old[i], from_size, size), reach);
    const bool is_free = dot >= 0 && !is_taken[static_cast<size_t>(dot)];
    if (!is_free) {
      ++count.dropped;
      continue;
    }
    is_taken[static_cast<size_t>(dot)] = true;
    Move(from, point, dot, to);
    ++count.carried;
  }
  FillCloth(from, Stretched(old, from_size, size), rest, reach, &is_taken,
            to);
  const bool has_reach = std::any_of(to->warp_reach.begin(),
                                     to->warp_reach.end(),
                                     [](double r) { return r > 0.0; });
  if (!has_reach) {
    to->warp_reach.clear();
  }
  return count;
}

}  // namespace

CarryCount CarryDots(const RigPiece& from, QSize from_size,
                     const WarpGrid& fresh, QSize size, RigPiece* to) {
  assert(to != nullptr);
  assert(from.layer_dots.size() < 1000000);
  const CarryCount count = CarryOne(from, from_size, fresh, size, to);
  // Each layer's own settings find their new dots the same way.
  to->layer_dots.clear();
  for (const auto& [layer, dots] : from.layer_dots) {
    RigPiece source = from;
    ShowDots(dots, &source);
    source.layer_dots.clear();
    RigPiece carried;
    CarryOne(source, from_size, fresh, size, &carried);
    to->layer_dots[layer] = {carried.warp_reach, carried.drag_nodes,
                             carried.point_motions};
  }
  return count;
}

std::vector<QPointF> CarryOffsets(const WarpGrid& old, QSize old_size,
                                  const std::vector<QPointF>& offsets,
                                  const WarpGrid& fresh, QSize size) {
  assert(offsets.size() <= static_cast<size_t>(kMaxWarpPoints));
  assert(fresh.PointCount() <= kMaxWarpPoints);
  const bool is_fitting = !offsets.empty() && old.IsOn() &&
                          static_cast<int>(offsets.size()) ==
                              old.PointCount();
  if (!is_fitting) {
    return {};
  }
  std::vector<QPointF> carried;
  for (const QPointF& spot : RestPoints(size, fresh)) {
    carried.push_back(
        CarriedAt(old, old_size, offsets, Stretched(spot, size, old_size)));
  }
  return carried;
}

}  // namespace snapper
