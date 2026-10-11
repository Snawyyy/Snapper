#include "anim/guides.h"

#include <QPointF>
#include <QRectF>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <optional>
#include <vector>

namespace snapper {
namespace {

// The edge, middle and edge of a span.
std::array<double, 3> Marks(double low, double size) {
  assert(std::isfinite(low) && std::isfinite(size));
  assert(size >= 0.0);
  return {low, low + size / 2.0, low + size};
}

// The closest catch along one axis: how far to move, and where the line
// goes.
struct Catch final {
  double shift = 0.0;
  double at = 0.0;
};

bool IsSkipped(const std::vector<GuideLine>& skip, bool is_vertical,
               double at) {
  assert(std::isfinite(at));
  assert(skip.size() < 1000000);
  return std::any_of(skip.begin(), skip.end(), [&](const GuideLine& line) {
    return line.is_vertical == is_vertical && std::abs(line.at - at) < 0.5;
  });
}

std::optional<Catch> Closest(const std::array<double, 3>& mine,
                             const std::vector<std::array<double, 3>>& theirs,
                             double reach, bool is_vertical,
                             const std::vector<GuideLine>& skip) {
  assert(reach >= 0.0);
  assert(theirs.size() < 1000000);
  std::optional<Catch> best;
  for (const auto& target : theirs) {
    for (const double mark : mine) {
      for (const double line : target) {
        const double gap = line - mark;
        const bool is_nearer =
            std::abs(gap) <= reach && !IsSkipped(skip, is_vertical, line) &&
            (!best.has_value() || std::abs(gap) < std::abs(best->shift));
        if (is_nearer) {
          best = Catch{gap, line};
        }
      }
    }
  }
  return best;
}

}  // namespace

std::vector<GuideLine> AlignedGuides(const QRectF& moving,
                                     const std::vector<QRectF>& targets) {
  assert(targets.size() < 1000000);
  assert(moving.width() >= 0.0 && moving.height() >= 0.0);
  std::vector<GuideLine> aligned;
  for (const QRectF& target : targets) {
    for (const double mark : Marks(moving.left(), moving.width())) {
      for (const double line : Marks(target.left(), target.width())) {
        const bool is_lined = std::abs(line - mark) < 0.5;
        if (is_lined) {
          aligned.push_back({true, line});
        }
      }
    }
    for (const double mark : Marks(moving.top(), moving.height())) {
      for (const double line : Marks(target.top(), target.height())) {
        const bool is_lined = std::abs(line - mark) < 0.5;
        if (is_lined) {
          aligned.push_back({false, line});
        }
      }
    }
  }
  return aligned;
}

GuideSnap SnapToGuides(const QRectF& moving,
                       const std::vector<QRectF>& targets, double reach,
                       const std::vector<GuideLine>& skip) {
  assert(reach >= 0.0);
  assert(targets.size() < 1000000);
  std::vector<std::array<double, 3>> across;
  std::vector<std::array<double, 3>> down;
  for (const QRectF& target : targets) {
    across.push_back(Marks(target.left(), target.width()));
    down.push_back(Marks(target.top(), target.height()));
  }
  GuideSnap snap;
  const auto x = Closest(Marks(moving.left(), moving.width()), across, reach,
                         true, skip);
  const auto y = Closest(Marks(moving.top(), moving.height()), down, reach,
                         false, skip);
  const bool has_x = x.has_value();
  const bool has_y = y.has_value();
  if (has_x) {
    snap.shift.setX(x->shift);
    snap.lines.push_back({true, x->at});
  }
  if (has_y) {
    snap.shift.setY(y->shift);
    snap.lines.push_back({false, y->at});
  }
  return snap;
}

}  // namespace snapper
