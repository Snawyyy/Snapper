#include <QImage>
#include <QPainter>
#include <QTest>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/collider.h"
#include "anim/doll_drag.h"
#include "anim/outline_trace.h"
#include "anim/sampler.h"

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

// A leg (thigh) in the shot at x 5 to 25 and a skirt (2 by 3 grid, 40
// wide, 10 pixels between rows) in front of it. The thigh's outline is
// its rectangle; the skirt's middle column dips into it.
Doll LegAndSkirt() {
  Doll doll;
  doll.art.pieces = {{"thigh", {"t.png"}, 0, {5, 0}, {20, 60}},
                     {"skirt", {"s.png"}, 0, {0, 0}, {40, 30}}};
  doll.rig.pieces = {{"thigh", "", {0, 0}, 0, -1, {}},
                     {"skirt", "", {0, 0}, 1, -1, {2, 3}}};
  doll.rig.pieces[0].collider = {
      8,
      {{{0, 0}, 0}, {{10, 0}, 1}, {{20, 0}, 2}, {{20, 30}, 3},
       {{20, 60}, 4}, {{10, 60}, 5}, {{0, 60}, 6}, {{0, 30}, 7}},
      10.0,
      {}};
  for (int point = 3; point < 12; ++point) {
    DragNode node;
    node.point = point;
    node.physics = true;
    node.gravity = 0.0;
    node.spring = 0.6;
    doll.rig.pieces[1].drag_nodes.push_back(node);
  }
  assert(doll.rig.pieces[1].warp.PointCount() == 12);
  assert(doll.rig.pieces[0].collider.IsOn());
  return doll;
}

Layer Still() {
  Layer layer;
  layer.content = DollLayer{"Doll", {}, false};
  assert(layer.transform.keys.empty());
  assert(std::holds_alternative<DollLayer>(layer.content));
  return layer;
}

// The skirt's middle column, one row down, in the shot (x 20).
QPointF MiddleNode(const Doll& doll, const Layer& layer, int frame) {
  assert(frame >= 0);
  assert(doll.rig.pieces.size() == 2);
  const PoseMap poses = DraggedPoses(doll, layer, Frame(frame));
  return QPointF(20, 10) + poses.at("skirt").warp.at(4);
}

}  // namespace

class ColliderTests final : public QObject {
  Q_OBJECT

 private slots:
  void TracesARectangleIntoAClosedRing();
  void TracesARoundShapeAndIgnoresFaintPixels();
  void PushesOutOfAnEdgeOneSided();
  void ASkirtPointDrivenIntoAThighEndsOutside();
  void AMovingAwayPointIsNotHeld();
  void ASparedPieceIsNotPushed();
  void DeletedDotsLeaveAGapThatDoesNotPush();
  void NoColliderMeansTheSamePoses();
  void AResumedTrailGivesTheSamePoses();
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

void ColliderTests::PushesOutOfAnEdgeOneSided() {
  // A floor edge from (0, 0) to (10, 0) with the outside up (y negative).
  const ColliderEdge floor{{0, 0}, {10, 0}, {0, -1}, 5.0};
  QCOMPARE(PushedOut({floor}, QPointF(4, 2)), QPointF(4, 0));
  // Outside, or off the side of the edge, or too deep: left alone.
  QCOMPARE(PushedOut({floor}, QPointF(4, -2)), QPointF(4, -2));
  QCOMPARE(PushedOut({floor}, QPointF(14, 2)), QPointF(14, 2));
  QCOMPARE(PushedOut({floor}, QPointF(4, 9)), QPointF(4, 9));
  QVERIFY(PushedOut({}, QPointF(1, 1)) == QPointF(1, 1));
}

void ColliderTests::ASkirtPointDrivenIntoAThighEndsOutside() {
  assert(kMaxColliderEdges > 0);
  const Doll doll = LegAndSkirt();
  const Layer layer = Still();
  for (int frame : {0, 1, 2, 5, 20, 60}) {
    const QPointF at = MiddleNode(doll, layer, frame);
    const bool is_outside_thigh = at.x() >= 25.0 - 1e-6 || at.x() <= 5.0;
    QVERIFY2(is_outside_thigh,
             qPrintable(QString::number(frame) + " " +
                        QString::number(at.x())));
  }
}

void ColliderTests::AMovingAwayPointIsNotHeld() {
  assert(kOutlineAlpha > 0);
  const Doll doll = LegAndSkirt();
  Layer layer = Still();
  auto& thigh = std::get<DollLayer>(layer.content).pieces["thigh"];
  PiecePose away;
  away.offset = QPointF(-200, 0);
  SetKey(&thigh, {Frame(0), PiecePose(), Ease::kStep});
  SetKey(&thigh, {Frame(8), away, Ease::kStep});
  // Pushed out while the leg is there, back on its drawn place after.
  QVERIFY(MiddleNode(doll, layer, 4).x() >= 25.0 - 1e-6);
  QVERIFY(std::abs(MiddleNode(doll, layer, 200).x() - 20.0) < 0.05);
}

void ColliderTests::ASparedPieceIsNotPushed() {
  Doll doll = LegAndSkirt();
  doll.rig.pieces[0].collider.spared = {"skirt"};
  QVERIFY(std::abs(MiddleNode(doll, Still(), 20).x() - 20.0) < 1e-6);
  // A piece drawn behind the collider is never pushed either.
  Doll behind = LegAndSkirt();
  behind.rig.pieces[1].order = -1;
  QVERIFY(std::abs(MiddleNode(behind, Still(), 20).x() - 20.0) < 1e-6);
}

void ColliderTests::DeletedDotsLeaveAGapThatDoesNotPush() {
  assert(kMaxColliderDots > 0);
  Doll doll = LegAndSkirt();
  Collider& collider = doll.rig.pieces[0].collider;
  // Deleting the right side's dots (slots 2, 3 and 4) leaves only the
  // left side: the node sits 15 from it, deeper than the push reaches.
  collider.dots.erase(collider.dots.begin() + 2, collider.dots.begin() + 5);
  QCOMPARE(collider.dots.size(), size_t{5});
  QVERIFY(std::abs(MiddleNode(doll, Still(), 20).x() - 20.0) < 1e-6);
  // The edge across the gap is not made.
  const std::vector<QPointF> dots = {{5, 0}, {15, 0}, {15, 60}, {5, 60},
                                     {5, 30}};
  const auto edges = ColliderEdges(collider, dots, QTransform());
  QCOMPARE(edges.size(), size_t{4});
  const bool is_bridged = std::any_of(
      edges.begin(), edges.end(), [&dots](const ColliderEdge& e) {
        return e.a == dots[1] && e.b == dots[2];
      });
  QVERIFY(!is_bridged);
}

void ColliderTests::NoColliderMeansTheSamePoses() {
  const Doll with = LegAndSkirt();
  Doll without = with;
  without.rig.pieces[0].collider = Collider();
  Doll plain_drag = with;
  for (DragNode& node : plain_drag.rig.pieces[1].drag_nodes) {
    node.physics = false;
  }
  Doll plain_without = plain_drag;
  plain_without.rig.pieces[0].collider = Collider();
  Layer layer = Still();
  PiecePose moved;
  moved.offset = QPointF(12, 6);
  SetKey(&layer.transform, {Frame(0), PiecePose(), Ease::kStep});
  SetKey(&layer.transform, {Frame(2), moved, Ease::kStep});
  for (int frame : {0, 2, 6, 30}) {
    // Plain drag nodes never feel a collider.
    QCOMPARE(DraggedPoses(plain_drag, layer, Frame(frame)),
             DraggedPoses(plain_without, layer, Frame(frame)));
  }
  QVERIFY(DraggedPoses(with, Still(), Frame(20)) !=
          DraggedPoses(without, Still(), Frame(20)));
}

void ColliderTests::AResumedTrailGivesTheSamePoses() {
  const Doll doll = LegAndSkirt();
  Layer layer = Still();
  auto& thigh = std::get<DollLayer>(layer.content).pieces["thigh"];
  PiecePose over;
  over.offset = QPointF(-8, 0);
  SetKey(&thigh, {Frame(0), PiecePose(), Ease::kStep});
  SetKey(&thigh, {Frame(6), over, Ease::kLinear});
  SetKey(&thigh, {Frame(14), PiecePose(), Ease::kLinear});
  DragTrail trail;
  for (int frame : {0, 3, 7, 30, 30, 5, 80, 12}) {
    QCOMPARE(DraggedPoses(doll, layer, Frame(frame), {}, &trail),
             DraggedPoses(doll, layer, Frame(frame)));
  }
  QVERIFY(trail.frames().size() > 50);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ColliderTests)
#include "collider_tests.moc"
