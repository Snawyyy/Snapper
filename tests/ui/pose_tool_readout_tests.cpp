#include <QTest>

#include <cassert>

#include "bench.h"
#include "pose_stage.h"
#include "ui/pose_tool.h"

namespace snapper {
namespace {

using posestage::Body;
using posestage::kFrame;
using posestage::Stage;

}  // namespace

// What a drag on the stage says while it runs.
class PoseToolReadoutTests final : public QObject {
  Q_OBJECT

 private slots:
  void SilentUntilTheDeadZoneIsLeft();
  void MoveSaysTheOffset();
  void BoxSaysNothing();
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

void PoseToolReadoutTests::CancelSilencesAndRestores() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press({60, 60}, false, false, kFrame);
  tool.Move({90, 60}, false);
  QVERIFY(tool.ReadoutOf().has_value());
  tool.Cancel();
  QVERIFY(!tool.ReadoutOf().has_value());
  QCOMPARE(Body(bench).offset, QPointF(0, 0));
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
