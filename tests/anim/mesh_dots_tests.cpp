#include <QImage>
#include <QTest>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/doll_drag.h"
#include "anim/mesh_carry.h"
#include "anim/mirror.h"
#include "anim/shape_mesh.h"
#include "anim/warp_motion.h"
#include "anim/warp_shape.h"
#include "leg_skirt.h"

namespace snapper {
namespace {

const QSize kSkirt(80, 60);

double Length(QPointF v) { return std::hypot(v.x(), v.y()); }

// The skirt of LegAndSkirt with a shape mesh made from its drawing (a
// solid 80 by 60 block) in place of its grid.
WarpGrid SkirtMesh() {
  QImage drawing(kSkirt, QImage::Format_ARGB32);
  drawing.fill(Qt::black);
  const WarpGrid mesh{0, 0, BuildShapeMesh({drawing}, MeshDetail::kNormal)};
  assert(mesh.IsMesh());
  assert(mesh.PointCount() > 20);
  return mesh;
}

Doll MeshedLegAndSkirt() {
  Doll doll = leg_skirt::LegAndSkirt();
  doll.rig.pieces[1].warp = SkirtMesh();
  assert(doll.rig.pieces[1].warp.IsMesh());
  assert(doll.rig.pieces[0].collider.IsOn());
  return doll;
}

// The skirt's mesh dots as shown (doll space is its drawing space).
std::vector<QPointF> Shown(const Doll& doll, const PoseMap& poses) {
  const WarpGrid& mesh = doll.rig.pieces[1].warp;
  assert(mesh.IsMesh());
  const auto pose = poses.find("skirt");
  const bool has_pose = pose != poses.end();
  assert(poses.size() <= static_cast<size_t>(kMaxDollPieces));
  return WarpedPoints(kSkirt, mesh, has_pose ? pose->second.warp
                                             : std::vector<QPointF>());
}

// The mesh dot nearest spot.
int DotNear(const WarpGrid& mesh, QPointF spot) {
  assert(mesh.IsMesh());
  assert(std::isfinite(spot.x()));
  return NearestPoint(RestPoints(kSkirt, mesh), spot, 1e9);
}

// How many of mesh's triangles points has turned over against rest.
int Flipped(const WarpGrid& mesh, const std::vector<QPointF>& points) {
  assert(mesh.IsMesh());
  const std::vector<QPointF> rest = RestPoints(kSkirt, mesh);
  assert(points.size() == rest.size());
  const auto turn = [](const std::vector<QPointF>& p, const MeshTriangle& t) {
    const QPointF a = p[static_cast<size_t>(t[0])];
    const QPointF b = p[static_cast<size_t>(t[1])];
    const QPointF c = p[static_cast<size_t>(t[2])];
    return (b.x() - a.x()) * (c.y() - a.y()) -
           (b.y() - a.y()) * (c.x() - a.x());
  };
  int flipped = 0;
  for (const MeshTriangle& t : mesh.mesh->triangles) {
    flipped += turn(rest, t) * turn(points, t) < 0.0 ? 1 : 0;
  }
  return flipped;
}

}  // namespace

class MeshDotsTests final : public QObject {
  Q_OBJECT

 private slots:
  void ACollidersPushesMeshDotsOut();
  void ADragNodeOnAMeshDotTrails();
  void ClothOnAMeshStaysASheet();
  void AMotionSpreadsOverMeshNeighbours();
  void APoseMirrorsOntoTheMesh();
  void CarryingKeepsKeysAndMovesSettings();
};

void MeshDotsTests::ACollidersPushesMeshDotsOut() {
  const Doll doll = MeshedLegAndSkirt();
  const Layer layer = leg_skirt::Kicking({{0, 0.0}, {6, 60.0}, {12, 0.0}});
  const std::vector<QPointF> rest = Shown(doll, PoseMap());
  double pushed = 0.0;
  for (int frame = 0; frame <= 12; ++frame) {
    const PoseMap poses = DraggedPoses(doll, layer, Frame(frame));
    const std::vector<QPointF> now = Shown(doll, poses);
    for (size_t i = 0; i < now.size(); ++i) {
      const bool was_in =
          leg_skirt::IsInThigh(doll, PoseMap(), rest[i], 0.0);
      QVERIFY2(was_in || !leg_skirt::IsInThigh(doll, poses, now[i], 0.5),
               qPrintable(QString("frame %1 dot %2 at %3,%4 rest %5,%6")
                              .arg(frame).arg(i).arg(now[i].x())
                              .arg(now[i].y()).arg(rest[i].x())
                              .arg(rest[i].y())));
      pushed = std::max(pushed, Length(now[i] - rest[i]));
    }
  }
  QVERIFY2(pushed > 5.0, qPrintable(QString::number(pushed)));
  const PoseMap back = DraggedPoses(doll, layer, Frame(12));
  QCOMPARE(Shown(doll, back), rest);
}

void MeshDotsTests::ADragNodeOnAMeshDotTrails() {
  Doll doll = MeshedLegAndSkirt();
  doll.rig.pieces[0].collider = Collider();
  const WarpGrid& mesh = doll.rig.pieces[1].warp;
  const int hem = DotNear(mesh, QPointF(40, 60));
  doll.rig.pieces[1].drag_nodes = {DragNode{hem}};
  Layer layer = leg_skirt::Still();
  PiecePose far;
  far.offset = QPointF(200, 0);
  SetKey(&layer.transform, {Frame(0), PiecePose(), Ease::kLinear});
  SetKey(&layer.transform, {Frame(6), far, Ease::kLinear});
  const std::vector<QPointF> now =
      Shown(doll, DraggedPoses(doll, layer, Frame(4)));
  const std::vector<QPointF> rest = Shown(doll, PoseMap());
  QVERIFY(Length(now[static_cast<size_t>(hem)] -
                 rest[static_cast<size_t>(hem)]) > 1.0);
  const int top = DotNear(mesh, QPointF(0, 0));
  QCOMPARE(now[static_cast<size_t>(top)], rest[static_cast<size_t>(top)]);
}

void MeshDotsTests::ClothOnAMeshStaysASheet() {
  Doll doll = MeshedLegAndSkirt();
  const WarpGrid& mesh = doll.rig.pieces[1].warp;
  const std::vector<QPointF> rest = RestPoints(kSkirt, mesh);
  for (size_t i = 0; i < rest.size(); ++i) {
    const bool is_low = rest[i].y() > 30.0;
    if (is_low) {
      DragNode node;
      node.point = static_cast<int>(i);
      node.physics = true;
      doll.rig.pieces[1].drag_nodes.push_back(node);
    }
  }
  const Layer layer = leg_skirt::Kicking(
      {{0, 0.0}, {4, 70.0}, {8, -70.0}, {12, 70.0}, {16, 0.0}});
  int moved = 0;
  for (int frame = 0; frame <= 24; ++frame) {
    const std::vector<QPointF> now =
        Shown(doll, DraggedPoses(doll, layer, Frame(frame)));
    for (const QPointF& point : now) {
      QVERIFY(std::isfinite(point.x()) && std::isfinite(point.y()));
    }
    QCOMPARE(Flipped(mesh, now), 0);
    moved += now != rest ? 1 : 0;
  }
  QVERIFY(moved > 0);
}

void MeshDotsTests::AMotionSpreadsOverMeshNeighbours() {
  const WarpGrid mesh = SkirtMesh();
  const int middle = DotNear(mesh, QPointF(40, 30));
  PointMotion motion{middle, WarpMotionKind::kSway, 10.0, 24, 0.0, 0.0};
  const std::vector<QPointF> pushes =
      PointMotionPushes(mesh, motion, 2.0, Frame(6));
  const double own = Length(pushes[static_cast<size_t>(middle)]);
  QVERIFY(own > 5.0);
  for (int i = 0; i < mesh.PointCount(); ++i) {
    const double cells = CellsApart(mesh, middle, i);
    const double push = Length(pushes[static_cast<size_t>(i)]);
    QVERIFY(push <= own + 1e-9);
    QVERIFY(cells < 2.0 || push == 0.0);
  }
}

void MeshDotsTests::APoseMirrorsOntoTheMesh() {
  const WarpGrid mesh = SkirtMesh();
  const int left = DotNear(mesh, QPointF(0, 30));
  const int right = DotNear(mesh, QPointF(80, 30));
  PiecePose pose;
  pose.warp.assign(static_cast<size_t>(mesh.PointCount()), QPointF());
  pose.warp[static_cast<size_t>(left)] = QPointF(-6, 2);
  const PiecePose mirrored = MirrorPose(pose, mesh, kSkirt, mesh, kSkirt);
  QCOMPARE(mirrored.warp.size(), pose.warp.size());
  const QPointF got = mirrored.warp[static_cast<size_t>(right)];
  QVERIFY2(Length(got - QPointF(6, 2)) < 1.0,
           qPrintable(QString("%1 %2").arg(got.x()).arg(got.y())));
}

void MeshDotsTests::CarryingKeepsKeysAndMovesSettings() {
  RigPiece grid{"skirt", "", {40, 0}, 1, -1, WarpGrid{4, 3}};
  grid.drag_nodes = {DragNode{17}};
  grid.point_motions = {PointMotion{2}};
  const WarpGrid mesh = SkirtMesh();
  RigPiece meshed;
  const CarryCount count = CarryDots(grid, kSkirt, mesh, kSkirt, &meshed);
  QCOMPARE(count, (CarryCount{2, 0}));
  // Point 17 sat at (40, 60) and point 2 at (40, 0).
  QCOMPARE(meshed.drag_nodes[0].point, DotNear(mesh, QPointF(40, 60)));
  QCOMPARE(meshed.point_motions[0].point, DotNear(mesh, QPointF(40, 0)));
  // A key that slid the whole grid slides the whole mesh.
  const std::vector<QPointF> slid(20, QPointF(5, -3));
  const std::vector<QPointF> carried =
      CarryOffsets(WarpGrid{4, 3}, kSkirt, slid, mesh, kSkirt);
  QCOMPARE(carried.size(), static_cast<size_t>(mesh.PointCount()));
  for (const QPointF& offset : carried) {
    QVERIFY(Length(offset - QPointF(5, -3)) < 1e-9);
  }
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::MeshDotsTests)
#include "mesh_dots_tests.moc"
