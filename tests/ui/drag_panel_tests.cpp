#include <QApplication>
#include <QEnterEvent>
#include <QLabel>
#include <QTest>

#include <cassert>
#include <set>

#include "bench.h"
#include "inspector_stage.h"
#include "ui/drag_box.h"
#include "ui/dot_kinds.h"
#include "ui/feel_pad.h"
#include "ui/fold_section.h"
#include "ui/kind_tile.h"

namespace snapper {

class DragPanelTests final : public QObject {
  Q_OBJECT

 private slots:
  void ClickingATileMakesEveryPickedDotThatKindInOneStep();
  void MixedPicksOutlineEveryKindInUse();
  void NothingPickedGreysEverythingWithAReason();
  void PresetChipsSetTheFeelInOneStep();
  void DraggingThePadIsOneUndoStepAndEscapeCancels();
  void ClickingThePadJumpsThePuck();
  void JellyShowsSquishAndClothShowsStretch();
  void TilesLoopOnlyUnderThePointerAndNeverInOff();
  void TheFoldIsClosedAtFirstAndRemembered();
  void TheBlobRunsOnlyWhileShownAndMotionIsOn();
};

namespace {

// Bob's head with a 2x2 grid; five of its dots can be picked.
struct Panel final {
  Bench bench;
  DragBox box;

  Panel() : box((Prepare(&bench), bench.All())) {
    assert(box.parent() == nullptr);
  }

  static void Prepare(Bench* bench) {
    assert(bench != nullptr);
    Stage(bench);
    const auto grid = bench->rig.SetWarpGrid("Bob", "head", {2, 2});
    assert(grid.has_value());
  }

  void Pick(std::vector<int> points) {
    std::set<WarpDot> dots;
    for (const int point : points) {
      dots.insert({LayerId(1), "head", point});
    }
    bench.selection.PickDots(dots, PickMode::kReplace);
  }

  const std::vector<DragNode>& Nodes() const {
    return bench.history.current().dolls.at("Bob")->rig.pieces[0].drag_nodes;
  }
  template <typename T>
  T* Find(const char* name) {
    return box.findChild<T*>(name);
  }
};

}  // namespace

void DragPanelTests::ClickingATileMakesEveryPickedDotThatKindInOneStep() {
  Panel panel;
  panel.Pick({0, 1, 4});
  panel.Find<KindTile>("kind_jelly")->click();
  QCOMPARE(panel.Nodes().size(), size_t{3});
  for (const DragNode& node : panel.Nodes()) {
    QCOMPARE(node.kind, NodeKind::kJelly);
  }
  QCOMPARE(panel.bench.history.UndoLabel(), QString("Jelly 3 dots"));
  QCOMPARE(panel.Find<KindTile>("kind_jelly")->share(), Share::kAll);
  QCOMPARE(panel.Find<KindTile>("kind_still")->share(), Share::kNone);
  QVERIFY(!panel.box.title().contains("Mixed"));
  QVERIFY(panel.box.title().contains("3 picked"));
  // One undo takes all three back.
  panel.bench.history.Undo();
  QVERIFY(panel.Nodes().empty());
  panel.Find<KindTile>("kind_cloth")->click();
  panel.Find<KindTile>("kind_still")->click();
  QVERIFY(panel.Nodes().empty());
  QCOMPARE(panel.Find<KindTile>("kind_still")->share(), Share::kAll);
}

void DragPanelTests::MixedPicksOutlineEveryKindInUse() {
  Panel panel;
  panel.Pick({0});
  panel.Find<KindTile>("kind_drag")->click();
  panel.Pick({0, 1});
  QCOMPARE(panel.Find<KindTile>("kind_drag")->share(), Share::kSome);
  QCOMPARE(panel.Find<KindTile>("kind_still")->share(), Share::kSome);
  QCOMPARE(panel.Find<KindTile>("kind_jelly")->share(), Share::kNone);
  QVERIFY(panel.box.title().contains("Mixed"));
  // Presets are for one kind only.
  QVERIFY(!panel.Find<QWidget>("preset_0")->isVisibleTo(&panel.box));
  // The pad still works on the moving dot.
  QVERIFY(panel.Find<FeelPad>("feel_pad")->isEnabled());
}

void DragPanelTests::NothingPickedGreysEverythingWithAReason() {
  Panel panel;
  for (const char* name :
       {"kind_still", "kind_drag", "kind_cloth", "kind_jelly"}) {
    auto* tile = panel.Find<KindTile>(name);
    QVERIFY(!tile->isEnabled());
    QVERIFY(!tile->toolTip().isEmpty());
  }
  for (const char* name : {"feel_pad", "feel_preview", "reach", "lag",
                           "gravity", "mirror_dots", "fine_tune"}) {
    auto* widget = panel.Find<QWidget>(name);
    QVERIFY2(!widget->isEnabled(), name);
    QVERIFY2(!widget->toolTip().isEmpty(), name);
  }
  QVERIFY(!panel.Find<QWidget>("preset_0")->isVisibleTo(&panel.box));
  // Dots that are all Still: tiles work, the pad says why not.
  panel.Pick({0});
  QVERIFY(panel.Find<KindTile>("kind_cloth")->isEnabled());
  QVERIFY(!panel.Find<FeelPad>("feel_pad")->isEnabled());
  QVERIFY(panel.Find<FeelPad>("feel_pad")->toolTip().contains("Jelly"));
}

void DragPanelTests::PresetChipsSetTheFeelInOneStep() {
  Panel panel;
  panel.Pick({0, 1});
  panel.Find<KindTile>("kind_jelly")->click();
  const auto presets = FeelPresets(NodeKind::kJelly);
  auto* chip = panel.Find<QPushButton>("preset_1");
  QVERIFY(chip->isVisibleTo(&panel.box));
  QCOMPARE(chip->text(), presets[1].name);
  QVERIFY(panel.Find<QPushButton>("preset_2")->isVisibleTo(&panel.box));
  QVERIFY(!panel.Find<QPushButton>("preset_3")->isVisibleTo(&panel.box));
  const DragNode before = panel.Nodes()[0];
  chip->click();
  for (const DragNode& node : panel.Nodes()) {
    QVERIFY(SameFeel(FeelOf(node), presets[1].feel));
  }
  QCOMPARE(panel.bench.history.UndoLabel(), RigManager::FeelLabel(2));
  QVERIFY(chip->isChecked());
  // Clicking the lit chip again keeps it lit.
  chip->click();
  QVERIFY(chip->isChecked());
  panel.bench.history.Undo();
  panel.bench.history.Undo();
  QCOMPARE(panel.Nodes()[0], before);
}

void DragPanelTests::DraggingThePadIsOneUndoStepAndEscapeCancels() {
  Panel panel;
  panel.Pick({0, 1});
  panel.Find<KindTile>("kind_cloth")->click();
  auto* pad = panel.Find<FeelPad>("feel_pad");
  const QRectF area = pad->Area();
  const std::vector<DragNode> before = panel.Nodes();
  const auto at = [&area](double firm, double heavy) {
    return PointOf({firm, heavy}, area).toPoint();
  };
  QTest::mousePress(pad, Qt::LeftButton, Qt::NoModifier, at(0.2, 0.2));
  QVERIFY(pad->is_dragging());
  QTest::mouseMove(pad, at(0.5, 0.5));
  QTest::mouseMove(pad, at(0.9, 0.8));
  QTest::mouseRelease(pad, Qt::LeftButton, Qt::NoModifier, at(0.9, 0.8));
  QVERIFY(!pad->is_dragging());
  const Feel landed = FeelOf(panel.Nodes()[0]);
  QVERIFY(std::abs(landed.firm - 0.9) < 0.03);
  QVERIFY(std::abs(landed.heavy - 0.8) < 0.03);
  QCOMPARE(panel.bench.history.UndoLabel(), RigManager::FeelLabel(2));
  // The whole drag was one step.
  panel.bench.history.Undo();
  QCOMPARE(panel.Nodes(), before);
  // Escape puts everything back and leaves no step.
  const QString label = panel.bench.history.UndoLabel();
  QTest::mousePress(pad, Qt::LeftButton, Qt::NoModifier, at(0.7, 0.7));
  QVERIFY(panel.Nodes() != before);
  QTest::mouseMove(pad, at(0.1, 0.1));
  QTest::keyClick(pad, Qt::Key_Escape);
  QVERIFY(!pad->is_dragging());
  QCOMPARE(panel.Nodes(), before);
  QCOMPARE(panel.bench.history.UndoLabel(), label);
  QTest::mouseRelease(pad, Qt::LeftButton, Qt::NoModifier, at(0.1, 0.1));
  QCOMPARE(panel.Nodes(), before);
}

void DragPanelTests::ClickingThePadJumpsThePuck() {
  Panel panel;
  panel.Pick({0, 1});
  panel.Find<KindTile>("kind_drag")->click();
  auto* pad = panel.Find<FeelPad>("feel_pad");
  QCOMPARE(pad->puck_count(), size_t{1});
  QTest::mouseClick(pad, Qt::LeftButton, Qt::NoModifier,
                    PointOf({0.75, 0.25}, pad->Area()).toPoint());
  const Feel landed = FeelOf(panel.Nodes()[1]);
  QVERIFY(std::abs(landed.firm - 0.75) < 0.03);
  QVERIFY(std::abs(landed.heavy - 0.25) < 0.03);
  QCOMPARE(panel.bench.history.UndoLabel(), RigManager::FeelLabel(2));
  // Different feels show a hollow puck each.
  QVERIFY(panel.bench.rig.SetFeelAll({{"Bob", "head", 0}}, {0.1, 0.1})
              .has_value());
  QCOMPARE(pad->puck_count(), size_t{2});
}

void DragPanelTests::JellyShowsSquishAndClothShowsStretch() {
  Panel panel;
  panel.Pick({4});
  auto* fine = panel.Find<FoldSection>("fine_tune");
  fine->SetOpen(true);
  const auto label = [&panel] {
    auto* form = panel.Find<FoldSection>("fine_tune")->form();
    auto* stretch = panel.Find<QDoubleSpinBox>("stretch");
    return qobject_cast<QLabel*>(form->labelForField(stretch))->text();
  };
  panel.Find<KindTile>("kind_jelly")->click();
  QVERIFY(panel.Find<QDoubleSpinBox>("stretch")->isVisibleTo(&panel.box));
  QVERIFY(!panel.Find<QDoubleSpinBox>("lag")->isVisibleTo(&panel.box));
  QCOMPARE(label(), QString("Squish"));
  QVERIFY(panel.Find<QDoubleSpinBox>("stretch")->toolTip().contains(
      "squashes"));
  panel.Find<KindTile>("kind_cloth")->click();
  QCOMPARE(label(), QString("Stretch"));
  panel.Find<KindTile>("kind_drag")->click();
  QVERIFY(panel.Find<QDoubleSpinBox>("lag")->isVisibleTo(&panel.box));
  QVERIFY(!panel.Find<QDoubleSpinBox>("stretch")->isVisibleTo(&panel.box));
}

void DragPanelTests::TilesLoopOnlyUnderThePointerAndNeverInOff() {
  Panel panel;
  panel.Pick({0});
  auto* tile = panel.Find<KindTile>("kind_cloth");
  QEnterEvent enter{QPointF(), QPointF(), QPointF()};
  QVERIFY(!tile->is_animating());
  QApplication::sendEvent(tile, &enter);
  QVERIFY(tile->is_animating());
  QEvent leave(QEvent::Leave);
  QApplication::sendEvent(tile, &leave);
  QVERIFY(!tile->is_animating());
  panel.bench.motion.SetMode(MotionMode::kOff);
  QApplication::sendEvent(tile, &enter);
  QVERIFY(!tile->is_animating());
  QApplication::sendEvent(tile, &leave);
  // A greyed tile does not loop either.
  panel.bench.motion.SetMode(MotionMode::kFull);
  panel.bench.selection.Clear();
  QApplication::sendEvent(tile, &enter);
  QVERIFY(!tile->is_animating());
}

void DragPanelTests::TheFoldIsClosedAtFirstAndRemembered() {
  Panel panel;
  panel.bench.motion.SetMode(MotionMode::kOff);
  panel.Pick({0});
  panel.Find<KindTile>("kind_drag")->click();
  auto* fine = panel.Find<FoldSection>("fine_tune");
  QVERIFY(!fine->is_open());
  QVERIFY(!panel.Find<QDoubleSpinBox>("lag")->isVisibleTo(&panel.box));
  fine->SetOpen(true);
  QVERIFY(panel.Find<QDoubleSpinBox>("lag")->isVisibleTo(&panel.box));
  // A new panel opens as it was left.
  DragBox again(panel.bench.All());
  QVERIFY(again.findChild<FoldSection*>("fine_tune")->is_open());
  fine->SetOpen(false);
  QVERIFY(!panel.Find<QDoubleSpinBox>("lag")->isVisibleTo(&panel.box));
}

void DragPanelTests::TheBlobRunsOnlyWhileShownAndMotionIsOn() {
  Panel panel;
  panel.Pick({0});
  panel.Find<KindTile>("kind_jelly")->click();
  auto* blob = panel.Find<WobbleBlob>("feel_preview");
  QVERIFY(!blob->is_animating());
  panel.box.show();
  QVERIFY(blob->is_animating());
  panel.bench.motion.SetMode(MotionMode::kOff);
  QVERIFY(!blob->is_animating());
  panel.bench.motion.SetMode(MotionMode::kCalm);
  QVERIFY(blob->is_animating());
  panel.box.hide();
  QVERIFY(!blob->is_animating());
}

}  // namespace snapper

QTEST_MAIN(snapper::DragPanelTests)
#include "drag_panel_tests.moc"
