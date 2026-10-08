#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include <cassert>

#include "render/effects.h"
#include "render/frame_renderer.h"
#include "render/stage_hit.h"
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

// A wide green picture for every video, noting which frame was asked.
class GreenVideos final : public VideoFrames {
 public:
  QImage Picture(const QString& path, Frame frame) override {
    asked = frame;
    asked_path = path;
    QImage picture(50, 25, QImage::Format_RGB32);
    picture.fill(Qt::green);
    return picture;
  }
  Frame asked;
  QString asked_path;
};

// The red doll's shot over a speedpaint, from frame 50 to 98.
Project ShotOverVideo(const QTemporaryDir& dir) {
  Project project = RedDollProject(dir);
  Clip paint;
  paint.id = ClipId(1);
  paint.source = VideoSource{"/takes/paint.mp4", Frame(500)};
  paint.in = Frame(5);
  paint.length = Frame(100);
  Clip shot;
  shot.id = ClipId(2);
  shot.source = ShotSource{ShotId(1)};
  shot.start = Frame(50);
  shot.length = Frame(48);
  assert(project.reel.tracks.size() >= 2);
  PlaceClip(&project.reel.tracks[0], paint);
  PlaceClip(&project.reel.tracks[1], shot);
  assert(project.reel.tracks[1].clips.size() == 1);
  return project;
}

}  // namespace

class RenderTests final : public QObject {
  Q_OBJECT

 private slots:
  void DollsDrawWhereKritaPutThem();
  void LayerMovesAndCameraZooms();
  void EffectsChangeWhatIsBelow();
  void SwipesPushTheShotOff();
  void PastTheEndIsBlack();
  void ReelDrawsTracksBottomFirst();
  void ReelSkipsRemovedShots();
  void WarpStretchesTheDrawing();
  void TextDrawsCentred();
  void MissingImagesDrawNothing();
  void ClicksPickWhatIsSeen();
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

void RenderTests::ReelDrawsTracksBottomFirst() {
  QTemporaryDir dir;
  const Project project = ShotOverVideo(dir);
  FrameRenderer renderer;
  GreenVideos videos;
  // The video fits the frame, keeping its shape, from its in point.
  const QImage paint = renderer.RenderReel(project, Frame(10), 1.0, &videos);
  QCOMPARE(videos.asked, Frame(15));
  QCOMPARE(videos.asked_path, QString("/takes/paint.mp4"));
  QCOMPARE(At(paint, 50, 50), QColor(Qt::green));
  QCOMPARE(At(paint, 50, 10), QColor(Qt::black));
  // The shot above covers it while it plays.
  const QImage shot = renderer.RenderReel(project, Frame(60), 1.0, &videos);
  QCOMPARE(At(shot, 50, 50), QColor(Qt::red));
  QCOMPARE(At(shot, 5, 5), QColor(Qt::white));
  const QImage after = renderer.RenderReel(project, Frame(98), 1.0, &videos);
  QCOMPARE(At(after, 50, 50), QColor(Qt::green));
  const QImage past = renderer.RenderReel(project, Frame(300), 1.0, &videos);
  QCOMPARE(At(past, 50, 50), QColor(Qt::black));
  const QImage blind = renderer.RenderReel(project, Frame(10), 1.0, nullptr);
  QCOMPARE(At(blind, 50, 50), QColor(Qt::black));
}

void RenderTests::ReelSkipsRemovedShots() {
  QTemporaryDir dir;
  Project project = ShotOverVideo(dir);
  project.shots.clear();
  FrameRenderer renderer;
  GreenVideos videos;
  // The removed shot's clip shows nothing, so the video below shows.
  const QImage frame = renderer.RenderReel(project, Frame(60), 1.0, &videos);
  QCOMPARE(At(frame, 50, 50), QColor(Qt::green));
  QCOMPARE(videos.asked, Frame(65));
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

void RenderTests::ClicksPickWhatIsSeen() {
  QTemporaryDir dir;
  Project project = RedDollProject(dir);
  // A see-through hole in the middle of the drawing.
  QImage holed(20, 20, QImage::Format_ARGB32);
  holed.fill(Qt::red);
  for (int y = 8; y < 12; ++y) {
    for (int x = 8; x < 12; ++x) {
      holed.setPixelColor(x, y, Qt::transparent);
    }
  }
  QVERIFY(holed.save(QDir(dir.path()).filePath("body.png")));
  Layer words;
  words.id = LayerId(2);
  TextLayer text;
  text.text = "HI";
  text.size = 20.0;
  words.content = text;
  PiecePose low;
  low.offset = QPointF(0, 35);
  SetKey(&words.transform, {Frame(0), low, Ease::kStep});
  FirstShot(&project).layers.push_back(words);
  ImageCache cache;
  const Shot& shot = *project.shots[0];
  const auto body = HitTest(project, shot, Frame(0), {45, 45}, 1.0, &cache);
  QVERIFY(body.has_value());
  QCOMPARE(body->layer, LayerId(1));
  QCOMPARE(body->piece, QString("body"));
  QVERIFY(!HitTest(project, shot, Frame(0), {50, 50}, 1.0, &cache));
  QVERIFY(!HitTest(project, shot, Frame(0), {5, 5}, 1.0, &cache));
  const auto label = HitTest(project, shot, Frame(0), {50, 85}, 1.0, &cache);
  QVERIFY(label.has_value());
  QCOMPARE(label->layer, LayerId(2));
  QVERIFY(label->piece.isEmpty());
  const auto half = HitTest(project, shot, Frame(0), {22, 22}, 0.5, &cache);
  QVERIFY(half.has_value());
}

}  // namespace snapper

QTEST_MAIN(snapper::RenderTests)
#include "render_tests.moc"
