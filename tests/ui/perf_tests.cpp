#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QTest>

#include <cassert>
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "anim/shape_mesh.h"
#include "anim/warp.h"
#include "bench.h"
#include "render/warp_raster.h"
#include "ui/main_window.h"
#include "ui/stage_view.h"

namespace snapper {
namespace {

constexpr int kPieces = 12;
constexpr int kPieceSide = 500;
constexpr int kMoves = 60;
constexpr int kHoverRun = 8;
constexpr int kScrubFrames = 30;
// The frame the animated doll's second key sits on.
constexpr int kTweenEnd = 48;

// A warp grid whose every point is moved a little off its place.
std::vector<QPointF> RestOffsets(const WarpGrid& grid, double sway) {
  assert(grid.IsOn());
  std::vector<QPointF> offsets(static_cast<size_t>(grid.PointCount()));
  for (size_t i = 0; i < offsets.size(); ++i) {
    offsets[i] = QPointF(static_cast<double>(i % 3) + sway, 2.0 + sway);
  }
  assert(!offsets.empty());
  return offsets;
}

// A doll of big warped drawings hung in a chain, like a real one.
void StageBigDoll(Bench* bench, bool is_animated) {
  assert(bench != nullptr);
  Doll doll;
  doll.folder = bench->dir.path();
  for (int i = 0; i < kPieces; ++i) {
    QImage art(kPieceSide, kPieceSide, QImage::Format_ARGB32);
    art.fill(QColor::fromHsv(i * 20, 200, 220, 200));
    const QString file = QStringLiteral("p%1.png").arg(i);
    art.save(QDir(bench->dir.path()).filePath(file));
    const QString name = QStringLiteral("piece%1").arg(i);
    doll.art.pieces.push_back(
        {name, {file}, 0, {-250.0 + i * 8, -250.0 + i * 20}, {500, 500}});
    RigPiece piece;
    piece.name = name;
    piece.parent = i == 0 ? QString() : QStringLiteral("piece%1").arg(i - 1);
    piece.pivot = {250, 250};
    piece.order = i;
    piece.warp = {4, 4};
    doll.rig.pieces.push_back(piece);
  }
  DollLayer posed{"Big", {}, false};
  for (const RigPiece& piece : doll.rig.pieces) {
    PiecePose pose;
    pose.warp = RestOffsets(piece.warp, 0.0);
    SetKey(&posed.pieces[piece.name], {Frame(0), pose, Ease::kLinear});
    pose.warp = RestOffsets(piece.warp, is_animated ? 12.0 : 0.0);
    SetKey(&posed.pieces[piece.name], {Frame(48), pose, Ease::kLinear});
  }
  Layer layer;
  layer.id = LayerId(1);
  layer.content = posed;
  Shot shot;
  shot.id = ShotId(1);
  shot.length = Frame(240);
  shot.layers = {layer};
  Project project;
  project.canvas = {1080, 1080};
  project.dolls["Big"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  bench->history.Reset(project);
  assert(!bench->history.current().shots.empty());
}

// Counts the paint events a widget gets.
class PaintCounter final : public QObject {
 public:
  explicit PaintCounter(QObject* parent) : QObject(parent) {
    assert(parent == nullptr);
    assert(paints_ == 0);
  }
  bool eventFilter(QObject* watched, QEvent* event) override {
    assert(watched != nullptr);
    const bool is_paint = event->type() == QEvent::Paint;
    paints_ += is_paint ? 1 : 0;
    return false;
  }
  int paints() const { return paints_; }
  void Reset() { paints_ = 0; }

 private:
  int paints_ = 0;
};

// A shown window over the big doll, with a counter on its stage.
struct Scene final {
  Bench bench;
  std::unique_ptr<MainWindow> window;
  StageView* stage = nullptr;
  std::unique_ptr<PaintCounter> counter;

  bool Open(bool is_animated = false) {
    // SNAPPER_PERF_PROJECT=<file> measures a real project instead.
    const QString real = qEnvironmentVariable("SNAPPER_PERF_PROJECT");
    const bool is_real = !real.isEmpty();
    if (is_real) {
      assert(bench.document.Open(real).has_value());
    } else {
      StageBigDoll(&bench, is_animated);
    }
    window = std::make_unique<MainWindow>(bench.All());
    window->resize(1600, 900);
    window->show();
    const bool is_shown = QTest::qWaitForWindowExposed(window.get());
    stage = window->findChild<StageView*>();
    assert(stage != nullptr);
    counter = std::make_unique<PaintCounter>(nullptr);
    stage->installEventFilter(counter.get());
    return is_shown;
  }
};

// Nanoseconds one call of work takes.
template <typename Work>
qint64 TimeOf(Work work) {
  QElapsedTimer clock;
  clock.start();
  assert(clock.isValid());
  work();
  const qint64 spent = clock.nsecsElapsed();
  assert(spent >= 0);
  return spent;
}

// A repaint at a frame the stage has not drawn yet, so the doll is
// bent anew: what every bend costs when nothing is kept.
qint64 ColdRepaintNs(Scene* scene, int frame) {
  assert(scene != nullptr);
  assert(frame > 0 && frame < kTweenEnd);
  scene->bench.playback.Seek(Frame(frame));
  return TimeOf([scene] { scene->stage->repaint(); });
}

// The loads of a busy machine hit some samples and not others, so the
// cost of a thing is the quickest of its samples.
template <typename Step>
qint64 QuickestOf(int count, Step step) {
  assert(count < 1000);
  assert(count > 0);
  qint64 quickest = std::numeric_limits<qint64>::max();
  for (int i = 0; i < count; ++i) {
    quickest = std::min(quickest, TimeOf([&step, i] { step(i); }));
  }
  return quickest;
}

// Kept work must be this many times cheaper than doing it again.
constexpr qint64 kCheaperBy = 3;

}  // namespace

class PerfTests final : public QObject {
  Q_OBJECT

 private slots:
  void IdleWindowPaintsNothing();
  void HoverMovesDoNotRebendTheDoll();
  void DragMovesDoNotRebendTheDoll();
  void ARepaintOfAnUnchangedStageIsQuick();
  void ScrubbingAWarpAnimationIsFarCheaperThanFullBends();
  void AShapeMeshBendsAboutAsFastAsAGrid();
};

void PerfTests::IdleWindowPaintsNothing() {
  Scene scene;
  QVERIFY(scene.Open());
  QTest::qWait(100);
  scene.counter->Reset();
  QTest::qWait(400);
  QCOMPARE(scene.counter->paints(), 0);
  QVERIFY(!scene.bench.playback.IsPlaying());
}

void PerfTests::HoverMovesDoNotRebendTheDoll() {
  Scene scene;
  QVERIFY(scene.Open(true));
  const qint64 cold = ColdRepaintNs(&scene, 10);
  // A run of moves across the doll, quickest of a few runs.
  const QPoint middle(scene.stage->width() / 2, scene.stage->height() / 2);
  const qint64 quickest = QuickestOf(5, [&scene, middle](int run) {
    for (int i = 0; i < kHoverRun; ++i) {
      QTest::mouseMove(scene.stage, middle + QPoint(run * 7 + i * 3, i));
      QCoreApplication::processEvents();
    }
  });
  const bool is_cheap = quickest * kCheaperBy < cold;
  QVERIFY2(is_cheap,
           qPrintable(QString("%1 vs %2 ns").arg(quickest).arg(cold)));
  QVERIFY(scene.counter->paints() < 5 * kHoverRun);
}

void PerfTests::DragMovesDoNotRebendTheDoll() {
  Scene scene;
  QVERIFY(scene.Open(true));
  const qint64 cold = ColdRepaintNs(&scene, 10);
  const QPoint middle(scene.stage->width() / 2, scene.stage->height() / 2);
  QTest::mousePress(scene.stage, Qt::LeftButton, {}, middle);
  const qint64 quickest = QuickestOf(kMoves, [&scene, middle](int i) {
    QTest::mouseMove(scene.stage, middle + QPoint(i, i));
    QCoreApplication::processEvents();
  });
  QTest::mouseRelease(scene.stage, Qt::LeftButton, {}, middle + QPoint(9, 9));
  const bool is_cheap = quickest * kCheaperBy < cold;
  QVERIFY2(is_cheap,
           qPrintable(QString("%1 vs %2 ns").arg(quickest).arg(cold)));
  QVERIFY(scene.bench.history.CanUndo());
}

void PerfTests::ARepaintOfAnUnchangedStageIsQuick() {
  Scene scene;
  QVERIFY(scene.Open(true));
  const qint64 cold = ColdRepaintNs(&scene, 10);
  const qint64 quickest =
      QuickestOf(10, [&scene](int) { scene.stage->repaint(); });
  const bool is_cheap = quickest * kCheaperBy < cold;
  QVERIFY2(is_cheap,
           qPrintable(QString("%1 vs %2 ns").arg(quickest).arg(cold)));
  QVERIFY(scene.counter->paints() >= 10);
}

void PerfTests::ScrubbingAWarpAnimationIsFarCheaperThanFullBends() {
  Scene scene;
  QVERIFY(scene.Open(true));
  scene.stage->repaint();
  // What bending the doll whole, as drawn before the bends were made
  // small, costs on this machine right now.
  const QImage art(kPieceSide, kPieceSide, QImage::Format_ARGB32);
  const WarpGrid grid{4, 4};
  const std::vector<QPointF> points = WarpedPoints(
      art.size(), grid, RestOffsets(grid, 6.0));
  const qint64 whole = QuickestOf(3, [&](int) {
    for (int i = 0; i < kPieces; ++i) {
      WarpImage(art, grid, points);
    }
  });
  // Every frame is a new bend, so each scrub step is a miss.
  const qint64 quickest = QuickestOf(kScrubFrames, [&scene](int i) {
    scene.bench.playback.Seek(Frame(i + 1));
    scene.stage->repaint();
  });
  const bool is_cheaper = quickest * 3 < whole * 2;
  QVERIFY2(is_cheaper,
           qPrintable(QString("%1 vs %2 ns").arg(quickest).arg(whole)));
  QCOMPARE(scene.bench.playback.frame(), Frame(kScrubFrames));
}

void PerfTests::AShapeMeshBendsAboutAsFastAsAGrid() {
  QImage art(kPieceSide, kPieceSide, QImage::Format_ARGB32);
  art.fill(QColor(200, 40, 60));
  const WarpGrid grid{4, 4};
  const WarpGrid mesh{0, 0, BuildShapeMesh({art}, MeshDetail::kMany)};
  QVERIFY(mesh.IsMesh());
  const auto bend = [&art](const WarpGrid& warp) {
    const std::vector<QPointF> points =
        WarpedPoints(art.size(), warp, RestOffsets(warp, 6.0));
    return QuickestOf(3, [&](int) { WarpImage(art, warp, points); });
  };
  const qint64 by_grid = bend(grid);
  const qint64 by_mesh = bend(mesh);
  // A mesh draws only where the drawing is, in more, smaller triangles.
  const bool is_quick = by_mesh < by_grid * 2;
  QVERIFY2(is_quick,
           qPrintable(QString("%1 vs %2 ns").arg(by_mesh).arg(by_grid)));
}

}  // namespace snapper

QTEST_MAIN(snapper::PerfTests)
#include "perf_tests.moc"
