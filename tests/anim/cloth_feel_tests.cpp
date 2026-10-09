#include <QTest>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>

#include "anim/links.h"
#include "anim/pose_memo.h"
#include "anim/sampler.h"

namespace snapper {
namespace {

constexpr LayerId kLayer(1);

// A 1000 px piece with a grid of cells by cells; the one grid point is
// a physics node with the default settings. When it moves, the piece
// is keyed 200 px right over three frames and stays.
Project OneDot(int cells, int point, bool moves, DragNode node = {}) {
  Doll doll;
  doll.art.pieces = {{"body", {"b.png"}, 0, {0, 0}, {1000, 1000}}};
  doll.rig.pieces = {{"body", "", {500, 0}, 0, -1, {cells, cells}}};
  node.point = point;
  node.physics = true;
  doll.rig.pieces[0].drag_nodes.push_back(node);

  Layer layer;
  layer.id = kLayer;
  DollLayer content{"Doll", {}, false};
  if (moves) {
    PiecePose gone;
    gone.offset = QPointF(200, 0);
    SetKey(&content.pieces["body"], {Frame(0), PiecePose(), Ease::kLinear});
    SetKey(&content.pieces["body"], {Frame(3), gone, Ease::kLinear});
  }
  layer.content = content;
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {layer};
  Project project;
  project.dolls["Doll"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  assert(project.shots.size() == 1);
  assert(project.dolls.size() == 1);
  return project;
}

// How far the dot shows from where the body carries it, as the stage
// asks for it: a link solver over the shared memo.
QPointF Shown(const Project& project, PoseMemo* memo, int frame, int point) {
  assert(memo != nullptr);
  assert(frame >= 0);
  const Shot& shot = *project.shots[0];
  LinkSolver solver(project, shot, memo);
  const PoseMap poses = solver.Poses(*project.dolls.at("Doll"),
                                     shot.layers[0], Frame(frame));
  const auto& warp = poses.at("body").warp;
  assert(point >= 0);
  const bool is_on_grid = warp.size() > static_cast<size_t>(point);
  return is_on_grid ? warp[static_cast<size_t>(point)] : QPointF();
}

}  // namespace

class ClothFeelTests final : public QObject {
  Q_OBJECT

 private slots:
  void ALoneDotLagsThenOvershootsThenSettles_data();
  void ALoneDotLagsThenOvershootsThenSettles();
  void ALoneDotSagsOnAStillDollAndBounces();
  void MaxedSettingsSwingFurtherThanDefaults();
  void ScrubbingBackGivesTheSameDot();
};

void ClothFeelTests::ALoneDotLagsThenOvershootsThenSettles_data() {
  assert(kLayer.value() == 1);
  assert(kLayer.value() > 0);
  QTest::addColumn<int>("cells");
  QTest::addColumn<int>("point");
  QTest::newRow("4x4 middle") << 4 << 12;
  QTest::newRow("8x8 middle") << 8 << 40;
  QTest::newRow("3x3 inner") << 3 << 5;
}

void ClothFeelTests::ALoneDotLagsThenOvershootsThenSettles() {
  QFETCH(int, cells);
  QFETCH(int, point);
  const Project project = OneDot(cells, point, true);
  PoseMemo memo;
  const double mid = Shown(project, &memo, 2, point).x();
  QVERIFY2(mid < -40.0, qPrintable(QString::number(mid)));
  double front = -1e9;
  for (int frame = 3; frame <= 60; ++frame) {
    front = std::max(front, Shown(project, &memo, frame, point).x());
  }
  QVERIFY2(front > 15.0, qPrintable(QString::number(front)));
  const QPointF end = Shown(project, &memo, 300, point);
  QVERIFY2(std::abs(end.x()) < 1.0, qPrintable(QString::number(end.x())));
}

void ClothFeelTests::ALoneDotSagsOnAStillDollAndBounces() {
  const Project project = OneDot(4, 12, false);
  PoseMemo memo;
  QCOMPARE(Shown(project, &memo, 0, 12), QPointF());
  double lowest = 0.0;
  for (int frame = 1; frame <= 48; ++frame) {
    lowest = std::max(lowest, Shown(project, &memo, frame, 12).y());
  }
  QVERIFY2(lowest > 10.0, qPrintable(QString::number(lowest)));
  const double rest = Shown(project, &memo, 300, 12).y();
  QVERIFY2(rest > 5.0, qPrintable(QString::number(rest)));
  QVERIFY2(lowest > rest * 1.05, qPrintable(QString::number(lowest)));
}

void ClothFeelTests::MaxedSettingsSwingFurtherThanDefaults() {
  DragNode wild;
  wild.stretch = 1.0;
  wild.inertia = 1.0;
  wild.air = 0.0;
  wild.gravity = 1.0;
  wild.spring = 1.0;
  double reach[2] = {0.0, 0.0};
  for (int i = 0; i < 2; ++i) {
    const Project project = OneDot(4, 12, true, i == 0 ? DragNode() : wild);
    PoseMemo memo;
    for (int frame = 1; frame <= 60; ++frame) {
      const QPointF at = Shown(project, &memo, frame, 12);
      reach[i] = std::max(reach[i], std::hypot(at.x(), at.y()));
    }
  }
  QVERIFY2(reach[1] > reach[0] * 1.2, qPrintable(QString::number(reach[1])));
  QVERIFY2(reach[1] > 100.0, qPrintable(QString::number(reach[1])));
}

void ClothFeelTests::ScrubbingBackGivesTheSameDot() {
  const Project project = OneDot(4, 12, true);
  PoseMemo memo;
  const QPointF late = Shown(project, &memo, 30, 12);
  const QPointF early = Shown(project, &memo, 5, 12);
  PoseMemo fresh;
  QCOMPARE(Shown(project, &fresh, 5, 12), early);
  QCOMPARE(Shown(project, &fresh, 30, 12), late);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ClothFeelTests)
#include "cloth_feel_tests.moc"
