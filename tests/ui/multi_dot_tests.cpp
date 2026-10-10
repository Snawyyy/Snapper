#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QTest>

#include "bench.h"
#include "inspector_stage.h"
#include "ui/drag_box.h"

namespace snapper {
namespace {

void Type(QDoubleSpinBox* box, double value) {
  assert(box != nullptr);
  assert(value == value);
  box->setValue(value);
  emit box->editingFinished();
}

}  // namespace

class MultiDotTests final : public QObject {
  Q_OBJECT

 private slots:
  void ManyPickedDotsEditAtOnceAndShowMixed();
  void TypedMathWorksOnEachPickedDotsOwnNumber();
};

void MultiDotTests::ManyPickedDotsEditAtOnceAndShowMixed() {
  Bench bench;
  Stage(&bench);
  QVERIFY(bench.rig.SetWarpGrid("Bob", "head", {2, 2}).has_value());
  const std::vector<DotRef> three = {
      {"Bob", "head", 0}, {"Bob", "head", 1}, {"Bob", "head", 2}};
  QVERIFY(bench.rig.SetDragNodes(three, true).has_value());
  QVERIFY(bench.rig.SetDragField({three[0]}, DragField::kBounce, 0.2)
              .has_value());
  DragBox box(bench.All());
  auto* bounce = box.findChild<QDoubleSpinBox*>("bounce");
  auto* drags = box.findChild<QCheckBox*>("drags");
  auto* physics = box.findChild<QCheckBox*>("physics");
  bench.selection.PickDots({{LayerId(1), "head", 0}, {LayerId(1), "head", 1},
                            {LayerId(1), "head", 4}},
                           PickMode::kReplace);
  // Two drag nodes and a plain dot: the tick is partly on, and the
  // two nodes' bounces differ.
  QCOMPARE(drags->checkState(), Qt::PartiallyChecked);
  QCOMPARE(bounce->findChild<QLineEdit*>()->text(), QString("Mixed"));
  Type(bounce, 0.9);
  QCOMPARE(bench.history.UndoLabel(), QString("Set bounce on 2 dots"));
  const auto& nodes =
      bench.history.current().dolls.at("Bob")->rig.pieces[0].drag_nodes;
  QCOMPARE(nodes[0].bounce, 0.9);
  QCOMPARE(nodes[1].bounce, 0.9);
  QCOMPARE(nodes[2].bounce, 0.5);
  QVERIFY(bounce->findChild<QLineEdit*>()->text() != "Mixed");
  // One undo takes the change back for both.
  bench.history.Undo();
  QCOMPARE(bench.history.current().dolls.at("Bob")->rig.pieces[0]
               .drag_nodes[0].bounce, 0.2);
  QCOMPARE(bench.history.current().dolls.at("Bob")->rig.pieces[0]
               .drag_nodes[1].bounce, 0.5);
  // A click on a partly ticked Drags sets every picked dot to drag; on
  // Physics, every drag node among them.
  drags->click();
  QCOMPARE(bench.history.current().dolls.at("Bob")->rig.pieces[0]
               .drag_nodes.size(), size_t{4});
  QCOMPARE(bench.history.UndoLabel(), QString("Drag 3 dots"));
  physics->click();
  QCOMPARE(bench.history.UndoLabel(), QString("Cloth 3 dots"));
  QCOMPARE(physics->checkState(), Qt::Checked);
  physics->click();
  QCOMPARE(physics->checkState(), Qt::Unchecked);
  // Nothing picked: greyed with a reason.
  bench.selection.PickDot(std::nullopt);
  QVERIFY(!bounce->isEnabled());
  QVERIFY(!bounce->toolTip().isEmpty());
}

void MultiDotTests::TypedMathWorksOnEachPickedDotsOwnNumber() {
  Bench bench;
  Stage(&bench);
  QVERIFY(bench.rig.SetWarpGrid("Bob", "head", {2, 2}).has_value());
  const std::vector<DotRef> two = {{"Bob", "head", 0}, {"Bob", "head", 1}};
  QVERIFY(bench.rig.SetDragNodes(two, true).has_value());
  QVERIFY(bench.rig.SetDragField({two[0]}, DragField::kBounce, 0.2)
              .has_value());
  QVERIFY(bench.rig.SetDragField({two[1]}, DragField::kBounce, 0.4)
              .has_value());
  DragBox box(bench.All());
  auto* bounce = box.findChild<QDoubleSpinBox*>("bounce");
  bench.selection.PickDots({{LayerId(1), "head", 0}, {LayerId(1), "head", 1}},
                           PickMode::kReplace);
  const auto enter = [bounce](const QString& text) {
    QLineEdit* edit = bounce->findChild<QLineEdit*>();
    edit->selectAll();
    QTest::keyClicks(edit, text);
    QTest::keyClick(edit, Qt::Key_Return);
  };
  const auto bounces = [&bench] {
    const auto& nodes =
        bench.history.current().dolls.at("Bob")->rig.pieces[0].drag_nodes;
    return std::pair(nodes[0].bounce, nodes[1].bounce);
  };
  // "*2" doubles each; they keep their difference in ratio.
  enter("*2");
  QCOMPARE(bounces().first, 0.4);
  QCOMPARE(bounces().second, 0.8);
  QCOMPARE(bench.history.UndoLabel(), QString("Set bounce on 2 dots"));
  enter("+0.1");
  QCOMPARE(bounces().first, 0.5);
  QCOMPARE(bounces().second, 0.9);
  // A plain number sets both to the same.
  enter("0.3");
  QCOMPARE(bounces().first, 0.3);
  QCOMPARE(bounces().second, 0.3);
  // Each was one step.
  bench.history.Undo();
  QCOMPARE(bounces().first, 0.5);
  QCOMPARE(bounces().second, 0.9);
}

}  // namespace snapper

QTEST_MAIN(snapper::MultiDotTests)
#include "multi_dot_tests.moc"
