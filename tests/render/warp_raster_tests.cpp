#include <QImage>
#include <QTest>

#include <cassert>
#include <cmath>
#include <vector>

#include "anim/shape_mesh.h"
#include "anim/warp.h"
#include "render/warp_raster.h"

namespace snapper {
namespace {

const WarpGrid kGrid{3, 3};
constexpr int kSide = 64;

// A picture with a different colour and see-through-ness at every
// pixel, so a change of rounding shows.
QImage Gradient() {
  QImage image(kSide, kSide, QImage::Format_ARGB32);
  for (int y = 0; y < kSide; ++y) {
    for (int x = 0; x < kSide; ++x) {
      image.setPixelColor(x, y, QColor(x * 4, y * 4, (x * y) % 256,
                                       64 + (x + y) * 15 / 8));
    }
  }
  assert(image.width() == kSide);
  assert(image.height() == kSide);
  return image;
}

// The rest grid with every point pushed a little off its place.
std::vector<QPointF> Crumpled() {
  std::vector<QPointF> points = RestPoints(QSize(kSide, kSide), kGrid);
  assert(points.size() == static_cast<size_t>(kGrid.PointCount()));
  for (int i = 0; i < static_cast<int>(points.size()); ++i) {
    points[static_cast<size_t>(i)] +=
        QPointF(3.7 * ((i * 7) % 5) - 6.0 + i * 0.13,
                2.9 * ((i * 3) % 4) - 4.0 - i * 0.07);
  }
  assert(!points.empty());
  return points;
}

// A fingerprint of every byte of the picture (FNV-1a, the same on every
// machine, unlike qHash).
quint64 Fingerprint(const QImage& image) {
  assert(!image.isNull());
  assert(image.format() == QImage::Format_ARGB32_Premultiplied);
  quint64 sum = 14695981039346656037ULL;
  const uchar* const bytes = image.constBits();
  for (qsizetype i = 0; i < image.sizeInBytes(); ++i) {
    sum = (sum ^ bytes[i]) * 1099511628211ULL;
  }
  return sum;
}

}  // namespace

class WarpRasterTests final : public QObject {
  Q_OBJECT

 private slots:
  void BendingKeepsEveryPixelTheSame();
  void RestGridLeavesThePictureAlone();
  void RestMeshLeavesEveryDrawnPixelAlone();
  void BentMeshLeavesNoSeams();
};

void WarpRasterTests::BendingKeepsEveryPixelTheSame() {
  const WarpedImage warped = WarpImage(Gradient(), kGrid, Crumpled());
  QCOMPARE(warped.image.size(), QSize(81, 73));
  QCOMPARE(warped.origin, QPointF(-6.0, -4.0));
  // Pinned so a later change of the bend shows up as a changed picture.
  QCOMPARE(Fingerprint(warped.image), quint64{17119216666373926065ULL});
}

void WarpRasterTests::RestGridLeavesThePictureAlone() {
  const QImage source =
      Gradient().convertedTo(QImage::Format_ARGB32_Premultiplied);
  const WarpedImage warped =
      WarpImage(source, kGrid, RestPoints(source.size(), kGrid));
  QCOMPARE(warped.origin, QPointF(0, 0));
  int differing = 0;
  for (int y = 0; y < kSide; ++y) {
    for (int x = 0; x < kSide; ++x) {
      const bool is_same = warped.image.pixel(x, y) == source.pixel(x, y);
      differing += is_same ? 0 : 1;
    }
  }
  QCOMPARE(differing, 0);
}

// The gradient cut to a disc, clear around it.
QImage GradientDisc() {
  QImage image = Gradient();
  const QPointF centre(kSide / 2.0, kSide / 2.0);
  for (int y = 0; y < kSide; ++y) {
    for (int x = 0; x < kSide; ++x) {
      const bool is_out =
          std::hypot(x + 0.5 - centre.x(), y + 0.5 - centre.y()) > 26.0;
      if (is_out) {
        image.setPixel(x, y, 0u);
      }
    }
  }
  assert(qAlpha(image.pixel(0, 0)) == 0);
  assert(qAlpha(image.pixel(kSide / 2, kSide / 2)) > 0);
  return image.convertedTo(QImage::Format_ARGB32_Premultiplied);
}

void WarpRasterTests::RestMeshLeavesEveryDrawnPixelAlone() {
  const QImage source = GradientDisc();
  const WarpGrid mesh{0, 0, BuildShapeMesh({source}, MeshDetail::kMany)};
  QVERIFY(mesh.IsMesh());
  const WarpedImage warped =
      WarpImage(source, mesh, RestPoints(source.size(), mesh));
  int differing = 0;
  for (int y = 0; y < kSide; ++y) {
    for (int x = 0; x < kSide; ++x) {
      const QPoint at = QPoint(x, y) - warped.origin.toPoint();
      const bool is_drawn = qAlpha(source.pixel(x, y)) > 0;
      const bool is_same =
          !is_drawn || (warped.image.rect().contains(at) &&
                        warped.image.pixel(at) == source.pixel(x, y));
      differing += is_same ? 0 : 1;
    }
  }
  QCOMPARE(differing, 0);
}

void WarpRasterTests::BentMeshLeavesNoSeams() {
  QImage source(kSide, kSide, QImage::Format_ARGB32_Premultiplied);
  source.fill(Qt::black);
  const WarpGrid mesh{0, 0, BuildShapeMesh({source}, MeshDetail::kMany)};
  QVERIFY(mesh.IsMesh());
  std::vector<QPointF> points = RestPoints(source.size(), mesh);
  for (size_t i = 0; i < points.size(); ++i) {
    const double k = static_cast<double>(i);
    points[i] += QPointF(1.7 * std::sin(k * 1.3), 1.7 * std::cos(k * 0.7));
  }
  const WarpedImage warped = WarpImage(source, mesh, points);
  int holes = 0;
  for (int y = 6; y < kSide - 6; ++y) {
    for (int x = 6; x < kSide - 6; ++x) {
      const QPoint at = QPoint(x, y) - warped.origin.toPoint();
      holes += qAlpha(warped.image.pixel(at)) == 255 ? 0 : 1;
    }
  }
  QCOMPARE(holes, 0);
}

}  // namespace snapper

QTEST_MAIN(snapper::WarpRasterTests)
#include "warp_raster_tests.moc"
