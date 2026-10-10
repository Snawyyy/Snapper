#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <map>

#include "anim/delaunay.h"
#include "anim/jitter.h"
#include "anim/shape_mesh.h"
#include "anim/warp.h"

namespace snapper {

class ShapeMeshTests : public QObject {
  Q_OBJECT

 private slots:
  void DelaunayTilesScatteredPoints();
  void DelaunayKeepsCirclesEmpty();
  void DelaunayTilesALattice();
  void MeshHugsTheDrawingAndCoversIt();
  void MeshSidesFollowTheOutline();
  void MeshIsTheSameEveryTime();
  void MeshKeepsTheHoleOfARing();
  void DetailAddsDots();
  void ScatteredSpecksAreAllCovered();
  void NothingVisibleMakesNoMesh();
  void SeparateShapesNeverPullEachOther();
  void MeshCoversEveryDrawingOfAPiece();
};

namespace {

std::vector<QPointF> Scattered(int count) {
  assert(count > 0);
  assert(count <= kMaxDelaunayPoints);
  std::vector<QPointF> points;
  for (int i = 0; i < count; ++i) {
    points.emplace_back(500.0 + 400.0 * Jitter(i, 0),
                        300.0 + 250.0 * Jitter(i, 1));
  }
  return points;
}

// A clear picture of size with these shapes painted solid.
QImage Painted(QSize size, const std::vector<QPainterPath>& shapes) {
  assert(!size.isEmpty());
  assert(!shapes.empty());
  QImage image(size, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(200, 40, 60));
  for (const QPainterPath& shape : shapes) {
    painter.drawPath(shape);
  }
  return image;
}

QPainterPath Disc(QPointF centre, double radius) {
  assert(radius > 0.0);
  assert(std::isfinite(centre.x()));
  QPainterPath path;
  path.addEllipse(centre, radius, radius);
  return path;
}

// Whether spot is inside (or on) triangle t of mesh.
bool IsIn(const WarpMesh& mesh, const MeshTriangle& t, QPointF spot) {
  assert(!mesh.points.empty());
  assert(t[0] >= 0);
  const QPointF a = mesh.points[static_cast<size_t>(t[0])];
  const QPointF b = mesh.points[static_cast<size_t>(t[1])];
  const QPointF c = mesh.points[static_cast<size_t>(t[2])];
  const auto side = [](QPointF p, QPointF q, QPointF r) {
    return (q.x() - p.x()) * (r.y() - p.y()) -
           (q.y() - p.y()) * (r.x() - p.x());
  };
  const double s1 = side(a, b, spot);
  const double s2 = side(b, c, spot);
  const double s3 = side(c, a, spot);
  return (s1 >= -1e-6 && s2 >= -1e-6 && s3 >= -1e-6) ||
         (s1 <= 1e-6 && s2 <= 1e-6 && s3 <= 1e-6);
}

// How many visible pixels of image no triangle of mesh covers.
int Uncovered(const WarpMesh& mesh, const QImage& image) {
  assert(!image.isNull());
  assert(!mesh.triangles.empty());
  int uncovered = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const bool is_visible = qAlpha(image.pixel(x, y)) > 0;
      const QPointF middle(x + 0.5, y + 0.5);
      const bool is_covered = std::any_of(
          mesh.triangles.begin(), mesh.triangles.end(),
          [&](const MeshTriangle& t) { return IsIn(mesh, t, middle); });
      uncovered += is_visible && !is_covered ? 1 : 0;
    }
  }
  return uncovered;
}

// How far spot is from the nearest visible pixel's middle.
double FromVisible(const QImage& image, QPointF spot) {
  assert(!image.isNull());
  assert(std::isfinite(spot.x()));
  double nearest = 1e9;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const bool is_visible = qAlpha(image.pixel(x, y)) > 0;
      if (is_visible) {
        nearest = std::min(nearest, std::hypot(x + 0.5 - spot.x(),
                                               y + 0.5 - spot.y()));
      }
    }
  }
  return nearest;
}

double Turn(QPointF a, QPointF b, QPointF c) {
  assert(std::isfinite(a.x() + b.x() + c.x()));
  assert(std::isfinite(a.y() + b.y() + c.y()));
  return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}

}  // namespace

void ShapeMeshTests::DelaunayTilesScatteredPoints() {
  const std::vector<QPointF> points = Scattered(300);
  const std::vector<MeshTriangle> triangles = Delaunay(points);
  QVERIFY(IsTiled(points, triangles));
  QVERIFY(triangles.size() <= 2 * points.size());
  QVERIFY(triangles.size() >= points.size());
  double area = 0.0;
  for (const MeshTriangle& t : triangles) {
    area += Turn(points[static_cast<size_t>(t[0])],
                 points[static_cast<size_t>(t[1])],
                 points[static_cast<size_t>(t[2])]) /
            2.0;
  }
  // The triangles cover most of the box the points spread over.
  QVERIFY(area > 0.8 * 800.0 * 500.0);
}

void ShapeMeshTests::DelaunayKeepsCirclesEmpty() {
  const std::vector<QPointF> points = Scattered(120);
  const std::vector<MeshTriangle> triangles = Delaunay(points);
  QVERIFY(!triangles.empty());
  int inside = 0;
  for (const MeshTriangle& t : triangles) {
    const QPointF a = points[static_cast<size_t>(t[0])];
    const QPointF b = points[static_cast<size_t>(t[1])];
    const QPointF c = points[static_cast<size_t>(t[2])];
    for (const QPointF& d : points) {
      const QPointF ad = a - d;
      const QPointF bd = b - d;
      const QPointF cd = c - d;
      const double det =
          QPointF::dotProduct(ad, ad) * (bd.x() * cd.y() - cd.x() * bd.y()) +
          QPointF::dotProduct(bd, bd) * (cd.x() * ad.y() - ad.x() * cd.y()) +
          QPointF::dotProduct(cd, cd) * (ad.x() * bd.y() - bd.x() * ad.y());
      inside += det > 1e-3 ? 1 : 0;
    }
  }
  QCOMPARE(inside, 0);
}

void ShapeMeshTests::DelaunayTilesALattice() {
  std::vector<QPointF> points;
  for (int i = 0; i < 100; ++i) {
    points.emplace_back(10.0 * (i % 10) + 1e-3 * Jitter(i, 0),
                        10.0 * (i / 10) + 1e-3 * Jitter(i, 1));
  }
  const std::vector<MeshTriangle> triangles = Delaunay(points);
  QVERIFY(IsTiled(points, triangles));
  double area = 0.0;
  for (const MeshTriangle& t : triangles) {
    area += Turn(points[static_cast<size_t>(t[0])],
                 points[static_cast<size_t>(t[1])],
                 points[static_cast<size_t>(t[2])]) /
            2.0;
  }
  // The lattice's square, give or take slivers along its nudged sides.
  QVERIFY(std::abs(area - 90.0 * 90.0) < 1.0);
}

void ShapeMeshTests::MeshHugsTheDrawingAndCoversIt() {
  const QImage image = Painted(QSize(120, 100), {Disc({60, 50}, 40)});
  const auto mesh = BuildShapeMesh({image}, MeshDetail::kNormal);
  QVERIFY(mesh != nullptr);
  QCOMPARE(Uncovered(*mesh, image), 0);
  // No dot strays past the thin margin round the outline.
  for (const QPointF& point : mesh->points) {
    QVERIFY(FromVisible(image, point) < 6.0);
  }
  QVERIFY(mesh->triangles.size() < 2 * mesh->points.size());
  QVERIFY(mesh->points.size() >= 20 && mesh->points.size() <= 120);
  QVERIFY(IsTiled(mesh->points, mesh->triangles));
}

void ShapeMeshTests::MeshSidesFollowTheOutline() {
  const QImage image = Painted(QSize(120, 100), {Disc({60, 50}, 40)});
  const auto mesh = BuildShapeMesh({image}, MeshDetail::kNormal);
  QVERIFY(mesh != nullptr);
  // A side only one triangle has is on the mesh's rim: it runs along
  // the outline, never across the inside.
  std::map<MeshEdge, int> uses;
  for (const MeshTriangle& t : mesh->triangles) {
    for (size_t k = 0; k < 3; ++k) {
      ++uses[{std::min(t[k], t[(k + 1) % 3]), std::max(t[k], t[(k + 1) % 3])}];
    }
  }
  int rim = 0;
  for (const auto& [edge, count] : uses) {
    const QPointF middle = (mesh->points[static_cast<size_t>(edge.first)] +
                            mesh->points[static_cast<size_t>(edge.second)]) /
                           2.0;
    const bool is_rim = count == 1;
    if (is_rim) {
      ++rim;
      const double from_centre = std::hypot(middle.x() - 60, middle.y() - 50);
      QVERIFY2(std::abs(from_centre - 40.0) < 5.0,
               qPrintable(QString::number(from_centre)));
    }
  }
  QVERIFY(rim >= 12);
}

void ShapeMeshTests::MeshIsTheSameEveryTime() {
  const QImage image = Painted(
      QSize(150, 90), {Disc({40, 45}, 30), Disc({110, 40}, 25)});
  const auto first = BuildShapeMesh({image}, MeshDetail::kMany);
  const auto second = BuildShapeMesh({image}, MeshDetail::kMany);
  QVERIFY(first != nullptr && second != nullptr);
  QVERIFY(*first == *second);
  QCOMPARE(first->edges, second->edges);
  QCOMPARE(first->apart, second->apart);
}

void ShapeMeshTests::MeshKeepsTheHoleOfARing() {
  QPainterPath ring = Disc({60, 60}, 50);
  ring.addEllipse(QPointF(60, 60), 25, 25);
  const QImage image = Painted(QSize(120, 120), {ring});
  QCOMPARE(qAlpha(image.pixel(60, 60)), 0);
  const auto mesh = BuildShapeMesh({image}, MeshDetail::kNormal);
  QVERIFY(mesh != nullptr);
  QCOMPARE(Uncovered(*mesh, image), 0);
  for (const MeshTriangle& t : mesh->triangles) {
    const QPointF middle = (mesh->points[static_cast<size_t>(t[0])] +
                            mesh->points[static_cast<size_t>(t[1])] +
                            mesh->points[static_cast<size_t>(t[2])]) /
                           3.0;
    QVERIFY(std::hypot(middle.x() - 60, middle.y() - 60) > 18.0);
  }
}

void ShapeMeshTests::DetailAddsDots() {
  const QImage image = Painted(QSize(200, 160), {Disc({100, 80}, 70)});
  const auto few = BuildShapeMesh({image}, MeshDetail::kFew);
  const auto normal = BuildShapeMesh({image}, MeshDetail::kNormal);
  const auto many = BuildShapeMesh({image}, MeshDetail::kMany);
  QVERIFY(few != nullptr && normal != nullptr && many != nullptr);
  QVERIFY(few->points.size() < normal->points.size());
  QVERIFY(normal->points.size() < many->points.size());
  QVERIFY(many->points.size() <= static_cast<size_t>(kMaxMeshPoints));
  QVERIFY(few->cell > many->cell);
}

void ShapeMeshTests::ScatteredSpecksAreAllCovered() {
  QImage image(160, 160, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  for (int i = 0; i < 400; ++i) {
    const int x = static_cast<int>(80 + 78 * Jitter(i, 2));
    const int y = static_cast<int>(80 + 78 * Jitter(i, 3));
    image.setPixel(x, y, qRgba(0, 0, 0, 255));
  }
  const auto mesh = BuildShapeMesh({image}, MeshDetail::kMany);
  QVERIFY(mesh != nullptr);
  QVERIFY(mesh->points.size() <= static_cast<size_t>(kMaxMeshPoints));
  QCOMPARE(Uncovered(*mesh, image), 0);
}

void ShapeMeshTests::NothingVisibleMakesNoMesh() {
  QImage image(40, 40, QImage::Format_ARGB32);
  image.fill(qRgba(255, 255, 255, 4));
  QVERIFY(BuildShapeMesh({image}, MeshDetail::kNormal) == nullptr);
  QVERIFY(BuildShapeMesh({}, MeshDetail::kNormal) == nullptr);
}

void ShapeMeshTests::SeparateShapesNeverPullEachOther() {
  const QImage image = Painted(
      QSize(200, 80), {Disc({40, 40}, 30), Disc({160, 40}, 30)});
  const auto mesh = BuildShapeMesh({image}, MeshDetail::kNormal);
  QVERIFY(mesh != nullptr);
  const WarpGrid grid{0, 0, mesh};
  int left = -1;
  int right = -1;
  for (size_t i = 0; i < mesh->points.size(); ++i) {
    left = mesh->points[i].x() < 100 ? static_cast<int>(i) : left;
    right = mesh->points[i].x() > 100 ? static_cast<int>(i) : right;
  }
  QVERIFY(left >= 0 && right >= 0);
  QVERIFY(CellsApart(grid, left, right) >= kFarApart);
  QCOMPARE(PullWeight(grid, left, right, 8.0), 0.0);
}

void ShapeMeshTests::MeshCoversEveryDrawingOfAPiece() {
  const QImage open = Painted(QSize(100, 100), {Disc({30, 50}, 20)});
  const QImage shut = Painted(QSize(100, 100), {Disc({70, 50}, 20)});
  const auto mesh = BuildShapeMesh({open, shut}, MeshDetail::kNormal);
  QVERIFY(mesh != nullptr);
  QCOMPARE(Uncovered(*mesh, open), 0);
  QCOMPARE(Uncovered(*mesh, shut), 0);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ShapeMeshTests)
#include "shape_mesh_tests.moc"
