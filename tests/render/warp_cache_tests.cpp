#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include <cassert>
#include <vector>

#include "anim/warp.h"
#include "render/image_cache.h"
#include "render/warp_cache.h"

namespace snapper {
namespace {

const WarpGrid kGrid{2, 2};

// A 40x40 drawing on disk.
QString WriteDrawing(const QTemporaryDir& dir) {
  QImage art(40, 40, QImage::Format_ARGB32);
  art.fill(Qt::green);
  const QString path = QDir(dir.path()).filePath("art.png");
  art.save(path);
  assert(!path.isEmpty());
  assert(dir.isValid());
  return path;
}

std::vector<QPointF> Pushed(double dx) {
  std::vector<QPointF> points = RestPoints(QSize(40, 40), kGrid);
  assert(!points.empty());
  points.back() += QPointF(dx, 0.0);
  assert(points.size() == static_cast<size_t>(kGrid.PointCount()));
  return points;
}

}  // namespace

class WarpCacheTests final : public QObject {
  Q_OBJECT

 private slots:
  void SameBendIsMadeOnce();
  void ANewBendIsMadeAndKept();
  void ForgettingAPathDropsItsBends();
  void UnreadableDrawingsBendToNothing();
  void TheCacheStaysWithinItsCap();
  void ASmallBendIsHalfTheSizeAndKeptApart();
};

void WarpCacheTests::SameBendIsMadeOnce() {
  QTemporaryDir dir;
  ImageCache cache;
  const QString path = WriteDrawing(dir);
  QVERIFY(QFile::exists(path));
  const std::vector<QPointF> points = Pushed(5.0);
  const WarpedImage& first = cache.Warped(path, kGrid, points);
  QVERIFY(!first.image.isNull());
  const uchar* const pixels = first.image.constBits();
  const WarpedImage& again = cache.Warped(path, kGrid, points);
  QCOMPARE(again.image.constBits(), pixels);
  QCOMPARE(again.origin, first.origin);
}

void WarpCacheTests::ANewBendIsMadeAndKept() {
  QTemporaryDir dir;
  const QString path = WriteDrawing(dir);
  const QImage source(path);
  WarpCache cache;
  const WarpedImage& near = cache.Bent(path, source, kGrid, Pushed(2.0));
  const uchar* const near_pixels = near.image.constBits();
  const WarpedImage& far = cache.Bent(path, source, kGrid, Pushed(9.0));
  QVERIFY(far.image.constBits() != near_pixels);
  QCOMPARE(cache.size(), size_t{2});
  const WarpedImage& back = cache.Bent(path, source, kGrid, Pushed(2.0));
  QCOMPARE(back.image.constBits(), near_pixels);
  QCOMPARE(cache.size(), size_t{2});
}

void WarpCacheTests::ForgettingAPathDropsItsBends() {
  QTemporaryDir dir;
  const QString path = WriteDrawing(dir);
  const QImage source(path);
  WarpCache cache;
  cache.Bent(path, source, kGrid, Pushed(2.0));
  cache.Bent(path, source, kGrid, Pushed(3.0));
  QVERIFY(cache.bytes() > 0);
  cache.Forget(path);
  QCOMPARE(cache.size(), size_t{0});
  QCOMPARE(cache.bytes(), std::int64_t{0});
}

void WarpCacheTests::UnreadableDrawingsBendToNothing() {
  ImageCache cache;
  const WarpedImage& none =
      cache.Warped("/no/such/drawing.png", kGrid, Pushed(2.0));
  QVERIFY(none.image.isNull());
  QVERIFY(cache.bytes() >= 0);
}

void WarpCacheTests::TheCacheStaysWithinItsCap() {
  QTemporaryDir dir;
  const QString path = WriteDrawing(dir);
  const QImage source(path);
  WarpCache cache;
  for (int i = 0; i < static_cast<int>(kMaxWarpEntries) + 20; ++i) {
    cache.Bent(path, source, kGrid, Pushed(1.0 + i * 0.25));
  }
  QCOMPARE(cache.size(), kMaxWarpEntries);
  QVERIFY(cache.bytes() <= kWarpCacheBytes);
}

void WarpCacheTests::ASmallBendIsHalfTheSizeAndKeptApart() {
  QTemporaryDir dir;
  const QString path = WriteDrawing(dir);
  const QImage source(path);
  WarpCache cache;
  const std::vector<QPointF> points = Pushed(6.0);
  const WarpedImage whole = cache.Bent(path, source, kGrid, points);
  const WarpedImage half = cache.Bent(path, source, kGrid, points, 2);
  QCOMPARE(whole.detail, 1.0);
  QCOMPARE(half.detail, 0.5);
  QCOMPARE(cache.size(), size_t{2});
  QVERIFY(qAbs(half.image.width() * 2 - whole.image.width()) <= 3);
  QVERIFY(qAbs(half.origin.x() - whole.origin.x()) <= 2.0);
}

}  // namespace snapper

QTEST_MAIN(snapper::WarpCacheTests)
#include "warp_cache_tests.moc"
