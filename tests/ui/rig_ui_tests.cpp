#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include <cassert>

#include "anim/doll_pose.h"
#include "bench.h"
#include "ui/grab.h"
#include "ui/motion.h"
#include "ui/rig_canvas.h"
#include "ui/rig_panel.h"

namespace snapper {
namespace {

// An arm hanging off a body, with drawings on disk.
void Stage(Bench* bench) {
  assert(bench != nullptr);
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
  assert(FindDoll(bench.history.current(), "Bob") != nullptr);
  assert(!bench.dir.path().isEmpty());
  return FindDoll(bench.history.current(), "Bob")->rig;
}

QPushButton* Button(QWidget* parent, const QString& text) {
  assert(parent != nullptr);
  assert(!text.isEmpty());
  for (QPushButton* button : parent->findChildren<QPushButton*>()) {
    const bool is_match = button->text() == text;
    if (is_match) {
      return button;
    }
  }
  return nullptr;
}

// A mouse move with no button down; QTest::mouseMove needs a shown window.
void Hover(QWidget* widget, QPoint at) {
  QMouseEvent move(QEvent::MouseMove, QPointF(at), QPointF(at),
                   Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QVERIFY(QApplication::sendEvent(widget, &move));
  QVERIFY(widget->rect().isValid());
}

}  // namespace

class RigUiTests final : public QObject {
  Q_OBJECT

 private slots:
  void PanelPicksTheDollAndEditsAPiece();
  void ChainsComeFromThePickedPiece();
  void DraggingAJointMovesThePivot();
  void DoubleClickAJointThenClickItsParent();
  void WarpSwitchesOnAsAShapeMesh();
  void KeepShapeTicksOn();
  void ShiftPickedJointsDragTogether();
  void JointsGrabFromTwelvePixelsAndWaitFourToDrag();
  void HoveredJointGrowsAndShowsItsCursor();
  void JointDragSaysHowFarItWent();
  void EscapeSilencesTheJointDrag();
  void JointDragLeavesAGhostOfTheJoint();
  void EscapeRemovesTheJointGhost();
};

void RigUiTests::PanelPicksTheDollAndEditsAPiece() {
  Bench bench;
  Stage(&bench);
  RigPanel panel(bench.All());
  QCOMPARE(panel.doll(), QString("Bob"));
  panel.PickPiece("arm");
  auto* parent = panel.findChildren<QComboBox*>()[0];
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

void RigUiTests::WarpSwitchesOnAsAShapeMesh() {
  Bench bench;
  Stage(&bench);
  RigPanel panel(bench.All());
  panel.PickPiece("arm");
  auto* on = panel.findChild<QCheckBox*>("warp_on");
  QVERIFY(on->isEnabled());
  on->click();
  QVERIFY(FindRig(BobRig(bench), "arm")->warp.IsMesh());
  QCOMPARE(bench.history.UndoLabel(), QString("Shape mesh of arm"));
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

void RigUiTests::JointsGrabFromTwelvePixelsAndWaitFourToDrag() {
  Bench bench;
  Stage(&bench);
  RigCanvas canvas(bench.All());
  canvas.resize(400, 400);
  canvas.SetDoll("Bob");
  const QPoint joint = canvas.World().map(QPointF(0, 0)).toPoint();
  // Just out of reach: the press is on empty space, nothing is grabbed.
  QTest::mousePress(&canvas, Qt::LeftButton, {},
                    joint + QPoint(0, -static_cast<int>(kGrabReach) - 3));
  QTest::mouseRelease(&canvas, Qt::LeftButton, {},
                      joint + QPoint(0, -static_cast<int>(kGrabReach) - 3));
  QVERIFY(!bench.history.CanUndo());
  // Within reach it grabs, but 3 pixels of drag change nothing.
  const QPoint near = joint + QPoint(0, -static_cast<int>(kGrabReach) + 1);
  const QPointF before = FindRig(BobRig(bench), "body")->pivot;
  QTest::mousePress(&canvas, Qt::LeftButton, {}, near);
  QTest::mouseMove(&canvas, near + QPoint(3, 0));
  QCOMPARE(FindRig(BobRig(bench), "body")->pivot, before);
  // Past 4 pixels the joint jumps under the cursor.
  QTest::mouseMove(&canvas, joint + QPoint(10, 0));
  QVERIFY(FindRig(BobRig(bench), "body")->pivot != before);
  QTest::mouseRelease(&canvas, Qt::LeftButton, {}, joint + QPoint(10, 0));
  QCOMPARE(bench.history.UndoLabel(), QString("Move pivot of body"));
}

void RigUiTests::HoveredJointGrowsAndShowsItsCursor() {
  Bench bench;
  Stage(&bench);
  bench.motion.SetMode(MotionMode::kOff);
  RigCanvas canvas(bench.All());
  canvas.resize(400, 400);
  canvas.SetDoll("Bob");
  const QPoint joint = canvas.World().map(QPointF(0, 0)).toPoint();
  Hover(&canvas, joint + QPoint(5, 0));
  QCOMPARE(canvas.cursor().shape(), CursorFor(GrabKind::kPivot));
  Hover(&canvas, QPoint(2, 2));
  QCOMPARE(canvas.cursor().shape(), Qt::ArrowCursor);
  Hover(&canvas, joint + QPoint(5, 0));
  QTest::mousePress(&canvas, Qt::LeftButton, {}, joint + QPoint(5, 0));
  QTest::keyClick(&canvas, Qt::Key_Escape);
  QTest::mouseRelease(&canvas, Qt::LeftButton, {}, joint + QPoint(5, 0));
  QVERIFY(!bench.history.CanUndo());
}

void RigUiTests::JointDragSaysHowFarItWent() {
  Bench bench;
  Stage(&bench);
  bench.motion.SetMode(MotionMode::kOff);
  RigCanvas canvas(bench.All());
  canvas.resize(400, 400);
  canvas.SetDoll("Bob");
  const QPoint joint = canvas.World().map(QPointF(0, 0)).toPoint();
  QVERIFY(!canvas.feedback().is_active());
  QTest::mousePress(&canvas, Qt::LeftButton, {}, joint);
  QTest::mouseMove(&canvas, joint + QPoint(30, -20));
  QVERIFY(canvas.feedback().is_active());
  QCOMPARE(canvas.feedback().text(), QString("+30, -20 px"));
  QVERIFY((canvas.feedback().ghost() - QPointF(joint)).manhattanLength() < 1.0);
  QTest::mouseRelease(&canvas, Qt::LeftButton, {}, joint + QPoint(30, -20));
  QVERIFY(!canvas.feedback().is_active());
  QVERIFY(bench.history.CanUndo());
}

void RigUiTests::JointDragLeavesAGhostOfTheJoint() {
  Bench bench;
  Stage(&bench);
  bench.motion.SetMode(MotionMode::kOff);
  RigCanvas canvas(bench.All());
  canvas.resize(400, 400);
  canvas.SetDoll("Bob");
  const QPoint joint = canvas.World().map(QPointF(0, 0)).toPoint();
  QTest::mousePress(&canvas, Qt::LeftButton, {}, joint);
  QTest::mouseMove(&canvas, joint + QPoint(30, -20));
  QCOMPARE(canvas.feedback().ghost_kind(), GhostKind::kRigHandle);
  QTest::mouseRelease(&canvas, Qt::LeftButton, {}, joint);
  QCOMPARE(canvas.feedback().ghost_kind(), GhostKind::kNone);
}

void RigUiTests::EscapeRemovesTheJointGhost() {
  Bench bench;
  Stage(&bench);
  RigCanvas canvas(bench.All());
  canvas.resize(400, 400);
  canvas.SetDoll("Bob");
  const QPoint joint = canvas.World().map(QPointF(0, 0)).toPoint();
  QTest::mousePress(&canvas, Qt::LeftButton, {}, joint);
  QTest::mouseMove(&canvas, joint + QPoint(30, 0));
  QCOMPARE(canvas.feedback().ghost_kind(), GhostKind::kRigHandle);
  QTest::keyClick(&canvas, Qt::Key_Escape);
  QCOMPARE(canvas.feedback().ghost_kind(), GhostKind::kNone);
}

void RigUiTests::EscapeSilencesTheJointDrag() {
  Bench bench;
  Stage(&bench);
  bench.motion.SetMode(MotionMode::kOff);
  RigCanvas canvas(bench.All());
  canvas.resize(400, 400);
  canvas.SetDoll("Bob");
  const QPoint joint = canvas.World().map(QPointF(0, 0)).toPoint();
  QTest::mousePress(&canvas, Qt::LeftButton, {}, joint);
  QTest::mouseMove(&canvas, joint + QPoint(30, 0));
  QVERIFY(canvas.feedback().is_active());
  QTest::keyClick(&canvas, Qt::Key_Escape);
  QVERIFY(!canvas.feedback().is_active());
  QTest::mouseRelease(&canvas, Qt::LeftButton, {}, joint + QPoint(30, 0));
  QVERIFY(!bench.history.CanUndo());
}

}  // namespace snapper

QTEST_MAIN(snapper::RigUiTests)
#include "rig_ui_tests.moc"
