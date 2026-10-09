#include <QMenu>
#include <QTest>

#include <cassert>

#include "bench.h"
#include "ui/timing_chart_action.h"
#include "ui/timing_chart_dialog.h"

namespace snapper {
namespace {

const ShotId kShot(1);
const TrackRef kArm{kShot, TrackKind::kPiece, LayerId(1), "arm"};

PiecePose Turned(double degrees) {
  assert(kShot.value() == 1);
  assert(kShot.value() > 0);
  PiecePose pose;
  pose.rotation = degrees;
  return pose;
}

// Bob's arm turned 0 on frame 0 and 80 on frame 8.
Project Stage() {
  assert(kShot.value() == 1);
  assert(kShot.value() > 0);
  DollLayer posed{"Bob", {}, false};
  SetKey(&posed.pieces["arm"], {Frame(0), Turned(0.0), Ease::kStep});
  SetKey(&posed.pieces["arm"], {Frame(8), Turned(80.0), Ease::kStep});
  Layer layer;
  layer.id = LayerId(1);
  layer.name = "Bob";
  layer.content = posed;
  Shot shot;
  shot.id = kShot;
  shot.length = Frame(48);
  shot.layers = {layer};
  Project project;
  project.shots = {std::make_shared<const Shot>(shot)};
  return project;
}

void PickRange(Bench* bench) {
  assert(kShot.value() == 1);
  assert(kShot.value() > 0);
  bench->history.Reset(Stage());
  bench->selection.SelectShot(kShot);
  bench->selection.PickThings({{LayerId(1), QString()}}, PickMode::kReplace,
                              LayerId(1));
  bench->selection.PickKeys({{kArm, Frame(0)}, {kArm, Frame(8)}},
                            PickMode::kReplace);
}

}  // namespace

class TimingChartUiTests final : public QObject {
  Q_OBJECT

 private slots:
  void MenuItemIsGreyedWithAReason();
  void HotkeyToastsTheReason();
  void DraggingATickMovesIt();
  void EscapeCancelsTheDragThenClosesTheWindow();
  void EnterApplies();
  void KeysDropdownRebuildsTicksAndReadsThePreset();
  void KeysChoiceIsRemembered();
  void ExistingKeysWithNoneBetweenGreysApplyWithAReason();
};

void TimingChartUiTests::MenuItemIsGreyedWithAReason() {
  Bench bench;
  bench.history.Reset(Stage());
  QMenu menu;
  AddTimingChartAction(&menu, bench.All());
  QCOMPARE(menu.actions().size(), 1);
  QVERIFY(!menu.actions()[0]->isEnabled());
  QVERIFY(!menu.actions()[0]->toolTip().isEmpty());
  QVERIFY(menu.actions()[0]->text().contains(
      TimingChartShortcut().toString(QKeySequence::NativeText)));
  PickRange(&bench);
  QMenu ready;
  AddTimingChartAction(&ready, bench.All());
  QVERIFY(ready.actions()[0]->isEnabled());
}

void TimingChartUiTests::HotkeyToastsTheReason() {
  Bench bench;
  bench.history.Reset(Stage());
  RequestTimingChart(bench.All());
  QCOMPARE(bench.toasts.queue().toasts().size(), size_t(1));
  QVERIFY(!bench.timing.IsOpen());
}

void TimingChartUiTests::DraggingATickMovesIt() {
  Bench bench;
  PickRange(&bench);
  TimingChartDialog dialog(bench.All(), nullptr);
  RequestTimingChart(bench.All());
  QVERIFY(bench.timing.IsOpen());
  QVERIFY(dialog.isVisible());
  TimingChartStrip& strip = *dialog.strip();
  const double start = bench.timing.fractions()[3];
  const QPoint grab(static_cast<int>(strip.XOf(start)), 34);
  QTest::mousePress(&strip, Qt::LeftButton, Qt::NoModifier, grab);
  QCOMPARE(strip.dragged(), 3);
  QTest::mouseMove(&strip, grab + QPoint(30, 0));
  QVERIFY(bench.timing.fractions()[3] > start);
  QTest::mouseRelease(&strip, Qt::LeftButton, Qt::NoModifier,
                      grab + QPoint(30, 0));
  QCOMPARE(strip.dragged(), -1);
  QCOMPARE(dialog.presets()->currentIndex(), -1);
  dialog.reject();
}

void TimingChartUiTests::EscapeCancelsTheDragThenClosesTheWindow() {
  Bench bench;
  PickRange(&bench);
  TimingChartDialog dialog(bench.All(), nullptr);
  RequestTimingChart(bench.All());
  TimingChartStrip& strip = *dialog.strip();
  const auto start = bench.timing.fractions();
  const QPoint grab(static_cast<int>(strip.XOf(start[3])), 34);
  QTest::mousePress(&strip, Qt::LeftButton, Qt::NoModifier, grab);
  QTest::mouseMove(&strip, grab + QPoint(30, 0));
  QTest::keyClick(&strip, Qt::Key_Escape);
  QCOMPARE(bench.timing.fractions(), start);
  QVERIFY(bench.timing.IsOpen());
  QTest::mouseRelease(&strip, Qt::LeftButton, Qt::NoModifier, grab);
  QTest::keyClick(&dialog, Qt::Key_Escape);
  QVERIFY(!bench.timing.IsOpen());
  QVERIFY(!dialog.isVisible());
  QVERIFY(!bench.history.CanUndo());
}

void TimingChartUiTests::EnterApplies() {
  Bench bench;
  PickRange(&bench);
  TimingChartDialog dialog(bench.All(), nullptr);
  RequestTimingChart(bench.All());
  QVERIFY(bench.timing.IsOpen());
  QTest::keyClick(&dialog, Qt::Key_Return);
  QVERIFY(!bench.timing.IsOpen());
  QCOMPARE(bench.history.UndoLabel(), QString("Timing chart on Bob"));
  QCOMPARE(bench.toasts.queue().toasts().size(), size_t(1));
}

void TimingChartUiTests::KeysDropdownRebuildsTicksAndReadsThePreset() {
  Bench bench;
  PickRange(&bench);
  TimingChartDialog dialog(bench.All(), nullptr);
  RequestTimingChart(bench.All());
  QCOMPARE(dialog.key_modes()->currentIndex(), 1);
  QCOMPARE(dialog.key_modes()->itemText(0), QString("Existing keys only"));
  QCOMPARE(dialog.key_modes()->itemText(3), QString("Add on 3s"));
  emit dialog.presets()->activated(2);
  QCOMPARE(bench.timing.spacing(), Spacing::kSlowOut);
  QVERIFY(bench.timing.MoveTick(0, 0.9).has_value());
  emit dialog.presets()->activated(-1);
  emit dialog.key_modes()->activated(2);
  QCOMPARE(bench.timing.frames(), std::vector<int>({2, 4, 6}));
  QCOMPARE(bench.timing.fractions(), SpacingFractions(Spacing::kSlowOut, 3));
  QCOMPARE(dialog.presets()->currentIndex(), 2);
  QVERIFY(dialog.apply_button()->isEnabled());
  dialog.reject();
}

void TimingChartUiTests::KeysChoiceIsRemembered() {
  Bench bench;
  PickRange(&bench);
  {
    TimingChartDialog dialog(bench.All(), nullptr);
    RequestTimingChart(bench.All());
    emit dialog.key_modes()->activated(3);
    QCOMPARE(bench.timing.key_mode(), KeyMode::kThrees);
    dialog.reject();
  }
  TimingChartDialog again(bench.All(), nullptr);
  RequestTimingChart(bench.All());
  QCOMPARE(bench.timing.key_mode(), KeyMode::kThrees);
  QCOMPARE(again.key_modes()->currentIndex(), 3);
  QCOMPARE(bench.timing.frames(), std::vector<int>({3, 6}));
  again.reject();
}

void TimingChartUiTests::ExistingKeysWithNoneBetweenGreysApplyWithAReason() {
  Bench bench;
  PickRange(&bench);
  TimingChartDialog dialog(bench.All(), nullptr);
  RequestTimingChart(bench.All());
  emit dialog.key_modes()->activated(0);
  QVERIFY(bench.timing.frames().empty());
  QVERIFY(!dialog.apply_button()->isEnabled());
  QCOMPARE(dialog.apply_button()->toolTip(),
           QString("No keys between A and B to respace"));
  emit dialog.key_modes()->activated(1);
  QVERIFY(dialog.apply_button()->isEnabled());
  dialog.reject();
}

}  // namespace snapper

QTEST_MAIN(snapper::TimingChartUiTests)
#include "timing_chart_ui_tests.moc"
