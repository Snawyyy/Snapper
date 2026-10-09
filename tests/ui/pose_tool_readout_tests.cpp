#include <QTest>

#include <cassert>

#include "anim/sampler.h"
#include "bench.h"
#include "pose_stage.h"
#include "ui/drag_text.h"
#include "ui/pose_tool.h"

namespace snapper {
namespace {

using posestage::kFrame;
using posestage::Stage;

double LeanOf(const Bench& bench) {
  assert(!bench.history.current().shots.empty());
  assert(!bench.history.current().shots[0]->layers.empty());
  return Sample(bench.history.current().shots[0]->layers[0].transform,
                Frame(0), PiecePose())
      .lean;
}

}  // namespace

// What a drag on the stage says while it runs.
class PoseToolReadoutTests final : public QObject {
  Q_OBJECT

 private slots:
  void SilentUntilTheDeadZoneIsLeft();
  void MoveSaysTheOffset();
  void BoxSaysNothing();
  void TurnSaysDegrees();
  void ShiftSnapsTheTurnToFifteen();
  void CancelSilencesAndRestores();
  void WheelTurnsFiveADegreeNotchOrOneWithShift();
};

void PoseToolReadoutTests::SilentUntilTheDeadZoneIsLeft() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  QVERIFY(!tool.ReadoutOf().has_value());
  tool.Press({60, 60}, false, false, kFrame);
  tool.Move({61, 60}, false);
  QVERIFY(!tool.ReadoutOf().has_value());
  tool.Release();
  QVERIFY(!tool.ReadoutOf().has_value());
}

void PoseToolReadoutTests::MoveSaysTheOffset() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press({60, 60}, false, false, kFrame);
  tool.Move({70, 56}, false);
  const auto readout = tool.ReadoutOf();
  QVERIFY(readout.has_value());
  QCOMPARE(readout->text, QStringLiteral("+10, -4 px"));
  QCOMPARE(readout->start, QPointF(60, 60));
  QCOMPARE(readout->cursor, QPointF(70, 56));
  QVERIFY(!readout->is_snapped);
  tool.Release();
}

void PoseToolReadoutTests::BoxSaysNothing() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press({2, 2}, false, false, kFrame);
  tool.Move({40, 40}, false);
  QVERIFY(tool.IsDragging());
  QVERIFY(!tool.ReadoutOf().has_value());
  tool.Release();
}

void PoseToolReadoutTests::TurnSaysDegrees() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.PickWhole({60, 60}, kFrame);
  tool.Press({82, 50}, false, false, kFrame);
  tool.Move({82, 90}, false);
  const auto readout = tool.ReadoutOf();
  QVERIFY(readout.has_value());
  QCOMPARE(readout->text, AngleText(40 * kTurnPerPixel));
  QVERIFY(!readout->is_snapped);
  tool.Release();
}

void PoseToolReadoutTests::ShiftSnapsTheTurnToFifteen() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.PickWhole({60, 60}, kFrame);
  tool.Press({82, 50}, false, false, kFrame);
  // 100 px is 25 degrees; the nearest 15 is 30.
  tool.Move({82, 150}, true);
  QCOMPARE(LeanOf(bench), 30.0);
  const auto readout = tool.ReadoutOf();
  QVERIFY(readout.has_value());
  QCOMPARE(readout->text, QStringLiteral("30 deg"));
  QVERIFY(readout->is_snapped);
  // Already on a multiple: nothing snapped.
  tool.Move({82, 110}, true);
  QCOMPARE(LeanOf(bench), 15.0);
  QCOMPARE(tool.ReadoutOf()->text, QStringLiteral("15 deg"));
  QVERIFY(!tool.ReadoutOf()->is_snapped);
  tool.Release();
  QCOMPARE(bench.history.UndoLabel(), QString("Lean Dot"));
}

void PoseToolReadoutTests::CancelSilencesAndRestores() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.PickWhole({60, 60}, kFrame);
  tool.Press({82, 50}, false, false, kFrame);
  tool.Move({82, 150}, true);
  QVERIFY(tool.ReadoutOf().has_value());
  tool.Cancel();
  QVERIFY(!tool.ReadoutOf().has_value());
  QCOMPARE(LeanOf(bench), 0.0);
  QVERIFY(!bench.history.CanUndo());
}

void PoseToolReadoutTests::WheelTurnsFiveADegreeNotchOrOneWithShift() {
  QCOMPARE(PoseTool::WheelDegrees(1, false), -5.0);
  QCOMPARE(PoseTool::WheelDegrees(-3, false), 15.0);
  QCOMPARE(PoseTool::WheelDegrees(2, true), -2.0);
}

}  // namespace snapper

QTEST_MAIN(snapper::PoseToolReadoutTests)
#include "pose_tool_readout_tests.moc"
