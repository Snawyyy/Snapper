#include "anim/jelly.h"

#include <QPointF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <vector>

#include "anim/cloth_parts.h"
#include "anim/jelly_fit.h"
#include "anim/warp_shape.h"
#include "model/node_physics.h"

namespace snapper {
namespace {

// How many sides from a pin a dot swings less than a free one: the swing
// grows smoothly away from what is still instead of shearing at its edge.
constexpr int kRampSides = 3;
// How much a blob's squash and stretch springs back more than its shift
// at no squish, as a share of the way left to a full pull.
constexpr double kStiffSquash = 0.6;
// How much of its area a blob keeps while it squashes, and how far it
// may squash or stretch at no squish and at most squish.
constexpr double kKeepArea = 0.6;
constexpr double kBendStiff = 0.2;
constexpr double kBendSoft = 0.6;
// The least a side may be squeezed to, as a share of its drawn length,
// and the rounds of pushing apart per frame (a net under the smooth
// fit, for a blob a collider squeezes hard).
constexpr double kLeastSide = 0.5;
constexpr int kApartRounds = 4;

// How much of its squish a node has, 0 to 1.
double SquishShare(const ClothSettings& settings) {
  assert(kMostSquish > 0.0);
  assert(kMostSquish < 1.0);
  return std::clamp(settings.stretch / kMostSquish, 0.0, 1.0);
}

// A group of jelly dots joined by sides of the mesh: it swings as one
// blob, spots measured from its middle in its own size.
struct Clump final {
  std::vector<size_t> nodes;
  std::vector<QPointF> spots;
};

// The node of each grid point, or -1 for a pin.
std::vector<int> NodeOfPoints(const ClothPiece& piece) {
  assert(piece.points.size() == piece.at.size());
  assert(piece.grid.PointCount() <= kMaxWarpPoints);
  std::vector<int> node_of(static_cast<size_t>(piece.grid.PointCount()), -1);
  for (size_t i = 0; i < piece.points.size(); ++i) {
    node_of[static_cast<size_t>(piece.points[i])] = static_cast<int>(i);
  }
  return node_of;
}

size_t RootOf(std::vector<size_t>* parent, size_t i) {
  assert(parent != nullptr && i < parent->size());
  assert(parent->size() <= static_cast<size_t>(kMaxWarpPoints));
  while ((*parent)[i] != i) {
    (*parent)[i] = (*parent)[(*parent)[i]];
    i = (*parent)[i];
  }
  return i;
}

// The clumps of piece: dots joined by a side of the mesh, each with its
// dots' spots about the middle of where the body carries them.
std::vector<Clump> ClumpsOf(const std::vector<int>& node_of,
                            const std::vector<QPointF>& carried,
                            const ClothPiece& piece) {
  assert(node_of.size() == carried.size());
  assert(piece.points.size() <= static_cast<size_t>(kMaxWarpPoints));
  std::vector<size_t> parent(piece.points.size());
  std::iota(parent.begin(), parent.end(), size_t{0});
  for (const MeshEdge& edge : WarpEdges(piece.grid)) {
    const int a = node_of[static_cast<size_t>(edge.first)];
    const int b = node_of[static_cast<size_t>(edge.second)];
    const bool is_pair = a >= 0 && b >= 0;
    if (is_pair) {
      parent[RootOf(&parent, static_cast<size_t>(a))] =
          RootOf(&parent, static_cast<size_t>(b));
    }
  }
  std::vector<Clump> clumps;
  std::vector<int> clump_of(parent.size(), -1);
  for (size_t i = 0; i < parent.size(); ++i) {
    const size_t root = RootOf(&parent, i);
    const bool is_new = clump_of[root] < 0;
    if (is_new) {
      clump_of[root] = static_cast<int>(clumps.size());
      clumps.emplace_back();
    }
    clumps[static_cast<size_t>(clump_of[root])].nodes.push_back(i);
  }
  for (Clump& clump : clumps) {
    QPointF sum;
    for (const size_t i : clump.nodes) {
      sum += carried[static_cast<size_t>(piece.points[i])];
    }
    const QPointF mid = sum / static_cast<double>(clump.nodes.size());
    double radius = std::max(piece.cell, 1e-6);
    for (const size_t i : clump.nodes) {
      radius = std::max(
          radius, Length(carried[static_cast<size_t>(piece.points[i])] - mid));
    }
    for (const size_t i : clump.nodes) {
      clump.spots.push_back(
          (carried[static_cast<size_t>(piece.points[i])] - mid) / radius);
    }
  }
  return clumps;
}

// How much of the swing each dot takes, 0 to 1, by how many sides it is
// from the nearest pin (a grid point with no dot): a dot next to a pin
// follows the body, and the share grows smoothly over kRampSides. A dot
// that no pin is near takes all of it.
std::vector<double> WeightsOf(const ClothPiece& piece) {
  assert(piece.points.size() == piece.at.size());
  assert(kRampSides > 0);
  constexpr int kFar = 1000;
  std::vector<int> sides(static_cast<size_t>(piece.grid.PointCount()), 0);
  for (const int point : piece.points) {
    sides[static_cast<size_t>(point)] = kFar;
  }
  const std::vector<MeshEdge> edges = WarpEdges(piece.grid);
  for (int round = 0; round < kRampSides; ++round) {
    for (const MeshEdge& edge : edges) {
      int& a = sides[static_cast<size_t>(edge.first)];
      int& b = sides[static_cast<size_t>(edge.second)];
      const int near_a = std::min(a, b + 1);
      const int near_b = std::min(b, a + 1);
      a = near_a;
      b = near_b;
    }
  }
  std::vector<double> weights;
  for (const int point : piece.points) {
    const double x = std::min(
        1.0, static_cast<double>(sides[static_cast<size_t>(point)]) /
                 kRampSides);
    weights.push_back(x * x * (3.0 - 2.0 * x));
  }
  return weights;
}

// How far each dot of clump is from where it was a frame ago, by its
// own motion: the node's offset and its kept speed (with the droop).
struct Motion final {
  std::vector<QPointF> offsets;
  std::vector<QPointF> owns;
};

Motion MotionOf(const std::vector<QPointF>& carried, const ClothPiece& piece) {
  assert(static_cast<int>(carried.size()) == piece.grid.PointCount());
  assert(piece.at.size() == piece.speed.size());
  Motion motion;
  const QPointF turned = QPointF(0.0, 1.0) - piece.drawn_down;
  for (size_t i = 0; i < piece.at.size(); ++i) {
    const ClothSettings& s = piece.settings[i];
    const QPointF anchor = carried[static_cast<size_t>(piece.points[i])];
    const QPointF body = piece.body_speed[i];
    motion.offsets.push_back(piece.at[i] - (anchor - body));
    motion.owns.push_back((piece.speed[i] - body) * s.keep +
                          turned * (s.fall * piece.cell));
  }
  return motion;
}

// One clump's next swing: what it is now and what it keeps of its own
// speed, with the shift and the squash each pulled back a little.
Swing NextSwing(const Clump& clump, const std::vector<double>& weights,
                const Motion& motion, const ClothPiece& piece) {
  assert(!clump.nodes.empty());
  assert(clump.nodes.size() == clump.spots.size());
  std::vector<double> w;
  std::vector<QPointF> offsets;
  std::vector<QPointF> owns;
  double pull = 0.0;
  double share = 0.0;
  for (const size_t i : clump.nodes) {
    w.push_back(weights[i]);
    offsets.push_back(motion.offsets[i]);
    owns.push_back(motion.owns[i]);
    pull += piece.settings[i].pull;
    share += SquishShare(piece.settings[i]);
  }
  const double n = static_cast<double>(clump.nodes.size());
  pull /= n;
  share /= n;
  const Pull back{pull, pull + (1.0 - pull) * kStiffSquash * (1.0 - share)};
  const Swing now = FitSwing(clump.spots, w, offsets);
  const Swing own = FitSwing(clump.spots, w, owns);
  const double most = kBendStiff + (kBendSoft - kBendStiff) * share;
  return Bounded(Plus(PulledBack(now, back), own), kKeepArea, most);
}

// Pushes apart every pair of dots (a node and a node, or a node and a
// pin) squeezed closer than kLeastSide of their drawn distance.
void KeepApart(const std::vector<int>& node_of,
               const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(node_of.size() == carried.size());
  const std::vector<MeshEdge> edges = WarpEdges(piece->grid);
  for (int round = 0; round < kApartRounds; ++round) {
    for (const MeshEdge& edge : edges) {
      const size_t first = static_cast<size_t>(edge.first);
      const size_t second = static_cast<size_t>(edge.second);
      const int a = node_of[first];
      const int b = node_of[second];
      const bool is_a_node = a >= 0;
      const bool is_b_node = b >= 0;
      const bool has_node = is_a_node || is_b_node;
      if (!has_node) {
        continue;
      }
      const QPointF from =
          is_a_node ? piece->at[static_cast<size_t>(a)] : carried[first];
      const QPointF to =
          is_b_node ? piece->at[static_cast<size_t>(b)] : carried[second];
      const double least =
          kLeastSide * Length(carried[second] - carried[first]);
      const double length = Length(to - from);
      const bool is_squeezed = length < least && length > 1e-9;
      if (!is_squeezed) {
        continue;
      }
      const QPointF push = (to - from) * ((least - length) / length);
      const double share = is_a_node && is_b_node ? 0.5 : 1.0;
      if (is_a_node) {
        piece->at[static_cast<size_t>(a)] -= push * share;
      }
      if (is_b_node) {
        piece->at[static_cast<size_t>(b)] += push * share;
      }
    }
  }
}

// The farthest a jelly node may stray from where the body carries it:
// one blob radius.
double ReachOf(const std::vector<QPointF>& carried, double cell) {
  assert(!carried.empty());
  assert(cell >= 0.0);
  QPointF sum;
  for (const QPointF& point : carried) {
    sum += point;
  }
  const QPointF mid = sum / static_cast<double>(carried.size());
  double radius = 0.0;
  for (const QPointF& point : carried) {
    radius = std::max(radius, Length(point - mid));
  }
  return std::max(radius, 2.0 * cell);
}

// How far an offset of length may stay: the same up to half the reach,
// then bending smoothly toward the reach.
double SoftLimited(double length, double reach) {
  assert(reach > 0.0);
  assert(length >= 0.0);
  const double knee = reach / 2.0;
  const bool is_past = length > knee;
  return is_past ? knee + knee * std::tanh((length - knee) / knee) : length;
}

// Bends every swing that is long toward the reach, and stops a node
// going further out: it keeps the body's speed and the part of its own
// along the edge.
void HoldInReach(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(static_cast<int>(carried.size()) == piece->grid.PointCount());
  const double reach = ReachOf(carried, piece->cell);
  for (size_t i = 0; i < piece->at.size(); ++i) {
    const QPointF anchor = carried[static_cast<size_t>(piece->points[i])];
    const QPointF off = piece->at[i] - anchor;
    const double length = Length(off);
    const bool is_out = length > reach / 2.0;
    if (!is_out) {
      continue;
    }
    const QPointF way = off / length;
    QPointF own = piece->speed[i] - piece->body_speed[i];
    const double outward = own.x() * way.x() + own.y() * way.y();
    own -= way * std::max(0.0, outward);
    piece->at[i] = anchor + way * SoftLimited(length, reach);
    piece->speed[i] = piece->body_speed[i] + own;
  }
}

}  // namespace

void StepJelly(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(static_cast<int>(carried.size()) == piece->grid.PointCount());
  const size_t count = piece->points.size();
  assert(piece->at.size() == count && piece->speed.size() == count &&
         piece->settings.size() == count && piece->body_speed.size() == count);
  const std::vector<QPointF> before = piece->at;
  const std::vector<int> node_of = NodeOfPoints(*piece);
  const std::vector<double> weights = WeightsOf(*piece);
  const Motion motion = MotionOf(carried, *piece);
  for (const Clump& clump : ClumpsOf(node_of, carried, *piece)) {
    const Swing next = NextSwing(clump, weights, motion, *piece);
    for (size_t k = 0; k < clump.nodes.size(); ++k) {
      const size_t i = clump.nodes[k];
      const QPointF anchor = carried[static_cast<size_t>(piece->points[i])];
      piece->at[i] = anchor + SwingAt(next, clump.spots[k]) * weights[i];
    }
  }
  KeepApart(node_of, carried, piece);
  FinishStep(carried, before, false, piece);
  HoldInReach(carried, piece);
}

}  // namespace snapper
