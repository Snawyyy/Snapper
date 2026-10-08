#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include "anim/doll_pose.h"
#include "bench.h"
#include "ui/rig_canvas.h"
#include "ui/rig_panel.h"

namespace snapper {
namespace {

// An arm hanging off a body, with drawings on disk.
void Stage(Bench* bench) {
  QImage grey(20, 20, QImage::Format_ARGB32);
  grey.fill(Qt::gray);
  QVERIFY(grey.save(QDir(bench->dir.path()).filePath("p.png")));
  Doll doll;
  doll.folder = bench->dir.path();
  doll.art.canvas = QSize(100, 100);
  doll.art.pieces = {{"body", {"p.png"}, 0, {-10, -10}, {20, 20}},
                     {"arm", {"p.png"}, 0, {10, -10}, {20, 20}}};
  doll.rig.pieces = {{"body", "", {10, 10}, 0, -1, {}},
                     {"arm", "", {0, 10}, 1, -1, {}}};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  bench->history.Reset(project);
}

const Rig& BobRig(const Bench& bench) {
  return FindDoll(bench.history.current(), "Bob")->rig;
}

QPushButton* Button(QWidget* parent, const QString& text) {
  for (QPushButton* button : parent->findChildren<QPushButton*>()) {
    const bool is_match = button->text() == text;
    if (is_match) {
      return button;
    }
  }
  return nullptr;
}

}  // namespace

class RigUiTests final : public QObject {
  Q_OBJECT

 private slots:
  void PanelPicksTheDollAndEditsAPiece();
  void ChainsComeFromThePickedPiece();
  void DraggingAJointMovesThePivot();
  void DoubleClickAJointThenClickItsParent();
  void WarpSwitchesOnAtThreeByThree();
  void KeepShapeTicksOn();
  void AnimationWaitsForAGrid();
  void ShiftPickedJointsDragTogether();
};

void RigUiTests::PanelPicksTheDollAndEditsAPiece() {
  Bench bench;
  Stage(&bench);
  RigPanel panel(bench.All());
  QCOMPARE(panel.doll(), QString("Bob"));
  panel.PickPiece("arm");
  auto* parent = panel.findChildren<QComboBox*>()[1];
  QVERIFY(parent->isEnabled());
  const int body = parent->findText("body");
  QVERIFY(body > 0);
  parent->setCurrentIndex(body);
  emit parent->activated(body);
  QCOMPARE(FindRig(BobRig(bench), "arm")->parent, QString("body"));
  // The arm now shows indented under the body.
  QVERIFY(panel.findChild<QListWidget*>()->item(1)->text().startsWith("  "));
}

void RigUiTests::ChainsComeFromThePickedPiece() {
  Bench bench;
  Stage(&bench);
  RigPanel panel(bench.All());
  panel.PickPiece("arm");
  QPushButton* add = Button(&panel, "Add for piece");
  QVERIFY(!add->isEnabled());
  QVERIFY(bench.rig.SetParent("Bob", "arm", "body").has_value());
  panel.PickPiece("arm");
  QVERIFY(add->isEnabled());
  add->click();
  QCOMPARE(BobRig(bench).chains.size(), size_t{1});
  QCOMPARE(BobRig(bench).chains[0].upper, QString("body"));
  QCOMPARE(BobRig(bench).chains[0].tip, QPointF(20, 10));
}

void RigUiTests::DraggingAJointMovesThePivot() {
  Bench bench;
  Stage(&bench);
  RigCanvas canvas(bench.All());
  canvas.resize(400, 400);
  canvas.SetDoll("Bob");
  canvas.SetPiece("body");
  const QTransform world = canvas.World();
  const QPoint joint = world.map(QPointF(0, 0)).toPoint();
  QTest::mousePress(&canvas, Qt::LeftButton, {}, joint);
  QTest::mouseMove(&canvas, world.map(QPointF(5, 0)).toPoint());
  QTest::mouseRelease(&canvas, Qt::LeftButton, {},
                      world.map(QPointF(5, 0)).toPoint());
  const QPointF pivot = FindRig(BobRig(bench), "body")->pivot;
  QVERIFY(std::abs(pivot.x() - 15.0) < 0.5);
  QVERIFY(std::abs(pivot.y() - 10.0) < 0.5);
  QCOMPARE(bench.history.UndoLabel(), QString("Move pivot of body"));
}

void RigUiTests::DoubleClickAJointThenClickItsParent() {
  Bench bench;
  Stage(&bench);
  RigCanvas canvas(bench.All());
  canvas.resize(400, 400);
  canvas.SetDoll("Bob");
  QSignalSpy hints(&canvas, &RigCanvas::Hint);
  const QTransform world = canvas.World();
  // The arm's joint is at the left edge of its drawing.
  const QPoint arm_joint = world.map(QPointF(10, 0)).toPoint();
  const QPoint body = world.map(QPointF(-5, -5)).toPoint();
  QTest::mouseDClick(&canvas, Qt::LeftButton, {}, arm_joint);
  QVERIFY(!hints.isEmpty() && !hints.last().first().toString().isEmpty());
  QTest::mouseClick(&canvas, Qt::LeftButton, {}, body);
  QCOMPARE(FindRig(BobRig(bench), "arm")->parent, QString("body"));
  QVERIFY(hints.last().first().toString().isEmpty());
  QTest::mouseDClick(&canvas, Qt::LeftButton, {}, arm_joint);
  QTest::keyClick(&canvas, Qt::Key_Escape);
  QTest::mouseClick(&canvas, Qt::LeftButton, {}, body);
  QCOMPARE(bench.history.UndoLabel(), QString("Parent arm"));
  QTest::mouseDClick(&canvas, Qt::LeftButton, {}, arm_joint);
  QTest::mouseClick(&canvas, Qt::LeftButton, {}, QPoint(2, 2));
  QVERIFY(FindRig(BobRig(bench), "arm")->parent.isEmpty());
}

void RigUiTests::WarpSwitchesOnAtThreeByThree() {
  Bench bench;
  Stage(&bench);
  RigPanel panel(bench.All());
  panel.PickPiece("arm");
  auto* on = panel.findChild<QCheckBox*>("warp_on");
  QVERIFY(on->isEnabled());
  on->click();
  QCOMPARE(FindRig(BobRig(bench), "arm")->warp, (WarpGrid{3, 3}));
  on->click();
  QVERIFY(!FindRig(BobRig(bench), "arm")->warp.IsOn());
}

void RigUiTests::KeepShapeTicksOn() {
  Bench bench;
  Stage(&bench);
  RigPanel panel(bench.All());
  auto* keep = panel.findChild<QCheckBox*>("keep_shape");
  QVERIFY(!keep->isEnabled());
  panel.PickPiece("arm");
  QVERIFY(keep->isEnabled());
  keep->click();
  QVERIFY(FindRig(BobRig(bench), "arm")->keeps_shape);
  QVERIFY(keep->isChecked());
}

void RigUiTests::AnimationWaitsForAGrid() {
  Bench bench;
  Stage(&bench);
  RigPanel panel(bench.All());
  panel.PickPiece("arm");
  auto* motion = panel.findChild<QComboBox*>("warp_motion");
  QVERIFY(!motion->isEnabled());
  QVERIFY(!motion->toolTip().isEmpty());
  panel.findChild<QCheckBox*>("warp_on")->click();
  QVERIFY(motion->isEnabled());
  QCOMPARE(motion->count(), kWarpMotionKindCount);
  motion->setCurrentIndex(static_cast<int>(WarpMotionKind::kWave));
  emit motion->activated(motion->currentIndex());
  QCOMPARE(FindRig(BobRig(bench), "arm")->warp_motion.kind,
           WarpMotionKind::kWave);
  auto* anchor = panel.findChild<QComboBox*>("warp_anchor");
  QVERIFY(anchor->isEnabled());
  motion->setCurrentIndex(static_cast<int>(WarpMotionKind::kPulse));
  emit motion->activated(motion->currentIndex());
  QVERIFY(!anchor->isEnabled());
}

void RigUiTests::ShiftPickedJointsDragTogether() {
  Bench bench;
  Stage(&bench);
  RigCanvas canvas(bench.All());
  canvas.resize(400, 400);
  canvas.SetDoll("Bob");
  QSignalSpy picks(&canvas, &RigCanvas::PickChanged);
  const QTransform world = canvas.World();
  QTest::mouseClick(&canvas, Qt::LeftButton, {},
                    world.map(QPointF(-5, -5)).toPoint());
  QTest::mouseClick(&canvas, Qt::LeftButton, Qt::ShiftModifier,
                    world.map(QPointF(25, 5)).toPoint());
  QCOMPARE(picks.last().first().toStringList().size(), 2);
  const QPoint joint = world.map(QPointF(0, 0)).toPoint();
  const QPoint moved = world.map(QPointF(4, 0)).toPoint();
  QTest::mousePress(&canvas, Qt::LeftButton, {}, joint);
  QTest::mouseMove(&canvas, moved);
  QTest::mouseRelease(&canvas, Qt::LeftButton, {}, moved);
  const double body = FindRig(BobRig(bench), "body")->pivot.x();
  const double arm = FindRig(BobRig(bench), "arm")->pivot.x();
  QVERIFY(std::abs(body - 14.0) < 0.6);
  QVERIFY(std::abs(arm - 4.0) < 0.6);
  QCOMPARE(bench.history.UndoLabel(), QString("Move 2 joints"));
}

}  // namespace snapper

QTEST_MAIN(snapper::RigUiTests)
#include "rig_ui_tests.moc"
