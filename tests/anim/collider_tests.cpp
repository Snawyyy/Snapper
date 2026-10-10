#include <QImage>
#include <QPainter>
#include <QTest>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/collider.h"
#include "anim/outline_trace.h"

namespace snapper {
namespace {

QImage Filled(QSize size, QRect shape, int alpha) {
  assert(size.width() > 0 && size.height() > 0);
  assert(alpha >= 0 && alpha <= 255);
  QImage image(size, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.fillRect(shape, QColor(200, 30, 30, alpha));
  return image;
}

// Whether p is on the border of the rectangle.
bool IsOnBorder(QPointF p, QRectF r) {
  assert(r.width() > 0.0 && r.height() > 0.0);
  assert(std::isfinite(p.x()) && std::isfinite(p.y()));
  const bool is_in_x = p.x() >= r.left() - 1e-6 && p.x() <= r.right() + 1e-6;
  const bool is_in_y = p.y() >= r.top() - 1e-6 && p.y() <= r.bottom() + 1e-6;
  const bool is_on_side = std::abs(p.x() - r.left()) < 1e-6 ||
                          std::abs(p.x() - r.right()) < 1e-6;
  const bool is_on_end = std::abs(p.y() - r.top()) < 1e-6 ||
                         std::abs(p.y() - r.bottom()) < 1e-6;
  return is_in_x && is_in_y && (is_on_side || is_on_end);
}

}  // namespace

class ColliderTests final : public QObject {
  Q_OBJECT

 private slots:
  void TracesARectangleIntoAClosedRing();
  void TracesARoundShapeAndIgnoresFaintPixels();
  void PushesOnlyWhatCameInSinceRest();
  void PushesStraightOutOfTheRunItCameIn();
  void ComesOutTowardAPlace();
  void DeletedDotsLeaveAGap();
  void PointsOffTheDrawingRideItsEdgeCells();
};

void ColliderTests::TracesARectangleIntoAClosedRing() {
  const QImage image = Filled({60, 40}, QRect(10, 5, 30, 20), 255);
  const std::vector<QPointF> ring =
      TraceOutline(image, kOutlineAlpha, kOutlineSpacing, 96);
  QVERIFY2(ring.size() >= 6 && ring.size() <= 10,
           qPrintable(QString::number(ring.size())));
  double area = 0.0;
  for (size_t i = 0; i < ring.size(); ++i) {
    QVERIFY(IsOnBorder(ring[i], QRectF(10, 5, 30, 20)));
    const QPointF next = ring[(i + 1) % ring.size()];
    area += ring[i].x() * next.y() - next.x() * ring[i].y();
    const double step = std::hypot(next.x() - ring[i].x(),
                                   next.y() - ring[i].y());
    QVERIFY(step < kOutlineSpacing * 1.6);
  }
  // Clockwise on a screen with y down, shape on the right.
  QVERIFY(area > 0.0);
  QVERIFY(TraceOutline(QImage(), kOutlineAlpha, 12.0, 96).empty());
  QVERIFY(TraceOutline(Filled({20, 20}, QRect(), 255), kOutlineAlpha, 12.0, 96)
              .empty());
}

void ColliderTests::TracesARoundShapeAndIgnoresFaintPixels() {
  QImage disc(80, 80, QImage::Format_ARGB32);
  disc.fill(Qt::transparent);
  QPainter painter(&disc);
  painter.setBrush(QColor(0, 0, 0, 255));
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(QPointF(40, 40), 25, 25);
  painter.fillRect(QRect(0, 0, 6, 80), QColor(0, 0, 0, 60));
  painter.end();
  const std::vector<QPointF> ring =
      TraceOutline(disc, kOutlineAlpha, kOutlineSpacing, 96);
  QVERIFY(ring.size() >= 10 && ring.size() <= 16);
  for (const QPointF& dot : ring) {
    const double radius = std::hypot(dot.x() - 40.0, dot.y() - 40.0);
    QVERIFY2(std::abs(radius - 25.0) < 1.5,
             qPrintable(QString::number(radius)));
  }
  const std::vector<QPointF> few = TraceOutline(disc, kOutlineAlpha, 1.0, 20);
  QCOMPARE(few.size(), size_t{20});
}

void ColliderTests::PushesOnlyWhatCameInSinceRest() {
  // A floor from (0, 0) to (10, 0); the shape is below (y > 0).
  const std::vector<ColliderEdge> floor = {{{0, 0}, {10, 0}, {0, -1}}};
  const QPointF out(0.0, -kColliderClearance);
  // Outside at rest, inside now: back out where it is, just past.
  QCOMPARE(PushedFromRest(floor, {4, -2}, {4, 3}), QPointF(4, 0) + out);
  QCOMPARE(PushedFromRest(floor, {2, -2}, {6, 3}), QPointF(6, 0) + out);
  // Inside as drawn stays: a skirt drawn over a thigh is left alone.
  QCOMPARE(PushedFromRest(floor, {4, 2}, {6, 3}), QPointF(6, 3));
  // Still outside, or gone back out: never held or pulled.
  QCOMPARE(PushedFromRest(floor, {4, -2}, {4, -1}), QPointF(4, -1));
  QCOMPARE(PushedFromRest(floor, {4, 3}, {4, -3}), QPointF(4, -3));
  // Past the end of the edge nothing is crossed, and no edges push
  // nothing.
  QCOMPARE(PushedFromRest(floor, {14, -2}, {14, 3}), QPointF(14, 3));
  QCOMPARE(PushedFromRest({}, {0, -2}, {1, 1}), QPointF(1, 1));
}

void ColliderTests::PushesStraightOutOfTheRunItCameIn() {
  // A box 0..10 by 0..10 run clockwise (shape on the right, y down).
  const std::vector<QPointF> corners = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
  Collider box;
  box.ring_size = 4;
  for (int i = 0; i < 4; ++i) {
    box.dots.push_back({corners[static_cast<size_t>(i)], i});
  }
  const auto edges = ColliderEdges(box, corners, QTransform());
  QCOMPARE(edges.size(), size_t{4});
  // Came in from above and to the left, slanting far: out straight
  // across at the nearest spot of the sides facing where it came from
  // (the top here), not back along the way it came, and never through
  // the near right side to the far side of the box.
  const double clear = kColliderClearance;
  QCOMPARE(PushedFromRest(edges, {-20, -1}, {9, 2}), QPointF(9, -clear));
  // From the right, the right side is the way out.
  QCOMPARE(PushedFromRest(edges, {20, 2}, {9, 2}), QPointF(10 + clear, 2));
  // A mirrored drawing turns the outside around with it.
  const auto mirrored =
      ColliderEdges(box, corners, QTransform::fromScale(-1, 1));
  QCOMPARE(mirrored.front().out, QPointF(0, 1));
}

void ColliderTests::ComesOutTowardAPlace() {
  const std::vector<ColliderEdge> floor = {{{0, 0}, {10, 0}, {0, -1}}};
  const QPointF out = OutToward(floor, {5, 4}, {5, -6});
  QCOMPARE(out, QPointF(5, -kColliderClearance));
  // No way out crossed: it stays.
  QCOMPARE(OutToward(floor, {5, 4}, {5, 8}), QPointF(5, 4));
}

void ColliderTests::DeletedDotsLeaveAGap() {
  assert(kMaxColliderDots > 0);
  Collider collider;
  collider.ring_size = 8;
  for (int slot : {0, 1, 5, 6, 7}) {
    collider.dots.push_back({QPointF(slot, 0), slot});
  }
  const std::vector<QPointF> dots = {{5, 0}, {15, 0}, {15, 60}, {5, 60},
                                     {5, 30}};
  const auto edges = ColliderEdges(collider, dots, QTransform());
  // 0-1, 5-6, 6-7 and 7-0 round the ring; 1 to 5 is a gap.
  QCOMPARE(edges.size(), size_t{4});
  const bool is_bridged = std::any_of(
      edges.begin(), edges.end(), [&dots](const ColliderEdge& e) {
        return e.a == dots[1] && e.b == dots[2];
      });
  QVERIFY(!is_bridged);
  // A walk in through the gap pushes nothing.
  QCOMPARE(PushedFromRest(edges, {20, 30}, {10, 30}), QPointF(10, 30));
}

void ColliderTests::PointsOffTheDrawingRideItsEdgeCells() {
  // A 2 by 2 grid over a 20 by 20 drawing, its right column pushed 10
  // right: a point 10 past the right edge goes with it.
  const WarpGrid grid{2, 2};
  std::vector<QPointF> points = {{0, 0},  {10, 0},  {30, 0},
                                 {0, 10}, {10, 10}, {30, 10},
                                 {0, 20}, {10, 20}, {30, 20}};
  const auto shot =
      ShotPoints({{30, 10}, {5, 5}}, grid, {20, 20}, points, QTransform());
  QCOMPARE(shot.size(), size_t{2});
  QCOMPARE(shot[0], QPointF(50, 10));
  QCOMPARE(shot[1], QPointF(5, 5));
  // Unbent, the drawing's own map carries them.
  QCOMPARE(ShotPoints({{1, 2}}, WarpGrid(), {20, 20}, {},
                      QTransform::fromTranslate(5, 5))
               .front(),
           QPointF(6, 7));
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ColliderTests)
#include "collider_tests.moc"
