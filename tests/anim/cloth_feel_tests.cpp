#include <QTest>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <functional>
#include <memory>

#include "anim/links.h"
#include "anim/pose_memo.h"
#include "anim/sampler.h"

namespace snapper {
namespace {

constexpr LayerId kLayer(1);

// How the doll moves in a test.
enum class Move { kStill, kSlide, kTurned };

// A 1000 px piece with a grid of cells by cells; the one grid point is
// a physics node. Slide keys the doll 200 px right over three frames;
// Turned stands it on its side from the first frame on.
Project OneDot(int cells, int point, Move move, DragNode node = {}) {
  Doll doll;
  doll.art.pieces = {{"body", {"b.png"}, 0, {0, 0}, {1000, 1000}}};
  doll.rig.pieces = {{"body", "", {500, 0}, 0, -1, {cells, cells}}};
  node.point = point;
  node.physics = true;
  doll.rig.pieces[0].drag_nodes.push_back(node);
  Layer layer;
  layer.id = kLayer;
  layer.content = DollLayer{"Doll", {}, false};
  PiecePose pose;
  const bool is_slide = move == Move::kSlide;
  const bool is_turned = move == Move::kTurned;
  if (is_slide) {
    SetKey(&layer.transform, {Frame(0), pose, Ease::kLinear});
    pose.offset = QPointF(200, 0);
    SetKey(&layer.transform, {Frame(3), pose, Ease::kLinear});
  }
  if (is_turned) {
    pose.rotation = 90.0;
    SetKey(&layer.transform, {Frame(0), pose, Ease::kStep});
  }
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

// How far the dot shows from where the body carries it, in the
// drawing, as the stage asks for it: a link solver over the memo.
QPointF Shown(const Project& project, PoseMemo* memo, int frame, int point) {
  assert(memo != nullptr);
  assert(frame >= 0 && point >= 0);
  const Shot& shot = *project.shots[0];
  LinkSolver solver(project, shot, memo);
  const PoseMap poses = solver.Poses(*project.dolls.at("Doll"),
                                     shot.layers[0], Frame(frame));
  const auto& warp = poses.at("body").warp;
  const bool is_on_grid = warp.size() > static_cast<size_t>(point);
  return is_on_grid ? warp[static_cast<size_t>(point)] : QPointF();
}

double Length(QPointF v) { return std::hypot(v.x(), v.y()); }

// The dot of a 4 by 4 grid's middle with one setting at value and the
// rest at their defaults.
DragNode With(double DragNode::*setting, double value) {
  assert(value >= 0.0 && value <= 1.0);
  assert(setting != nullptr);
  DragNode node;
  node.*setting = value;
  return node;
}

// How far the lone dot travels on its own after the slide stops.
double KeepsMoving(const DragNode& node) {
  assert(node.inertia >= 0.0);
  assert(node.air >= 0.0);
  const Project project = OneDot(4, 12, Move::kSlide, node);
  PoseMemo memo;
  double moved = 0.0;
  QPointF last = Shown(project, &memo, 3, 12);
  for (int frame = 4; frame <= 40; ++frame) {
    const QPointF at = Shown(project, &memo, frame, 12);
    moved += Length(at - last);
    last = at;
  }
  return moved;
}

// The most the lone dot strays while the doll slides and just after.
double Swing(const DragNode& node) {
  assert(node.stretch >= 0.0);
  assert(node.inertia >= 0.0);
  const Project project = OneDot(4, 12, Move::kSlide, node);
  PoseMemo memo;
  double most = 0.0;
  for (int frame = 0; frame <= 30; ++frame) {
    most = std::max(most, Length(Shown(project, &memo, frame, 12)));
  }
  return most;
}

// How much the lone dot still moves between frames 20 and 40.
double Ringing(const DragNode& node) {
  assert(node.air >= 0.0);
  assert(node.spring >= 0.0);
  const Project project = OneDot(4, 12, Move::kSlide, node);
  PoseMemo memo;
  double moved = 0.0;
  QPointF last = Shown(project, &memo, 20, 12);
  for (int frame = 21; frame <= 40; ++frame) {
    const QPointF at = Shown(project, &memo, frame, 12);
    moved += Length(at - last);
    last = at;
  }
  return moved;
}

// How far down (in shot space, the doll on its side) the dot hangs.
double Hang(const DragNode& node) {
  assert(node.gravity >= 0.0);
  assert(node.spring >= 0.0);
  const Project project = OneDot(4, 12, Move::kTurned, node);
  PoseMemo memo;
  return Length(Shown(project, &memo, 90, 12));
}

// Checks effect at 0, 0.5 and 1 of setting: none at 0 when is_none, and
// growing (or, with is_falling, shrinking) as it goes up.
void CheckSlider(const char* name, double DragNode::*setting,
                 const std::function<double(const DragNode&)>& effect,
                 bool is_falling, bool is_none) {
  assert(name != nullptr);
  assert(setting != nullptr);
  std::array<double, 3> at{};
  for (size_t i = 0; i < at.size(); ++i) {
    at[i] = effect(With(setting, static_cast<double>(i) / 2.0));
  }
  const QString said = QString("%1: %2 %3 %4")
                           .arg(name)
                           .arg(at[0])
                           .arg(at[1])
                           .arg(at[2]);
  const double step = std::max(at[0], at[2]) * 0.05 + 0.1;
  const bool is_ordered = is_falling ? at[0] > at[1] + step &&
                                           at[1] > at[2] + step
                                     : at[0] + step < at[1] &&
                                           at[1] + step < at[2];
  QVERIFY2(is_ordered, qPrintable(said));
  const bool is_nothing_at_zero = !is_none || at[0] < 0.5;
  QVERIFY2(is_nothing_at_zero, qPrintable(said));
}

}  // namespace

class ClothFeelTests final : public QObject {
  Q_OBJECT

 private slots:
  void ALoneDotLagsThenOvershootsThenSettles_data();
  void ALoneDotLagsThenOvershootsThenSettles();
  void AStillDollStaysAsDrawn();
  void EachSliderDoesOneThing();
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
  const Project project = OneDot(cells, point, Move::kSlide);
  PoseMemo memo;
  // The default dot reads as heavy and jiggly: it lags the slide, then
  // swings past where it belongs, then settles.
  const double mid = Shown(project, &memo, 2, point).x();
  QVERIFY2(mid < -10.0, qPrintable(QString::number(mid)));
  double front = -1e9;
  for (int frame = 3; frame <= 30; ++frame) {
    front = std::max(front, Shown(project, &memo, frame, point).x());
  }
  QVERIFY2(front > 3.0, qPrintable(QString::number(front)));
  const QPointF end = Shown(project, &memo, 120, point);
  QVERIFY2(Length(end) < 0.5, qPrintable(QString::number(end.x())));
}

void ClothFeelTests::AStillDollStaysAsDrawn() {
  DragNode heavy;
  heavy.gravity = 1.0;
  heavy.spring = 0.0;
  heavy.stretch = 1.0;
  const Project project = OneDot(4, 12, Move::kStill, heavy);
  QCOMPARE(project.shots.size(), size_t{1});
  PoseMemo memo;
  // Upright, the drawing already hangs: no sag, no jitter, ever.
  for (int frame : {0, 1, 2, 10, 50, 300}) {
    QCOMPARE(Shown(project, &memo, frame, 12), QPointF());
  }
}

void ClothFeelTests::EachSliderDoesOneThing() {
  // Stretch: how far it swings when the doll slides; none at 0 (it
  // keeps its distance to the dots around it).
  CheckSlider("stretch", &DragNode::stretch, Swing, false, true);
  // Inertia: how much it keeps moving once the doll stops; none at 0
  // (it moves with the doll).
  CheckSlider("inertia", &DragNode::inertia, KeepsMoving, false, true);
  // Air resistance: how much it still rings later on.
  CheckSlider("air", &DragNode::air, Ringing, true, false);
  // Gravity: how far it hangs when the doll is on its side.
  CheckSlider("gravity", &DragNode::gravity, Hang, false, true);
  // Bounce: how firmly it holds its drawn place against gravity.
  CheckSlider("bounce", &DragNode::spring, Hang, true, false);
  QVERIFY(!QTest::currentTestFailed());
  QVERIFY(kLayer.value() == 1);
}

void ClothFeelTests::ScrubbingBackGivesTheSameDot() {
  const Project project = OneDot(4, 12, Move::kSlide);
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
