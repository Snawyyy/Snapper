#include "anim/cloth.h"

#include <QPointF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

namespace snapper {
namespace {

// Rounds of tie fixing per frame: more is stiffer cloth, never a loop
// without an end.
constexpr int kTieRounds = 40;
// Pixels per frame per frame a gravity of 1 pulls down.
constexpr double kGravityPull = 8.0;
// The pull back to the carried place a spring of 1 gives, per frame.
constexpr double kSpringPull = 0.6;
// Rounds of pushing out at the end of a frame, so the pushes of
// neighbouring cloth settle together.
constexpr int kSettlePasses = 6;
// A middle this close to an outline counts as on it, not across it.
constexpr double kAcrossSlack = 0.1;
// How far past its drawn length a pin tie lets a node go, as a share of
// that length at stretch 0 and at stretch 1. A pin is the body, which
// limits a node, never freezes it: a lone node between pins must swing.
constexpr double kPinGiveTight = 0.5;
constexpr double kPinGiveLoose = 1.5;
// How much of a tie's overshoot one round fixes at stretch 1.
constexpr double kRubberStiffness = 0.02;

// Two things the cloth ties together: a node and another node, or a
// node and a pin (other < 0, held at pin).
struct Tie final {
  size_t node = 0;
  int other = -1;
  QPointF pin;
  double rest = 0.0;
};

double Length(QPointF v) { return std::hypot(v.x(), v.y()); }

// The node of grid point, or -1.
int NodeAt(const ClothPiece& piece, int point) {
  assert(point >= 0);
  assert(piece.points.size() <= static_cast<size_t>(kMaxWarpPoints));
  const auto found =
      std::find(piece.points.begin(), piece.points.end(), point);
  const bool is_node = found != piece.points.end();
  return is_node ? static_cast<int>(found - piece.points.begin()) : -1;
}

// The farthest node may be from a pin that is rest away when drawn.
double PinReach(const ClothPiece& piece, int node, double rest) {
  assert(node >= 0 && static_cast<size_t>(node) < piece.settings.size());
  assert(rest >= 0.0);
  const double stretch = piece.settings[static_cast<size_t>(node)].stretch;
  return rest * (1.0 + kPinGiveTight +
                 (kPinGiveLoose - kPinGiveTight) * stretch);
}

// A tie across one grid edge from point to next, if a node is on it.
void AddTie(const ClothPiece& piece, const std::vector<QPointF>& carried,
            int point, int next, std::vector<Tie>* ties) {
  assert(ties != nullptr);
  assert(static_cast<size_t>(next) < carried.size());
  const int a = NodeAt(piece, point);
  const int b = NodeAt(piece, next);
  const bool is_cloth = a >= 0 || b >= 0;
  if (!is_cloth) {
    return;
  }
  const QPointF from = carried[static_cast<size_t>(point)];
  const QPointF to = carried[static_cast<size_t>(next)];
  const double rest = Length(to - from);
  const bool is_pair = a >= 0 && b >= 0;
  const bool is_start_node = a >= 0;
  if (is_pair) {
    ties->push_back({static_cast<size_t>(a), b, QPointF(), rest});
  } else if (is_start_node) {
    ties->push_back({static_cast<size_t>(a), -1, to, PinReach(piece, a, rest)});
  } else {
    ties->push_back(
        {static_cast<size_t>(b), -1, from, PinReach(piece, b, rest)});
  }
}

// Every tie of the grid: each point to the one at its right and below.
std::vector<Tie> TiesOf(const ClothPiece& piece,
                        const std::vector<QPointF>& carried) {
  assert(static_cast<int>(carried.size()) == piece.grid.PointCount());
  assert(piece.points.size() == piece.at.size());
  std::vector<Tie> ties;
  const int across = piece.grid.columns + 1;
  for (int row = 0; row <= piece.grid.rows; ++row) {
    for (int column = 0; column <= piece.grid.columns; ++column) {
      const int point = row * across + column;
      const bool has_right = column < piece.grid.columns;
      const bool has_below = row < piece.grid.rows;
      if (has_right) {
        AddTie(piece, carried, point, point + 1, &ties);
      }
      if (has_below) {
        AddTie(piece, carried, point, point + across, &ties);
      }
    }
  }
  assert(ties.size() <= static_cast<size_t>(2 * kMaxWarpPoints));
  return ties;
}

// Pulls the two ends of a tie in when they are farther apart than
// rest; a slack tie does nothing, as cloth buckles rather than pushes.
void Fix(const Tie& tie, const ClothPiece& piece, std::vector<QPointF>* at) {
  assert(at != nullptr);
  assert(tie.node < at->size());
  const double stretch = piece.settings[tie.node].stretch;
  const double stiffness =
      kRubberStiffness + (1.0 - kRubberStiffness) * (1.0 - stretch);
  const bool is_pair = tie.other >= 0;
  const QPointF end =
      is_pair ? (*at)[static_cast<size_t>(tie.other)] : tie.pin;
  const QPointF gap = end - (*at)[tie.node];
  const double length = Length(gap);
  const bool is_taut = length > tie.rest && length > 1e-9;
  if (!is_taut) {
    return;
  }
  const QPointF pull = gap * ((length - tie.rest) / length * stiffness);
  if (is_pair) {
    (*at)[tie.node] += pull * 0.5;
    (*at)[static_cast<size_t>(tie.other)] -= pull * 0.5;
  } else {
    (*at)[tie.node] += pull;
  }
}

// Keeps a node within kMaxClothDrift of the place the body carries it.
QPointF Tethered(QPointF at, QPointF anchor) {
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  assert(std::isfinite(anchor.x()) && std::isfinite(anchor.y()));
  const QPointF off = at - anchor;
  const double length = Length(off);
  const bool is_far = length > kMaxClothDrift;
  return is_far ? anchor + off * (kMaxClothDrift / length) : at;
}

// Pushes every node that is behind an edge out to it.
void PushNodes(const std::vector<ColliderEdge>& edges,
               std::vector<QPointF>* at) {
  assert(at != nullptr);
  assert(edges.size() <= kMaxColliderEdges);
  const bool has_edges = !edges.empty();
  if (!has_edges) {
    return;
  }
  for (QPointF& point : *at) {
    point = PushedOut(edges, point);
  }
}

// Pushes out the cloth between two nodes that an outline has come
// across since the last frame: its middle is tested, and both nodes
// move by what it needs. before holds where the nodes stood then.
void PushCloth(const ClothPiece& piece, const std::vector<Tie>& ties,
               const std::vector<QPointF>& before,
               std::vector<QPointF>* at) {
  assert(at != nullptr);
  assert(before.size() == at->size());
  const bool has_edges = !piece.edges.empty();
  if (!has_edges) {
    return;
  }
  for (const Tie& tie : ties) {
    const bool is_pair = tie.other >= 0;
    if (!is_pair) {
      continue;
    }
    const size_t other = static_cast<size_t>(tie.other);
    const QPointF was = (before[tie.node] + before[other]) / 2.0;
    const bool was_across =
        Length(PushedOutDeep(piece.edges_before, was) - was) >
        kAcrossSlack;
    const QPointF mid = ((*at)[tie.node] + (*at)[other]) / 2.0;
    const QPointF moved = PushedOutDeep(piece.edges, mid) - mid;
    const bool is_new = !was_across && Length(moved) > kAcrossSlack;
    if (is_new) {
      (*at)[tie.node] += moved;
      (*at)[other] += moved;
    }
  }
}

}  // namespace

ClothSettings ClothSettingsOf(const DragNode& node) {
  assert(node.physics);
  assert(std::isfinite(node.stretch) && std::isfinite(node.spring));
  return {node.stretch, node.inertia, node.air, node.gravity, node.spring};
}

void RestCloth(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(static_cast<int>(carried.size()) == piece->grid.PointCount());
  for (size_t i = 0; i < piece->points.size(); ++i) {
    piece->at[i] = carried[static_cast<size_t>(piece->points[i])];
    piece->speed[i] = QPointF();
    piece->at[i] = PushedOut(piece->edges, piece->at[i]);
  }
}

void StepCloth(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(static_cast<int>(carried.size()) == piece->grid.PointCount());
  const size_t count = piece->points.size();
  assert(piece->at.size() == count && piece->speed.size() == count);
  const std::vector<QPointF> before = piece->at;
  for (size_t i = 0; i < count; ++i) {
    const ClothSettings& s = piece->settings[i];
    const QPointF anchor = carried[static_cast<size_t>(piece->points[i])];
    // Never below half: the default must swing, not creep.
    const double keep = 0.5 + 0.5 * s.inertia * (1.0 - s.air);
    const QPointF down(0.0, s.gravity * kGravityPull);
    const QPointF back = (anchor - piece->at[i]) * (s.spring * kSpringPull);
    piece->at[i] += piece->speed[i] * keep + down + back;
  }
  const std::vector<Tie> ties = TiesOf(*piece, carried);
  for (int round = 0; round < kTieRounds; ++round) {
    for (const Tie& tie : ties) {
      Fix(tie, *piece, &piece->at);
    }
    PushNodes(piece->edges, &piece->at);
    PushCloth(*piece, ties, before, &piece->at);
  }
  for (size_t i = 0; i < count; ++i) {
    const QPointF anchor = carried[static_cast<size_t>(piece->points[i])];
    piece->at[i] = Tethered(piece->at[i], anchor);
  }
  for (int pass = 0; pass < kSettlePasses; ++pass) {
    PushCloth(*piece, ties, before, &piece->at);
    PushNodes(piece->edges, &piece->at);
  }
  for (size_t i = 0; i < count; ++i) {
    piece->speed[i] = piece->at[i] - before[i];
  }
}

}  // namespace snapper
