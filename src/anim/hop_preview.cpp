#include "anim/hop_preview.h"

#include <QPointF>
#include <QRectF>
#include <QSize>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>

#include "anim/cloth_parts.h"
#include "anim/collider.h"
#include "anim/jelly.h"
#include "anim/warp.h"
#include "model/warp_mesh.h"

namespace snapper {
namespace {

constexpr double kPi = 3.141592653589793;
// The blob's half width and half height, in preview pixels.
constexpr double kRadiusX = 20.0;
constexpr double kRadiusY = 16.0;
// Its dots: one in the middle, an inner ring and an edge ring. The
// middle and the inner ring are its core, pins that follow the body as
// a doll's undragged dots do; the edge ring is the material.
constexpr int kInner = 6;
constexpr double kCore = 0.3;
constexpr int kEdge = 2 * kInner;
constexpr int kFirstEdge = 1 + kInner;
constexpr int kDotCount = kFirstEdge + kEdge;
// The edge dot at the top, where a Drag preview hangs its spring, and
// how far (in mesh cells) its pull spreads, as a dot's reach does.
constexpr int kTopDot = kFirstEdge + 3 * kEdge / 4;
constexpr double kSpringReach = 2.0;
// The body rests this far above the floor, so its bottom reads as out.
constexpr double kFloorGap = 2.0 * kColliderClearance;

QPointF OnRing(double angle, double share) {
  assert(std::isfinite(angle));
  assert(share >= 0.0 && share <= 1.0);
  return {kRadiusX * (1.0 + share * std::cos(angle)),
          kRadiusY * (1.0 + share * std::sin(angle))};
}

// The blob's mesh, in its own drawing pixels.
WarpGrid BlobGrid() {
  std::vector<QPointF> points{OnRing(0.0, 0.0)};
  for (int j = 0; j < kInner; ++j) {
    points.push_back(OnRing(2.0 * kPi * j / kInner, kCore));
  }
  for (int k = 0; k < kEdge; ++k) {
    points.push_back(OnRing(2.0 * kPi * k / kEdge, 1.0));
  }
  std::vector<MeshTriangle> triangles;
  for (int j = 0; j < kInner; ++j) {
    const int in = 1 + j;
    const int in_next = 1 + (j + 1) % kInner;
    const int out = kFirstEdge + 2 * j;
    const int out_mid = out + 1;
    const int out_next = kFirstEdge + (2 * j + 2) % kEdge;
    triangles.push_back({0, in, in_next});
    triangles.push_back({in, out, out_mid});
    triangles.push_back({in, out_mid, in_next});
    triangles.push_back({in_next, out_mid, out_next});
  }
  const QSize size(static_cast<int>(2 * kRadiusX) + 1,
                   static_cast<int>(2 * kRadiusY) + 1);
  WarpGrid grid;
  grid.mesh = MakeWarpMesh(size, MeshDetail::kFew, points, triangles);
  assert(grid.mesh != nullptr);
  assert(grid.PointCount() == kDotCount);
  return grid;
}

// Every dot of the blob as the body carries it at frame.
std::vector<QPointF> CarriedAt(const WarpGrid& grid, int frame) {
  assert(grid.PointCount() == kDotCount);
  assert(frame >= 0 && frame < kHopFrames);
  const HopPose pose = HopAt(frame);
  const QPointF middle(0.0, -(kRadiusY + kFloorGap + pose.height));
  const double c = std::cos(pose.lean);
  const double s = std::sin(pose.lean);
  std::vector<QPointF> carried;
  for (const QPointF& rest : grid.mesh->points) {
    const QPointF local = rest - QPointF(kRadiusX, kRadiusY);
    carried.push_back(middle + QPointF(local.x() * c - local.y() * s,
                                       local.x() * s + local.y() * c));
  }
  return carried;
}

// The floor as a collider: everything below y 0 is inside. Each dot's
// ghost is where it stands landed.
PushField FloorField(const std::vector<QPointF>& landed) {
  assert(landed.size() == static_cast<size_t>(kDotCount));
  assert(kFloorGap > 0.0);
  constexpr double kWide = 1000.0;
  PushField floor;
  floor.edges.push_back({QPointF(-kWide, 0.0), QPointF(kWide, 0.0),
                         QPointF(0.0, -1.0)});
  floor.box = QRectF(-kWide, 0.0, 2.0 * kWide, kWide);
  floor.ghosts = landed;
  return floor;
}

}  // namespace

HopPose HopAt(int frame) {
  assert(kHopAirFrames > 0 && kHopAirFrames < kHopFrames);
  const int at = ((frame % kHopFrames) + kHopFrames) % kHopFrames;
  const int lift_off = kHopFrames - kHopAirFrames;
  const bool is_flying = at > lift_off;
  if (!is_flying) {
    return {};
  }
  const double t = static_cast<double>(at - lift_off) / kHopAirFrames;
  assert(t > 0.0 && t < 1.0);
  return {kHopHeight * 4.0 * t * (1.0 - t), kHopLean * std::sin(kPi * t)};
}

HopPreview::HopPreview(const DragNode* node) : grid_(BlobGrid()) {
  assert(grid_.mesh != nullptr);
  assert(kTopDot < kDotCount);
  carried_ = CarriedAt(grid_, 0);
  dots_ = carried_;
  const bool has_node = node != nullptr;
  if (!has_node) {
    return;
  }
  node_ = *node;
  spring_ = DragSpringOf(*node);
  spring_at_ = carried_[kTopDot];
  const bool is_spring = node->kind == NodeKind::kDrag;
  if (is_spring) {
    return;
  }
  piece_.grid = grid_;
  piece_.cell = grid_.mesh->cell;
  piece_.fields.push_back(FloorField(carried_));
  for (int point = kFirstEdge; point < kDotCount; ++point) {
    piece_.points.push_back(point);
    piece_.settings.push_back(ClothSettingsOf(*node));
  }
  const size_t count = piece_.points.size();
  piece_.at.assign(count, QPointF());
  piece_.speed.assign(count, QPointF());
  piece_.body_speed.assign(count, QPointF());
  RestCloth(carried_, &piece_);
}

void HopPreview::SetNode(const DragNode* node) {
  assert(grid_.mesh != nullptr);
  assert(piece_.settings.size() == piece_.points.size());
  const bool is_same_kind = node != nullptr && node_.has_value() &&
                            node->kind == node_->kind;
  if (!is_same_kind) {
    *this = HopPreview(node);
    return;
  }
  node_ = *node;
  spring_ = DragSpringOf(*node);
  const bool is_piece = !piece_.points.empty();
  if (is_piece) {
    piece_.settings.assign(piece_.points.size(), ClothSettingsOf(*node));
  }
}

void HopPreview::Rest() {
  assert(grid_.mesh != nullptr);
  assert(frame_ >= 0);
  const std::optional<DragNode> node = node_;
  *this = HopPreview(node.has_value() ? &*node : nullptr);
}

void HopPreview::Step() {
  assert(carried_.size() == static_cast<size_t>(kDotCount));
  assert(frame_ >= 0 && frame_ < kHopFrames);
  const bool is_still = !node_.has_value();
  if (is_still) {
    return;
  }
  frame_ = (frame_ + 1) % kHopFrames;
  const std::vector<QPointF> was = carried_;
  carried_ = CarriedAt(grid_, frame_);
  const bool is_spring = node_->kind == NodeKind::kDrag;
  if (is_spring) {
    StepSpring();
  } else {
    StepPiece(was);
  }
}

void HopPreview::StepPiece(const std::vector<QPointF>& was) {
  assert(was.size() == carried_.size());
  assert(node_.has_value());
  const double lean = HopAt(frame_).lean;
  piece_.drawn_down = QPointF(-std::sin(lean), std::cos(lean));
  for (size_t i = 0; i < piece_.points.size(); ++i) {
    const size_t point = static_cast<size_t>(piece_.points[i]);
    piece_.body_speed[i] = carried_[point] - was[point];
  }
  const bool is_jelly = node_->kind == NodeKind::kJelly;
  if (is_jelly) {
    StepJelly(carried_, &piece_);
  } else {
    StepCloth(carried_, &piece_);
  }
  dots_ = carried_;
  for (size_t i = 0; i < piece_.points.size(); ++i) {
    dots_[static_cast<size_t>(piece_.points[i])] = piece_.at[i];
  }
}

void HopPreview::StepSpring() {
  assert(node_.has_value());
  assert(carried_.size() == static_cast<size_t>(kDotCount));
  const QPointF goal = carried_[kTopDot];
  StepDragSpring(spring_, goal, &spring_at_, &spring_speed_);
  const QPointF trail = spring_at_ - goal;
  dots_ = carried_;
  for (int point = 0; point < kDotCount; ++point) {
    dots_[static_cast<size_t>(point)] +=
        trail * PullWeight(grid_, kTopDot, point, kSpringReach);
  }
}

std::vector<int> HopPreview::Outline() {
  assert(kEdge > 2);
  assert(kFirstEdge + kEdge == kDotCount);
  std::vector<int> outline;
  for (int point = kFirstEdge; point < kDotCount; ++point) {
    outline.push_back(point);
  }
  return outline;
}

int HopPreview::spring_dot() const {
  assert(kTopDot < kDotCount);
  assert(dots_.size() == static_cast<size_t>(kDotCount));
  const bool is_spring =
      node_.has_value() && node_->kind == NodeKind::kDrag;
  return is_spring ? kTopDot : -1;
}

double HopPreview::Bend() const {
  assert(dots_.size() == carried_.size());
  assert(!dots_.empty());
  double most = 0.0;
  for (size_t k = 0; k < dots_.size(); ++k) {
    most = std::max(most, Length(dots_[k] - carried_[k]));
  }
  return most;
}

}  // namespace snapper
