#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QTest>

#include <cassert>
#include <set>

#include "bench.h"
#include "ui/collider_box.h"
#include "ui/rig_canvas.h"
#include "ui/rig_panel.h"

namespace snapper {
namespace {

// A body with an arm drawn in front of it, with drawings on disk.
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

}  // namespace

class ColliderUiTests final : public QObject {
  Q_OBJECT

 private slots:
  void MakingAColliderShowsItsDotsOnlyWhilePicked();
  void ColliderDotsPickDeleteAndUndoInOneStep();
  void TheColliderBoxListsPiecesInFront();
  void ABoxPicksColliderDotsAndEscapeCancelsIt();
  void MirrorCommandsActOrSayWhyNot();
};

void ColliderUiTests::MakingAColliderShowsItsDotsOnlyWhilePicked() {
  Bench bench;
  Stage(&bench);
  RigCanvas canvas(bench.All());
  canvas.resize(400, 400);
  canvas.SetDoll("Bob");
  canvas.MakeCollider("body");
  QCOMPARE(bench.history.UndoLabel(), QString("Make collider of body"));
  const Collider& made = FindRig(BobRig(bench), "body")->collider;
  QVERIFY(made.IsOn());
  const QPoint corner =
      canvas.World().map(QPointF(-10, -10) + made.dots[0].at).toPoint();
  // The piece is picked, so its first dot takes a click.
  QTest::mouseClick(&canvas, Qt::LeftButton, {}, corner);
  QCOMPARE(canvas.picked_dots(), (std::set<QString>{"body:0"}));
  // Unpicked, its dots hide and take no click.
  canvas.SetPick({}, QString());
  QTest::mouseClick(&canvas, Qt::LeftButton, {}, corner);
  QVERIFY(canvas.picked_dots().empty());
  QVERIFY(!made.dots.empty());
}

void ColliderUiTests::ColliderDotsPickDeleteAndUndoInOneStep() {
  Bench bench;
  Stage(&bench);
  RigCanvas canvas(bench.All());
  canvas.resize(400, 400);
  canvas.SetDoll("Bob");
  canvas.MakeCollider("body");
  const size_t all = FindRig(BobRig(bench), "body")->collider.dots.size();
  const QTransform world = canvas.World();
  const auto dot_spot = [&](int slot) {
    return world.map(QPointF(-10, -10) +
                     FindRig(BobRig(bench), "body")->collider.dots
                         [static_cast<size_t>(slot)].at).toPoint();
  };
  QTest::mouseClick(&canvas, Qt::LeftButton, {}, dot_spot(1));
  QTest::mouseClick(&canvas, Qt::LeftButton, Qt::ShiftModifier, dot_spot(2));
  QCOMPARE(canvas.picked_dots().size(), size_t{2});
  // Escape lets go of the dots before the piece.
  QTest::keyClick(&canvas, Qt::Key_Escape);
  QVERIFY(canvas.picked_dots().empty());
  QTest::mouseClick(&canvas, Qt::LeftButton, {}, dot_spot(1));
  QTest::mouseClick(&canvas, Qt::LeftButton, Qt::ShiftModifier, dot_spot(2));
  QTest::keyClick(&canvas, Qt::Key_Delete);
  QCOMPARE(FindRig(BobRig(bench), "body")->collider.dots.size(), all - 2);
  QVERIFY(canvas.picked_dots().empty());
  QCOMPARE(bench.history.UndoLabel(), QString("Delete collider dots"));
  bench.history.Undo();
  QCOMPARE(FindRig(BobRig(bench), "body")->collider.dots.size(), all);
}

void ColliderUiTests::TheColliderBoxListsPiecesInFront() {
  Bench bench;
  Stage(&bench);
  RigPanel panel(bench.All());
  auto* box = panel.findChild<ColliderBox*>("collider_box");
  panel.PickPiece("body");
  QVERIFY(!box->isVisibleTo(&panel));
  QImage solid(20, 20, QImage::Format_ARGB32);
  solid.fill(Qt::gray);
  QVERIFY(bench.rig.MakeCollider("Bob", "body", solid).has_value());
  panel.Refresh();
  QVERIFY(box->isVisibleTo(&panel));
  QList<QCheckBox*> ticks;
  for (QCheckBox* tick : box->findChildren<QCheckBox*>()) {
    const bool is_shown = tick->isVisibleTo(box);
    if (is_shown) {
      ticks.push_back(tick);
    }
  }
  QCOMPARE(ticks.size(), 1);
  QCOMPARE(ticks[0]->text(), QString("arm"));
  QVERIFY(ticks[0]->isChecked());
  // The arm has no warp grid yet: nothing of it can be pushed, and the
  // tick says so.
  QVERIFY(!ticks[0]->isEnabled());
  QVERIFY(ticks[0]->toolTip().contains("no warp grid"));
  QVERIFY(bench.rig.SetWarpGrid("Bob", "arm", {2, 2}).has_value());
  panel.Refresh();
  ticks.clear();
  for (QCheckBox* tick : box->findChildren<QCheckBox*>()) {
    const bool is_shown = tick->isVisibleTo(box);
    if (is_shown) {
      ticks.push_back(tick);
    }
  }
  QCOMPARE(ticks.size(), 1);
  QVERIFY(ticks[0]->isEnabled());
  ticks[0]->click();
  QCOMPARE(FindRig(BobRig(bench), "body")->collider.spared,
           (std::vector<QString>{"arm"}));
  QCOMPARE(bench.history.UndoLabel(), QString("body spares arm"));
  // The arm is drawn in front of the body, not the other way round.
  panel.PickPiece("arm");
  QVERIFY(!box->isVisibleTo(&panel));
}

void ColliderUiTests::ABoxPicksColliderDotsAndEscapeCancelsIt() {
  Bench bench;
  Stage(&bench);
  RigCanvas canvas(bench.All());
  canvas.resize(400, 400);
  canvas.SetDoll("Bob");
  canvas.MakeCollider("body");
  const Collider& made = FindRig(BobRig(bench), "body")->collider;
  const QTransform world = canvas.World();
  const QPointF origin(-10, -10);
  // A box from empty space round the whole piece picks every dot.
  const QPoint from = world.map(QPointF(-45, -45)).toPoint();
  const QPoint to = world.map(QPointF(45, 45)).toPoint();
  QTest::mousePress(&canvas, Qt::LeftButton, {}, from);
  QTest::mouseMove(&canvas, to);
  QTest::mouseRelease(&canvas, Qt::LeftButton, {}, to);
  QCOMPARE(canvas.picked_dots().size(), made.dots.size());
  // Ctrl and a box over the top left corner takes those out.
  const QPoint corner = world.map(origin + QPointF(4, 4)).toPoint();
  QTest::mousePress(&canvas, Qt::LeftButton, Qt::ControlModifier, from);
  QTest::mouseMove(&canvas, corner);
  QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::ControlModifier, corner);
  const size_t left = canvas.picked_dots().size();
  QVERIFY(left < made.dots.size());
  // Escape in the middle of a box changes nothing.
  QTest::mousePress(&canvas, Qt::LeftButton, {}, from);
  QTest::mouseMove(&canvas, to);
  QTest::keyClick(&canvas, Qt::Key_Escape);
  QTest::mouseRelease(&canvas, Qt::LeftButton, {}, to);
  QCOMPARE(canvas.picked_dots().size(), left);
}

void ColliderUiTests::MirrorCommandsActOrSayWhyNot() {
  Bench bench;
  Stage(&bench);
  // Body and arm have no twin: the buttons are greyed with the reason.
  RigPanel panel(bench.All());
  panel.PickPiece("body");
  QPushButton* mirror = nullptr;
  for (QPushButton* button : panel.findChildren<QPushButton*>()) {
    const bool is_mirror = button->text() == "Mirror rig to other side";
    mirror = is_mirror ? button : mirror;
  }
  QVERIFY(mirror != nullptr);
  QVERIFY(!mirror->isEnabled());
  QVERIFY(mirror->toolTip().contains("twin"));
  // A pair: the button acts, and the canvas does the same.
  Project project = bench.history.current();
  Doll doll = *project.dolls.at("Bob");
  doll.art.pieces = {{"arm_l", {"p.png"}, 0, {-30, -10}, {20, 20}},
                     {"arm_r", {"p.png"}, 0, {10, -10}, {20, 20}}};
  doll.rig.pieces = {{"arm_l", "", {0, 0}, 0, -1, {2, 2}},
                     {"arm_r", "", {0, 0}, 0, -1, {2, 2}}};
  doll.rig.pieces[0].drag_nodes = {{0, 0.5, 0.5}};
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  bench.history.Reset(project);
  panel.Refresh();
  panel.PickPiece("arm_l");
  QVERIFY(mirror->isEnabled());
  mirror->click();
  QCOMPARE(FindRig(BobRig(bench), "arm_r")->drag_nodes.size(), size_t{1});
  QCOMPARE(FindRig(BobRig(bench), "arm_r")->drag_nodes[0].point, 2);
  QCOMPARE(bench.history.UndoLabel(), QString("Mirror arm_l to arm_r"));
  bench.history.Undo();
  RigCanvas canvas(bench.All());
  canvas.SetDoll("Bob");
  canvas.MirrorToOtherSide("arm_l");
  QCOMPARE(FindRig(BobRig(bench), "arm_r")->drag_nodes.size(), size_t{1});
}

}  // namespace snapper

QTEST_MAIN(snapper::ColliderUiTests)
#include "collider_ui_tests.moc"
