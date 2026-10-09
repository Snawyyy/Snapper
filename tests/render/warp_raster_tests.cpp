#include <QImage>
#include <QTest>

#include <cassert>
#include <vector>

#include "anim/warp.h"
#include "render/warp_raster.h"

namespace snapper {
namespace {

constexpr WarpGrid kGrid{3, 3};
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

}  // namespace snapper

QTEST_MAIN(snapper::WarpRasterTests)
#include "warp_raster_tests.moc"
