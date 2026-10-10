#include "anim/cloth.h"

#include <QPointF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "anim/cloth_parts.h"
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
// The shortest a tie may get, as a share of its drawn length: cloth
// buckles, but never folds flat onto itself.
constexpr double kLeastLength = 0.5;
// The shortest a tie across two mesh triangles may get: it holds the
// sheet open so a fine mesh does not crumple.
constexpr double kLeastBend = 0.9;

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

}  // namespace

ClothSettings ClothSettingsOf(const DragNode& node) {
  assert(node.kind != NodeKind::kDrag);
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
    Move(carried, i, piece->settings[i].spring * kSpringPull, piece);
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
  FinishStep(carried, before, piece);
}

}  // namespace snapper
