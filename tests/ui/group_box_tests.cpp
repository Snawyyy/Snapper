#include <QApplication>
#include <QPushButton>
#include <QTest>

#include <cassert>
#include <set>

#include "bench.h"
#include "inspector_stage.h"
#include "ui/group_box.h"
#include "ui/inspector.h"

namespace snapper {

class GroupBoxTests final : public QObject {
  Q_OBJECT

 private slots:
  void TheButtonIsGreyWithAReasonUntilDotsArePicked();
  void AChipPicksItsDotsAndShowsPressedWhenAllArePicked();
  void ChipsFollowRenamesDeletesAndUndo();
  void TheInspectorShowsGroupsForADoll();
};

namespace {

// Bob's head with a 2x2 grid (nine dots), picked.
struct Panel final {
  Bench bench;
  GroupBox box;

  Panel() : box((Prepare(&bench), bench.All())) {
    assert(box.parent() == nullptr);
  }

  static void Prepare(Bench* bench) {
    assert(bench != nullptr);
    Stage(bench);
    const auto grid = bench->rig.SetWarpGrid("Bob", "head", {2, 2});
    assert(grid.has_value());
  }

  static std::set<WarpDot> Dots(std::vector<int> points) {
    std::set<WarpDot> dots;
    for (const int point : points) {
      dots.insert({LayerId(1), "head", point});
    }
    return dots;
  }

  void Pick(std::vector<int> points) {
    bench.selection.PickDots(Dots(points), PickMode::kReplace);
  }

  std::vector<QPushButton*> Chips() const {
    // Replaced chips go at the next turn of the event loop.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto found = box.findChildren<QPushButton*>("group_chip");
    return {found.begin(), found.end()};
  }

  QPushButton* Make() const {
    return box.findChild<QPushButton*>("make_group");
  }
};

}  // namespace

void GroupBoxTests::TheButtonIsGreyWithAReasonUntilDotsArePicked() {
  Panel panel;
  QVERIFY(!panel.Make()->isEnabled());
  QVERIFY(!panel.Make()->toolTip().isEmpty());
  QVERIFY(panel.box.findChild<QLabel*>("group_hint")->isVisibleTo(&panel.box));
  panel.Pick({1, 2});
  QVERIFY(panel.Make()->isEnabled());
  QVERIFY(panel.Chips().empty());
}

void GroupBoxTests::AChipPicksItsDotsAndShowsPressedWhenAllArePicked() {
  Panel panel;
  panel.Pick({1, 2, 3});
  QVERIFY(panel.bench.groups.Create(kShot, Panel::Dots({1, 2, 3}), "Still")
              .has_value());
  QVERIFY(panel.bench.groups.Create(kShot, Panel::Dots({5, 6}), "Jelly")
              .has_value());
  const auto chips = panel.Chips();
  QCOMPARE(chips.size(), size_t(2));
  QCOMPARE(chips[0]->text(), QString("Still (3)"));
  QVERIFY(chips[0]->isChecked());
  QVERIFY(!chips[1]->isChecked());
  QVERIFY(panel.box.findChild<QLabel*>("group_hint")->isHidden());
  // A click picks the other group's dots instead.
  chips[1]->click();
  QCOMPARE(panel.bench.selection.dots().size(), size_t(2));
  QVERIFY(chips[1]->isChecked());
  QVERIFY(!chips[0]->isChecked());
  // Picking is not an edit: no undo step for it.
  QCOMPARE(panel.bench.history.UndoLabel(), QString("Group 2 dots"));
  // A group picked, the dot panel's material reaches all of it at once.
  panel.bench.selection.PickDots({}, PickMode::kReplace);
  chips[0]->click();
  QVERIFY(panel.bench.rig.SetNodeKindAll(
              {{"Bob", "head", 1}, {"Bob", "head", 2}, {"Bob", "head", 3}},
              NodeKind::kJelly)
              .has_value());
  QVERIFY(chips[0]->isChecked());
}

void GroupBoxTests::ChipsFollowRenamesDeletesAndUndo() {
  Panel panel;
  panel.Pick({1});
  QVERIFY(panel.bench.groups.Create(kShot, Panel::Dots({1})).has_value());
  QCOMPARE(panel.Chips().size(), size_t(1));
  QVERIFY(panel.bench.groups.Rename(kShot, "Group 1", "Hair").has_value());
  QCOMPARE(panel.Chips().size(), size_t(1));
  QCOMPARE(panel.Chips()[0]->text(), QString("Hair (1)"));
  QVERIFY(panel.bench.groups.Delete(kShot, "Hair").has_value());
  QVERIFY(panel.Chips().empty());
  panel.bench.history.Undo();
  QCOMPARE(panel.Chips().size(), size_t(1));
}

void GroupBoxTests::TheInspectorShowsGroupsForADoll() {
  Bench bench;
  Stage(&bench);
  Inspector inspector(bench.All());
  inspector.show();
  const auto* box = inspector.findChild<GroupBox*>();
  QVERIFY(box != nullptr);
  QVERIFY(box->isVisibleTo(&inspector));
  // A text layer is no doll: no groups.
  bench.selection.SelectLayer(LayerId(2));
  QVERIFY(!box->isVisibleTo(&inspector));
}

}  // namespace snapper

QTEST_MAIN(snapper::GroupBoxTests)
#include "group_box_tests.moc"
