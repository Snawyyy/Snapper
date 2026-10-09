#include "anim/outline_trace.h"

#include <QPoint>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace snapper {
namespace {

// Heading 0 east, 1 south, 2 west, 3 north (y down).
constexpr std::array<QPoint, 4> kStep = {
    QPoint(1, 0), QPoint(0, 1), QPoint(-1, 0), QPoint(0, -1)};
// The pixels ahead of a vertex to the left and right of each heading.
constexpr std::array<QPoint, 4> kAheadLeft = {
    QPoint(0, -1), QPoint(0, 0), QPoint(-1, 0), QPoint(-1, -1)};
constexpr std::array<QPoint, 4> kAheadRight = {
    QPoint(0, 0), QPoint(-1, 0), QPoint(-1, -1), QPoint(0, -1)};

// Whether a pixel is visible; off the image none is.
class Mask final {
 public:
  Mask(const QImage& image, int threshold)
      : image_(image.convertToFormat(QImage::Format_ARGB32)),
        threshold_(threshold) {
    assert(threshold >= 0 && threshold <= 255);
    assert(image_.format() == QImage::Format_ARGB32 || image_.isNull());
  }
  int width() const { return image_.width(); }
  int height() const { return image_.height(); }
  bool IsVisible(QPoint at) const {
    assert(threshold_ >= 0);
    assert(!image_.isNull());
    const bool is_inside = at.x() >= 0 && at.y() >= 0 &&
                           at.x() < image_.width() && at.y() < image_.height();
    return is_inside && qAlpha(image_.pixel(at)) >= threshold_;
  }

 private:
  QImage image_;
  int threshold_;
};

// The top left corner of the first visible pixel from the top left,
// or (-1, -1).
QPoint FirstVisible(const Mask& mask) {
  assert(mask.width() >= 0 && mask.height() >= 0);
  assert(mask.width() <= 1 << 20);
  for (int y = 0; y < mask.height(); ++y) {
    for (int x = 0; x < mask.width(); ++x) {
      const bool is_visible = mask.IsVisible(QPoint(x, y));
      if (is_visible) {
        return QPoint(x, y);
      }
    }
  }
  return QPoint(-1, -1);
}

// Follows the edges of the shape that starts at corner start, with the
// shape on the right, and gives the corners where the outline turns.
std::vector<QPointF> FollowEdges(const Mask& mask, QPoint start) {
  assert(start.x() >= 0 && start.y() >= 0);
  assert(mask.IsVisible(start));
  std::vector<QPointF> turns;
  QPoint at = start;
  int heading = 0;
  const std::int64_t limit =
      4 * static_cast<std::int64_t>(mask.width()) * mask.height() + 16;
  bool is_closed = false;
  for (std::int64_t step = 0; step < limit && !is_closed; ++step) {
    at += kStep[static_cast<size_t>(heading)];
    const auto h = static_cast<size_t>(heading);
    const bool is_left = mask.IsVisible(at + kAheadLeft[h]);
    const bool is_right = mask.IsVisible(at + kAheadRight[h]);
    const int next = is_left ? (heading + 3) % 4
                     : is_right ? heading
                                : (heading + 1) % 4;
    const bool is_turn = next != heading;
    if (is_turn) {
      turns.emplace_back(at);
    }
    heading = next;
    is_closed = at == start && heading == 0;
  }
  return turns;
}

double Distance(QPointF a, QPointF b) {
  assert(std::isfinite(a.x()) && std::isfinite(a.y()));
  assert(std::isfinite(b.x()) && std::isfinite(b.y()));
  return std::hypot(a.x() - b.x(), a.y() - b.y());
}

// How far p is from the segment a to b.
double FromSegment(QPointF p, QPointF a, QPointF b) {
  assert(std::isfinite(p.x()) && std::isfinite(a.x()));
  assert(std::isfinite(b.x()));
  const QPointF ab = b - a;
  const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
  const bool is_point = len2 < 1e-12;
  const QPointF ap = p - a;
  const double along = (ap.x() * ab.x() + ap.y() * ab.y()) / len2;
  const double t = is_point ? 0.0 : std::clamp(along, 0.0, 1.0);
  return Distance(p, a + ab * t);
}

// Douglas-Peucker on an open chain, with a stack in place of recursion.
std::vector<QPointF> SimplifyChain(const std::vector<QPointF>& chain,
                                   double tolerance) {
  assert(!chain.empty());
  assert(tolerance >= 0.0);
  std::vector<bool> keep(chain.size(), false);
  keep.front() = true;
  keep.back() = true;
  std::vector<std::pair<size_t, size_t>> todo{{0, chain.size() - 1}};
  for (size_t round = 0; round < 2 * chain.size() && !todo.empty(); ++round) {
    const auto [lo, hi] = todo.back();
    todo.pop_back();
    double worst = tolerance;
    size_t at = 0;
    for (size_t i = lo + 1; i < hi; ++i) {
      const double away = FromSegment(chain[i], chain[lo], chain[hi]);
      const bool is_worse = away > worst;
      if (is_worse) {
        worst = away;
        at = i;
      }
    }
    const bool is_split = at != 0;
    if (is_split) {
      keep[at] = true;
      todo.emplace_back(lo, at);
      todo.emplace_back(at, hi);
    }
  }
  std::vector<QPointF> kept;
  for (size_t i = 0; i < chain.size(); ++i) {
    const bool is_kept = keep[i];
    if (is_kept) {
      kept.push_back(chain[i]);
    }
  }
  return kept;
}

// The closed ring with its staircases straightened: it is cut at its
// first corner and the one farthest from it, and each half simplified.
std::vector<QPointF> SimplifyRing(const std::vector<QPointF>& ring,
                                  double tolerance) {
  assert(!ring.empty());
  assert(tolerance >= 0.0);
  size_t far = 0;
  for (size_t i = 1; i < ring.size(); ++i) {
    const bool is_farther = Distance(ring[i], ring[0]) >
                            Distance(ring[far], ring[0]);
    if (is_farther) {
      far = i;
    }
  }
  const bool is_flat = far == 0;
  if (is_flat) {
    return ring;
  }
  const auto mid = static_cast<std::ptrdiff_t>(far);
  const std::vector<QPointF> first(ring.begin(), ring.begin() + mid + 1);
  std::vector<QPointF> second(ring.begin() + mid, ring.end());
  second.push_back(ring[0]);
  std::vector<QPointF> joined = SimplifyChain(first, tolerance);
  const std::vector<QPointF> back = SimplifyChain(second, tolerance);
  joined.insert(joined.end(), back.begin() + 1, back.end() - 1);
  return joined;
}

// count dots spaced evenly along the closed ring, the first at its
// first corner.
std::vector<QPointF> Resample(const std::vector<QPointF>& ring, int count) {
  assert(!ring.empty());
  assert(count > 0);
  double total = 0.0;
  for (size_t i = 0; i < ring.size(); ++i) {
    total += Distance(ring[i], ring[(i + 1) % ring.size()]);
  }
  std::vector<QPointF> dots;
  size_t edge = 0;
  double walked = 0.0;
  for (int k = 0; k < count; ++k) {
    const double target = total * k / count;
    for (size_t guard = 0; guard < ring.size(); ++guard) {
      const QPointF from = ring[edge];
      const QPointF to = ring[(edge + 1) % ring.size()];
      const double length = Distance(from, to);
      const bool is_here = target <= walked + length + 1e-9;
      if (is_here) {
        const double t = length > 1e-12 ? (target - walked) / length : 0.0;
        dots.push_back(from + (to - from) * std::clamp(t, 0.0, 1.0));
        break;
      }
      walked += length;
      edge = (edge + 1) % ring.size();
    }
  }
  return dots;
}

}  // namespace

std::vector<QPointF> TraceOutline(const QImage& image, int alpha_threshold,
                                  double spacing, int max_dots) {
  assert(spacing > 0.0);
  assert(max_dots >= 3);
  const bool is_image = !image.isNull();
  if (!is_image) {
    return {};
  }
  const Mask mask(image, std::clamp(alpha_threshold, 0, 255));
  const QPoint start = FirstVisible(mask);
  const bool is_found = start.x() >= 0;
  if (!is_found) {
    return {};
  }
  const std::vector<QPointF> ring =
      SimplifyRing(FollowEdges(mask, start), 0.75);
  double total = 0.0;
  for (size_t i = 0; i < ring.size(); ++i) {
    total += Distance(ring[i], ring[(i + 1) % ring.size()]);
  }
  const int count = std::clamp(static_cast<int>(std::lround(total / spacing)),
                               3, max_dots);
  return Resample(ring, count);
}

}  // namespace snapper
