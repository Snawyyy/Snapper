#include "anim/stroke_fit.h"

#include <QLineF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

namespace snapper {
namespace {

// Strokes past this many points are thinned first, so the fit stays
// quick whatever the mouse sends.
constexpr size_t kMaxStrokePoints = 20000;

double GapToLine(QPointF at, QPointF from, QPointF to) {
  assert(std::isfinite(at.x()));
  assert(std::isfinite(from.x()) && std::isfinite(to.x()));
  const QPointF along = to - from;
  const double length2 = QPointF::dotProduct(along, along);
  if (length2 <= 1e-12) {
    return QLineF(at, from).length();
  }
  const double t =
      std::clamp(QPointF::dotProduct(at - from, along) / length2, 0.0, 1.0);
  return QLineF(at, from + t * along).length();
}

void Keep(std::span<const QPointF> stroke, size_t first, size_t last,
          double tolerance, std::vector<bool>* kept) {
  assert(kept != nullptr);
  assert(last < stroke.size());
  std::vector<std::pair<size_t, size_t>> stack = {{first, last}};
  while (!stack.empty()) {
    const auto [from, to] = stack.back();
    stack.pop_back();
    double worst = 0.0;
    size_t at = from;
    for (size_t i = from + 1; i < to; ++i) {
      const double gap = GapToLine(stroke[i], stroke[from], stroke[to]);
      if (gap > worst) {
        worst = gap;
        at = i;
      }
    }
    if (worst > tolerance) {
      (*kept)[at] = true;
      stack.emplace_back(from, at);
      stack.emplace_back(at, to);
    }
  }
}

}  // namespace

std::vector<QPointF> SimplifyStroke(std::span<const QPointF> stroke,
                                    double tolerance) {
  assert(tolerance >= 0.0);
  assert(stroke.size() < 100000000);
  if (stroke.size() > kMaxStrokePoints) {
    stroke = stroke.first(kMaxStrokePoints);
  }
  if (stroke.size() < 3) {
    return {stroke.begin(), stroke.end()};
  }
  std::vector<bool> kept(stroke.size(), false);
  kept.front() = true;
  kept.back() = true;
  Keep(stroke, 0, stroke.size() - 1, tolerance, &kept);
  std::vector<QPointF> places;
  for (size_t i = 0; i < stroke.size(); ++i) {
    if (kept[i]) {
      places.push_back(stroke[i]);
    }
  }
  return places;
}

std::vector<QPointF> OvalPlaces(const QRectF& box) {
  assert(std::isfinite(box.width()));
  assert(std::isfinite(box.height()));
  std::vector<QPointF> places;
  const QPointF center = box.center();
  for (int i = 0; i < 8; ++i) {
    const double angle = (i * 45.0 - 90.0) * std::numbers::pi / 180.0;
    places.emplace_back(center.x() + std::cos(angle) * box.width() / 2.0,
                        center.y() + std::sin(angle) * box.height() / 2.0);
  }
  return places;
}

std::vector<QPointF> RectanglePlaces(const QRectF& box) {
  assert(std::isfinite(box.width()));
  assert(std::isfinite(box.height()));
  const QRectF norm = box.normalized();
  return {norm.topLeft(), norm.topRight(), norm.bottomRight(),
          norm.bottomLeft()};
}

}  // namespace snapper
