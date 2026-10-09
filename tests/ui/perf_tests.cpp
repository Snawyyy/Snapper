#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QTest>

#include <cassert>
#include <memory>
#include <vector>

#include "bench.h"
#include "ui/main_window.h"
#include "ui/stage_view.h"

namespace snapper {
namespace {

constexpr int kPieces = 12;
constexpr int kPieceSide = 500;
constexpr int kMoves = 60;
constexpr int kScrubFrames = 30;
// Generous: the debug build on a slow machine must pass, while the
// time before the warp cache was several times this.
constexpr qint64 kMovesBudgetMs = 3000;

// A warp grid whose every point is moved a little off its place.
std::vector<QPointF> RestOffsets(WarpGrid grid, double sway) {
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
    StageBigDoll(&bench, is_animated);
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

}  // namespace

class PerfTests final : public QObject {
  Q_OBJECT

 private slots:
  void IdleWindowPaintsNothing();
  void HoverMovesDoNotRebendTheDoll();
  void DragMovesDoNotRebendTheDoll();
  void ARepaintOfAnUnchangedStageIsQuick();
  void ScrubbingAWarpAnimationHolds30Fps();
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
  QVERIFY(scene.Open());
  QElapsedTimer clock;
  clock.start();
  for (int i = 0; i < kMoves; ++i) {
    QTest::mouseMove(scene.stage, QPoint(200 + i * 5, 300 + i));
    QCoreApplication::processEvents();
  }
  const bool is_quick = clock.elapsed() < kMovesBudgetMs;
  QVERIFY2(is_quick, qPrintable(QString::number(clock.elapsed())));
  QVERIFY(scene.counter->paints() < kMoves);
}

void PerfTests::DragMovesDoNotRebendTheDoll() {
  Scene scene;
  QVERIFY(scene.Open());
  const QPoint middle(scene.stage->width() / 2, scene.stage->height() / 2);
  QElapsedTimer clock;
  clock.start();
  QTest::mousePress(scene.stage, Qt::LeftButton, {}, middle);
  for (int i = 0; i < kMoves; ++i) {
    QTest::mouseMove(scene.stage, middle + QPoint(i, i));
    QCoreApplication::processEvents();
  }
  QTest::mouseRelease(scene.stage, Qt::LeftButton, {}, middle + QPoint(9, 9));
  const bool is_quick = clock.elapsed() < kMovesBudgetMs;
  QVERIFY2(is_quick, qPrintable(QString::number(clock.elapsed())));
  QVERIFY(scene.bench.history.CanUndo());
}

void PerfTests::ARepaintOfAnUnchangedStageIsQuick() {
  Scene scene;
  QVERIFY(scene.Open());
  scene.stage->repaint();
  QElapsedTimer clock;
  clock.start();
  for (int i = 0; i < 10; ++i) {
    scene.stage->repaint();
  }
  const bool is_quick = clock.elapsed() < kMovesBudgetMs / 10;
  QVERIFY2(is_quick, qPrintable(QString::number(clock.elapsed())));
  QVERIFY(scene.counter->paints() >= 10);
}

void PerfTests::ScrubbingAWarpAnimationHolds30Fps() {
  Scene scene;
  QVERIFY(scene.Open(true));
  scene.stage->repaint();
  QElapsedTimer clock;
  clock.start();
  for (int i = 1; i <= kScrubFrames; ++i) {
    scene.bench.playback.Seek(Frame(i));
    scene.stage->repaint();
  }
  const bool is_30_fps = clock.elapsed() < kScrubFrames * 1000 / 30;
  QVERIFY2(is_30_fps, qPrintable(QString::number(clock.elapsed())));
  QCOMPARE(scene.bench.playback.frame(), Frame(kScrubFrames));
}

}  // namespace snapper

QTEST_MAIN(snapper::PerfTests)
#include "perf_tests.moc"
