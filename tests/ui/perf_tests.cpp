#include <QApplication>
#include <QDir>
#include <QImage>
#include <QTest>

#include <cassert>
#include <memory>

#include "bench.h"
#include "ui/main_window.h"
#include "ui/stage_view.h"

namespace snapper {
namespace {

constexpr int kPictures = 12;
constexpr int kPictureSide = 500;
constexpr int kMoves = 60;
constexpr int kHoverRun = 8;

// A shot of big pictures stacked over each other, like a busy scene.
void StageBigScene(Bench* bench) {
  assert(bench != nullptr);
  QImage art(kPictureSide, kPictureSide, QImage::Format_ARGB32);
  art.fill(QColor(200, 40, 60, 160));
  const QString path = QDir(bench->dir.path()).filePath("big.png");
  const bool is_saved = art.save(path);
  assert(is_saved);
  Shot shot;
  shot.id = ShotId(1);
  shot.length = Frame(240);
  for (int i = 0; i < kPictures; ++i) {
    Layer layer;
    layer.id = LayerId(i + 1);
    layer.name = QStringLiteral("big%1").arg(i);
    layer.content = ImageLayer{path};
    LayerPose place;
    place.offset = QPointF(i * 10.0, i * 5.0);
    SetKey(&layer.transform, {Frame(0), place, Ease::kLinear});
    place.rotation = 30.0;
    SetKey(&layer.transform, {Frame(48), place, Ease::kLinear});
    shot.layers.push_back(layer);
  }
  Project project;
  project.canvas = {1080, 1080};
  project.next_layer_id = kPictures + 1;
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

// A shown window over the big scene, with a counter on its stage.
struct Scene final {
  Bench bench;
  std::unique_ptr<MainWindow> window;
  StageView* stage = nullptr;
  std::unique_ptr<PaintCounter> counter;

  bool Open() {
    // SNAPPER_PERF_PROJECT=<file> measures a real project instead.
    const QString real = qEnvironmentVariable("SNAPPER_PERF_PROJECT");
    const bool is_real = !real.isEmpty();
    if (is_real) {
      assert(bench.document.Open(real).has_value());
    } else {
      StageBigScene(&bench);
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

}  // namespace

class PerfTests final : public QObject {
  Q_OBJECT

 private slots:
  void IdleWindowPaintsNothing();
  void HoverMovesPaintLittle();
  void ADragIsOneUndoStep();
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

void PerfTests::HoverMovesPaintLittle() {
  Scene scene;
  QVERIFY(scene.Open());
  QTest::qWait(100);
  scene.counter->Reset();
  const QPoint middle(scene.stage->width() / 2, scene.stage->height() / 2);
  for (int i = 0; i < kHoverRun; ++i) {
    QTest::mouseMove(scene.stage, middle + QPoint(i * 3, i));
    QCoreApplication::processEvents();
  }
  QVERIFY(scene.counter->paints() < 5 * kHoverRun);
}

void PerfTests::ADragIsOneUndoStep() {
  Scene scene;
  QVERIFY(scene.Open());
  const QPoint middle(scene.stage->width() / 2, scene.stage->height() / 2);
  QTest::mousePress(scene.stage, Qt::LeftButton, {}, middle);
  for (int i = 0; i < kMoves; ++i) {
    QTest::mouseMove(scene.stage, middle + QPoint(i, i));
    QCoreApplication::processEvents();
  }
  QTest::mouseRelease(scene.stage, Qt::LeftButton, {}, middle + QPoint(9, 9));
  QVERIFY(scene.bench.history.CanUndo());
  scene.bench.history.Undo();
  QVERIFY(!scene.bench.history.CanUndo());
}

}  // namespace snapper

QTEST_MAIN(snapper::PerfTests)
#include "perf_tests.moc"
