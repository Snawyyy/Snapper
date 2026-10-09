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
  ticks[0]->click();
  QCOMPARE(FindRig(BobRig(bench), "body")->collider.spared,
           (std::vector<QString>{"arm"}));
  QCOMPARE(bench.history.UndoLabel(), QString("body spares arm"));
  // The arm is drawn in front of the body, not the other way round.
  panel.PickPiece("arm");
  QVERIFY(!box->isVisibleTo(&panel));
}

}  // namespace snapper

QTEST_MAIN(snapper::ColliderUiTests)
#include "collider_ui_tests.moc"
