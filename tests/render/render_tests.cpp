#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include "render/effects.h"
#include "render/frame_renderer.h"
#include "render/warp_raster.h"

namespace snapper {
namespace {

// A 100x100 project with one shot holding a 20x20 red doll in the
// middle, its drawing written into dir.
Project RedDollProject(const QTemporaryDir& dir) {
  QImage red(20, 20, QImage::Format_ARGB32);
  red.fill(Qt::red);
  red.save(QDir(dir.path()).filePath("body.png"));
  Doll doll;
  doll.name = "Dot";
  doll.folder = dir.path();
  doll.art.pieces = {{"body", {"body.png"}, 0, {-10, -10}, {20, 20}}};
  doll.rig.pieces = {{"body", "", {10, 10}, 0, -1, {}}};
  Layer layer;
  layer.id = LayerId(1);
  layer.content = DollLayer{"Dot", {}, false};
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {layer};
  Project project;
  project.canvas = {100, 100};
  project.dolls["Dot"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  return project;
}

Shot& FirstShot(Project* project) {
  auto shot = std::make_shared<Shot>(*project->shots[0]);
  project->shots[0] = shot;
  return *shot;
}

QColor At(const QImage& image, int x, int y) { return image.pixelColor(x, y); }

}  // namespace

class RenderTests final : public QObject {
  Q_OBJECT

 private slots:
  void DollsDrawWhereKritaPutThem();
  void LayerMovesAndCameraZooms();
  void EffectsChangeWhatIsBelow();
  void SwipesPushTheShotOff();
  void PastTheEndIsBlack();
  void WarpStretchesTheDrawing();
  void TextDrawsCentred();
  void MissingImagesDrawNothing();
};

void RenderTests::DollsDrawWhereKritaPutThem() {
  QTemporaryDir dir;
  FrameRenderer renderer;
  const QImage frame =
      renderer.RenderFrame(RedDollProject(dir), Frame(0), 1.0);
  QCOMPARE(frame.size(), QSize(100, 100));
  QCOMPARE(At(frame, 50, 50), QColor(Qt::red));
  QCOMPARE(At(frame, 5, 5), QColor(Qt::white));
  const QImage half = renderer.RenderFrame(RedDollProject(dir), Frame(0), 0.5);
  QCOMPARE(half.size(), QSize(50, 50));
  QCOMPARE(At(half, 25, 25), QColor(Qt::red));
}

void RenderTests::LayerMovesAndCameraZooms() {
  QTemporaryDir dir;
  Project project = RedDollProject(dir);
  Shot& shot = FirstShot(&project);
  PiecePose moved;
  moved.offset = QPointF(30, 0);
  SetKey(&shot.layers[0].transform, {Frame(0), moved, Ease::kStep});
  FrameRenderer renderer;
  QCOMPARE(At(renderer.RenderFrame(project, Frame(0), 1.0), 80, 50),
           QColor(Qt::red));
  shot.layers[0].transform.keys.clear();
  CameraPose zoomed;
  zoomed.zoom = 2.0;
  SetKey(&shot.camera, {Frame(0), zoomed, Ease::kStep});
  QCOMPARE(At(renderer.RenderFrame(project, Frame(0), 1.0), 32, 50),
           QColor(Qt::red));
}

void RenderTests::EffectsChangeWhatIsBelow() {
  QTemporaryDir dir;
  Project project = RedDollProject(dir);
  Layer invert;
  invert.id = LayerId(2);
  invert.content = EffectLayer{EffectKind::kInvert, Qt::white, {}};
  FirstShot(&project).layers.push_back(invert);
  FrameRenderer renderer;
  const QImage frame = renderer.RenderFrame(project, Frame(0), 1.0);
  QCOMPARE(At(frame, 50, 50), QColor(Qt::cyan));
  QCOMPARE(At(frame, 5, 5), QColor(Qt::black));
  QImage a = frame.copy();
  QImage b = frame.copy();
  ApplyEffect(EffectKind::kGlitch, 1.0, Qt::white, 1.0, 7, &a);
  ApplyEffect(EffectKind::kGlitch, 1.0, Qt::white, 1.0, 7, &b);
  QCOMPARE(a, b);
  QVERIFY(a != frame);
  for (const EffectKind kind : {EffectKind::kHalftone, EffectKind::kPosterize,
                                EffectKind::kZoomPunch, EffectKind::kFlash}) {
    QImage changed = frame.copy();
    ApplyEffect(kind, 1.0, Qt::white, 1.0, 0, &changed);
    QVERIFY(changed != frame);
  }
  QImage untouched = frame.copy();
  ApplyEffect(EffectKind::kFill, 0.0, Qt::green, 1.0, 0, &untouched);
  QCOMPARE(untouched, frame);
}

void RenderTests::SwipesPushTheShotOff() {
  Project project;
  project.canvas = {100, 100};
  Shot a;
  a.id = ShotId(1);
  a.length = Frame(10);
  a.background = Qt::red;
  a.transition = {TransitionKind::kSwipeLeft, Frame(3)};
  Shot b = a;
  b.id = ShotId(2);
  b.background = Qt::blue;
  project.shots = {std::make_shared<const Shot>(a),
                   std::make_shared<const Shot>(b)};
  FrameRenderer renderer;
  const QImage mid = renderer.RenderFrame(project, Frame(8), 1.0);
  QCOMPARE(At(mid, 25, 50), QColor(Qt::red));
  QCOMPARE(At(mid, 75, 50), QColor(Qt::blue));
  QCOMPARE(At(renderer.RenderFrame(project, Frame(12), 1.0), 50, 50),
           QColor(Qt::blue));
}

void RenderTests::PastTheEndIsBlack() {
  QTemporaryDir dir;
  FrameRenderer renderer;
  const QImage frame =
      renderer.RenderFrame(RedDollProject(dir), Frame(500), 1.0);
  QCOMPARE(At(frame, 50, 50), QColor(Qt::black));
}

void RenderTests::WarpStretchesTheDrawing() {
  QImage red(10, 10, QImage::Format_ARGB32);
  red.fill(Qt::red);
  const WarpGrid grid{1, 1};
  const std::vector<QPointF> points = {QPointF(0, 0), QPointF(20, 0),
                                       QPointF(0, 10), QPointF(20, 10)};
  const WarpedImage warped = WarpImage(red, grid, points);
  QCOMPARE(warped.origin, QPointF(0, 0));
  QVERIFY(warped.image.width() >= 20);
  QCOMPARE(warped.image.pixelColor(15, 5), QColor(Qt::red));
  QCOMPARE(warped.image.pixelColor(10, 2).alpha(), 255);
  const WarpedImage rest = WarpImage(red, WarpGrid(), {});
  QCOMPARE(rest.image.size(), QSize(10, 10));
}

void RenderTests::TextDrawsCentred() {
  Project project;
  project.canvas = {200, 100};
  Layer words;
  words.id = LayerId(1);
  TextLayer text;
  text.text = "IIII";
  text.size = 40.0;
  text.fill = Qt::black;
  text.outline_width = 0.0;
  words.content = text;
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {words};
  project.shots = {std::make_shared<const Shot>(shot)};
  FrameRenderer renderer;
  const QImage frame = renderer.RenderFrame(project, Frame(0), 1.0);
  int left = 0;
  int right = 0;
  for (int y = 0; y < frame.height(); ++y) {
    for (int x = 0; x < frame.width(); ++x) {
      const bool is_ink = qGray(frame.pixel(x, y)) < 128;
      left += is_ink && x < 100 ? 1 : 0;
      right += is_ink && x >= 100 ? 1 : 0;
    }
  }
  QVERIFY(left > 0 && right > 0);
  QVERIFY(std::abs(left - right) < (left + right) / 3);
}

void RenderTests::MissingImagesDrawNothing() {
  ImageCache cache;
  QVERIFY(cache.Get("/no/such/file.png").isNull());
  QVERIFY(cache.Get("/no/such/file.png").isNull());
  cache.Forget("/no/such/file.png");
  QCOMPARE(cache.bytes(), std::int64_t{0});
}

}  // namespace snapper

QTEST_MAIN(snapper::RenderTests)
#include "render_tests.moc"
