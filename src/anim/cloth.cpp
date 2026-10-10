#include "anim/cloth.h"

#include <QPointF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "anim/grid_fold.h"
#include "anim/warp_shape.h"

namespace snapper {
namespace {

// Rounds of tie fixing per frame: more is stiffer cloth, never a loop
// without an end.
constexpr int kTieRounds = 24;
// How much longer than drawn a tie may get at stretch 1.
constexpr double kMaxStretch = 1.0;
// How hard a tie pulls back, per frame, on what it is stretched past
// its drawn length at stretch 0; less as stretch goes up, none at 1.
// So stretch is felt through its whole range, not only at the limit.
constexpr double kFirmness = 0.2;
// The pull back to the drawn place a bounce of 1 gives, per frame.
constexpr double kSpringPull = 0.3;
// The share of its motion a node keeps each frame with no air
// resistance (a little is always lost, so it settles), and what air
// resistance 1 takes off that.
constexpr double kKeep = 0.98;
constexpr double kAirDrag = 0.3;
// The share of a grid cell a gravity of 1 pulls per frame per frame,
// per unit of turn away from upright (2 when upside down): sized by the
// cell, so a small drawing hangs as a big one does.
constexpr double kGravityPull = 0.02;
// The share of a collider's push a node keeps as speed: the rest is
// lost in the hit, so cloth rides on a leg instead of flying off it.
constexpr double kPushKick = 0.3;
// The shortest a tie may get, as a share of its drawn length: cloth
// buckles, but never folds flat onto itself.
constexpr double kLeastLength = 0.5;
// The shortest a tie across two mesh triangles may get: it holds the
// sheet open so a fine mesh does not crumple.
constexpr double kLeastBend = 0.9;
// Rounds of turning back mesh triangles the cloth turned over.
constexpr int kFlipRounds = 4;

// Two things the cloth ties together: a node and another node, or a
// node and a pin (other < 0, held at pin).
struct Tie final {
  size_t node = 0;
  int other = -1;
  QPointF pin;
  double rest = 0.0;
  double shortest = 0.0;
  double longest = 0.0;
  // How hard it pulls back once stretched past rest, per frame.
  double firmness = 0.0;
};

double Length(QPointF v) { return std::hypot(v.x(), v.y()); }

// A sum of two finite numbers far below overflow is finite.
bool IsFinite(QPointF p) { return std::isfinite(p.x() + p.y()); }

// The node of grid point, or -1.
int NodeAt(const ClothPiece& piece, int point) {
  assert(point >= 0);
  assert(piece.points.size() <= static_cast<size_t>(kMaxWarpPoints));
  const auto found =
      std::find(piece.points.begin(), piece.points.end(), point);
  const bool is_node = found != piece.points.end();
  return is_node ? static_cast<int>(found - piece.points.begin()) : -1;
}

// How long a tie of drawn length rest at node may get.
double Longest(const ClothPiece& piece, int node, double rest) {
  assert(node >= 0 && static_cast<size_t>(node) < piece.settings.size());
  assert(rest >= 0.0);
  const double stretch = piece.settings[static_cast<size_t>(node)].stretch;
  return rest * (1.0 + kMaxStretch * stretch);
}

// A tie across one grid edge from point to next, if a node is on it,
// never shorter than least of its drawn length.
void AddTie(const ClothPiece& piece, const std::vector<QPointF>& carried,
            int point, int next, double least, std::vector<Tie>* ties) {
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
  const int node = is_start_node ? a : b;
  const int other = is_pair ? b : -1;
  const QPointF pin = is_start_node ? to : from;
  const double longest = is_pair ? std::min(Longest(piece, a, rest),
                                            Longest(piece, b, rest))
                                 : Longest(piece, node, rest);
  const double give = 1.0 - piece.settings[static_cast<size_t>(node)].stretch;
  ties->push_back({static_cast<size_t>(node), other, is_pair ? QPointF() : pin,
                   rest, rest * least, longest,
                   kFirmness * give * give});
}

// Every tie of the grid: one across each drawn side with a node on it.
std::vector<Tie> TiesOf(const ClothPiece& piece,
                        const std::vector<QPointF>& carried) {
  assert(static_cast<int>(carried.size()) == piece.grid.PointCount());
  assert(piece.points.size() == piece.at.size());
  std::vector<Tie> ties;
  for (const MeshEdge& edge : WarpEdges(piece.grid)) {
    AddTie(piece, carried, edge.first, edge.second, kLeastLength, &ties);
  }
  // A mesh's triangles alone let cloth crease flat; ties across each
  // two of them keep it a sheet.
  for (const MeshEdge& bend : WarpBends(piece.grid)) {
    AddTie(piece, carried, bend.first, bend.second, kLeastBend, &ties);
  }
  assert(ties.size() <= static_cast<size_t>(6 * kMaxWarpPoints));
  return ties;
}

// Pulls the two ends of a tie in when they are farther apart than it
// may get, and apart when they are closer than it may get; between the
// two a tie does nothing, as cloth buckles rather than pushes.
void Fix(const Tie& tie, std::vector<QPointF>* at) {
  assert(at != nullptr);
  assert(tie.node < at->size());
  const bool is_pair = tie.other >= 0;
  const QPointF end =
      is_pair ? (*at)[static_cast<size_t>(tie.other)] : tie.pin;
  const QPointF gap = end - (*at)[tie.node];
  const double length = Length(gap);
  const double wanted = std::clamp(length, tie.shortest, tie.longest);
  const bool is_off = std::abs(wanted - length) > 1e-9 && length > 1e-9;
  if (!is_off) {
    return;
  }
  const QPointF pull = gap * ((length - wanted) / length);
  if (is_pair) {
    (*at)[tie.node] += pull * 0.5;
    (*at)[static_cast<size_t>(tie.other)] -= pull * 0.5;
  } else {
    (*at)[tie.node] += pull;
  }
}

// Pulls the ends of a tie stretched past its drawn length part of the
// way back, as a spring would over one frame.
void Firm(const Tie& tie, std::vector<QPointF>* at) {
  assert(at != nullptr);
  assert(tie.node < at->size());
  const bool is_pair = tie.other >= 0;
  const QPointF end =
      is_pair ? (*at)[static_cast<size_t>(tie.other)] : tie.pin;
  const QPointF gap = end - (*at)[tie.node];
  const double length = Length(gap);
  const bool is_stretched = length > tie.rest && length > 1e-9;
  if (!is_stretched) {
    return;
  }
  const QPointF pull = gap * ((length - tie.rest) / length * tie.firmness);
  if (is_pair) {
    (*at)[tie.node] += pull * 0.5;
    (*at)[static_cast<size_t>(tie.other)] -= pull * 0.5;
  } else {
    (*at)[tie.node] += pull;
  }
}

// Keeps a node within kMaxClothDrift of the place the body carries it,
// and puts it there if the numbers ever went bad.
QPointF Tethered(QPointF at, QPointF anchor) {
  assert(IsFinite(anchor));
  assert(kMaxClothDrift > 0.0);
  const bool is_bad = !IsFinite(at);
  if (is_bad) {
    return anchor;
  }
  const QPointF off = at - anchor;
  const double length = Length(off);
  const bool is_far = length > kMaxClothDrift;
  return is_far ? anchor + off * (kMaxClothDrift / length) : at;
}

// Pushes every node a collider has come into back out, toward its own
// place (carried, which is already out): so it lands on the side of
// the outline it belongs on however deep a fast move took it.
void PushNodes(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(piece->at.size() == piece->points.size());
  for (const PushField& field : piece->fields) {
    for (size_t i = 0; i < piece->at.size(); ++i) {
      const size_t point = static_cast<size_t>(piece->points[i]);
      const QPointF at = piece->at[i];
      const bool is_in =
          PushedFromRest(field.edges, field.ghosts[point], at) != at;
      if (is_in) {
        piece->at[i] = OutToward(field.edges, at, carried[point]);
      }
    }
  }
}

// Keeps the cloth from turning over on itself: the grid with the nodes
// where they are must not fold against the grid as carried.
void UnfoldNodes(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(static_cast<int>(carried.size()) == piece->grid.PointCount());
  std::vector<QPointF> grid = carried;
  for (size_t i = 0; i < piece->at.size(); ++i) {
    grid[static_cast<size_t>(piece->points[i])] = piece->at[i];
  }
  Unfold(piece->grid, carried, &grid);
  for (size_t i = 0; i < piece->at.size(); ++i) {
    piece->at[i] = grid[static_cast<size_t>(piece->points[i])];
  }
}

// q mirrored across the line through u and v.
QPointF Mirrored(QPointF q, QPointF u, QPointF v) {
  assert(IsFinite(q) && IsFinite(u));
  assert(IsFinite(v));
  const QPointF d = v - u;
  const double length2 = d.x() * d.x() + d.y() * d.y();
  const bool is_point = length2 < 1e-12;
  if (is_point) {
    return q;
  }
  const QPointF foot =
      u + d * (((q - u).x() * d.x() + (q - u).y() * d.y()) / length2);
  return foot * 2.0 - q;
}

// Turns back every mesh triangle the cloth turned over against how the
// body carries it: a node corner is mirrored across the side facing
// it, which keeps every tie to that side as long as it was, so the
// sheet stays a sheet instead of crumpling. A grid keeps its own rules.
void UnflipNodes(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(static_cast<int>(carried.size()) == piece->grid.PointCount());
  const bool is_mesh = piece->grid.IsMesh();
  if (!is_mesh) {
    return;
  }
  std::vector<QPointF> at = carried;
  std::vector<int> node_of(carried.size(), -1);
  for (size_t i = 0; i < piece->at.size(); ++i) {
    at[static_cast<size_t>(piece->points[i])] = piece->at[i];
    node_of[static_cast<size_t>(piece->points[i])] = static_cast<int>(i);
  }
  const auto turn = [](QPointF a, QPointF b, QPointF c) {
    return (b.x() - a.x()) * (c.y() - a.y()) -
           (b.y() - a.y()) * (c.x() - a.x());
  };
  for (int round = 0; round < kFlipRounds; ++round) {
    bool is_any = false;
    for (const MeshTriangle& t : piece->grid.mesh->triangles) {
      const auto p = [&t](size_t k) { return static_cast<size_t>(t[k]); };
      const double was = turn(carried[p(0)], carried[p(1)], carried[p(2)]);
      const double now = turn(at[p(0)], at[p(1)], at[p(2)]);
      const bool is_flipped = was * now < 0.0;
      for (size_t k = 0; k < 3 && is_flipped; ++k) {
        const bool is_node = node_of[p(k)] >= 0;
        if (is_node) {
          at[p(k)] = Mirrored(at[p(k)], at[p((k + 1) % 3)],
                              at[p((k + 2) % 3)]);
          is_any = true;
          break;
        }
      }
    }
    if (!is_any) {
      break;
    }
  }
  for (size_t i = 0; i < piece->at.size(); ++i) {
    piece->at[i] = at[static_cast<size_t>(piece->points[i])];
  }
}

// Moves node i one frame by its own motion and the pulls on it.
void Move(const std::vector<QPointF>& carried, size_t i, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(i < piece->at.size());
  const ClothSettings& s = piece->settings[i];
  const QPointF anchor = carried[static_cast<size_t>(piece->points[i])];
  const QPointF body = piece->body_speed[i];
  // Its place against the body before this frame's carry.
  const QPointF off = piece->at[i] - (anchor - body);
  // Its own motion on top of the body's: inertia keeps it, air slows it.
  QPointF own = (piece->speed[i] - body) * s.inertia;
  own *= std::max(0.0, kKeep - kAirDrag * s.air);
  const QPointF turned = QPointF(0.0, 1.0) - piece->drawn_down;
  own += turned * (s.gravity * kGravityPull * piece->cell) -
         off * (s.spring * kSpringPull);
  piece->at[i] = anchor + off + own;
}

}  // namespace

ClothSettings ClothSettingsOf(const DragNode& node) {
  assert(node.physics);
  assert(std::isfinite(node.stretch) && std::isfinite(node.spring));
  const auto unit = [](double v) {
    return std::isfinite(v) ? std::clamp(v, 0.0, 1.0) : 0.0;
  };
  return {unit(node.stretch), unit(node.inertia), unit(node.air),
          unit(node.gravity), unit(node.spring)};
}

void RestCloth(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(static_cast<int>(carried.size()) == piece->grid.PointCount());
  for (size_t i = 0; i < piece->points.size(); ++i) {
    piece->at[i] = carried[static_cast<size_t>(piece->points[i])];
    piece->speed[i] = QPointF();
  }
  PushNodes(carried, piece);
}

void StepCloth(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(static_cast<int>(carried.size()) == piece->grid.PointCount());
  const size_t count = piece->points.size();
  assert(piece->at.size() == count && piece->speed.size() == count &&
         piece->body_speed.size() == count);
  const std::vector<QPointF> before = piece->at;
  for (size_t i = 0; i < count; ++i) {
    Move(carried, i, piece);
  }
  const std::vector<Tie> ties = TiesOf(*piece, carried);
  for (const Tie& tie : ties) {
    Firm(tie, &piece->at);
  }
  for (int round = 0; round < kTieRounds; ++round) {
    for (const Tie& tie : ties) {
      Fix(tie, &piece->at);
    }
  }
  UnflipNodes(carried, piece);
  UnfoldNodes(carried, piece);
  const std::vector<QPointF> unpushed = piece->at;
  PushNodes(carried, piece);
  for (size_t i = 0; i < count; ++i) {
    const QPointF anchor = carried[static_cast<size_t>(piece->points[i])];
    piece->at[i] = Tethered(piece->at[i], anchor);
    const QPointF push = piece->at[i] - unpushed[i];
    const bool was_bad = !IsFinite(before[i]) || !IsFinite(unpushed[i]);
    piece->speed[i] = was_bad ? QPointF()
                              : unpushed[i] - before[i] + push * kPushKick;
  }
}

}  // namespace snapper
