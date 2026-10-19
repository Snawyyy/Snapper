#include <QTest>

#include "anim/sampler.h"
#include "bench.h"
#include "pose_stage.h"
#include "render/stage_hit.h"
#include "ui/grab.h"
#include "ui/pose_tool.h"

namespace snapper {
namespace {

using posestage::Body;
using posestage::kBoneTip;
using posestage::kFrame;
using posestage::Stage;
using posestage::StageBone;

}  // namespace

// The big grab zones, the dead zone and the hover of the pose tool.
class PoseToolGrabTests final : public QObject {
  Q_OBJECT

 private slots:
  void NothingMovesInTheDeadZone();
  void HandlesGrabFromTwelvePixels();
  void HiddenHandlesStayGrabbableBriefly();
  void HoverNamesTheHandleOrTheThing();
};

void PoseToolGrabTests::NothingMovesInTheDeadZone() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press({60, 60}, false, false, kFrame);
  tool.Move({60 + kDragDeadZone - 0.5, 60}, false);
  QCOMPARE(Body(bench).offset, QPointF(0, 0));
  tool.Move({60 + kDragDeadZone, 60}, false);
  // Past the zone the whole distance applies at once.
  QCOMPARE(Body(bench).offset, QPointF(kDragDeadZone, 0));
  tool.Move({61, 60}, false);
  QCOMPARE(Body(bench).offset, QPointF(1, 0));
  tool.Release();
  QCOMPARE(bench.history.UndoLabel(), QString("Move layer"));
  tool.Press({60, 60}, false, false, kFrame);
  tool.Move({62, 61}, false);
  tool.Release();
  QCOMPARE(Body(bench).offset, QPointF(1, 0));
}

void PoseToolGrabTests::HandlesGrabFromTwelvePixels() {
  Bench bench;
  Stage(&bench);
  StageBone(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press(kBoneTip + QPointF(kGrabReach + 1, 0), false, false, kFrame);
  tool.Move(kBoneTip + QPointF(kGrabReach + 1, 40), false);
  tool.Release();
  QVERIFY(!tool.IsDragging());
  const QString made = bench.history.UndoLabel();
  tool.Press(kBoneTip + QPointF(0, kGrabReach), false, false, kFrame);
  QVERIFY(tool.IsDragging());
  tool.Move(kBoneTip + QPointF(30, kGrabReach), false);
  tool.Release();
  QVERIFY(bench.history.UndoLabel() != made);
  QVERIFY(bench.history.UndoLabel().startsWith("Bend"));
}

void PoseToolGrabTests::HiddenHandlesStayGrabbableBriefly() {
  Bench bench;
  Stage(&bench);
  StageBone(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Observe();
  const LayerId bo = bench.selection.layer();
  bench.selection.Clear();
  tool.Observe();
  tool.Press(kBoneTip, false, false, kFrame);
  tool.Move(kBoneTip + QPointF(30, 0), false);
  tool.Release();
  QVERIFY(bench.history.UndoLabel().startsWith("Bend"));
  bench.history.Undo();
  const QString before = bench.history.UndoLabel();
  bench.selection.PickThings({Pick{bo}}, PickMode::kReplace, bo);
  tool.Observe();
  bench.selection.Clear();
  tool.Observe();
  QTest::qWait(kGraceMs + 60);
  tool.Press(kBoneTip, false, false, kFrame);
  tool.Move(kBoneTip + QPointF(30, 0), false);
  tool.Release();
  QCOMPARE(bench.history.UndoLabel(), before);
}

void PoseToolGrabTests::HoverNamesTheHandleOrTheThing() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  const auto over = tool.HoverAt({60, 60}, kFrame);
  QCOMPARE(over.kind, GrabKind::kNone);
  QVERIFY(over.pick.has_value());
  QCOMPARE(over.pick->layer, LayerId(1));
  tool.PickWhole({60, 60}, kFrame);
  // Picked things show their red outline, not the hover one.
  QVERIFY(!tool.HoverAt({60, 60}, kFrame).pick.has_value());
  const BoneId bone = StageBone(&bench);
  const auto on_handle = tool.HoverAt(kBoneTip, kFrame);
  QCOMPARE(on_handle.kind, GrabKind::kBone);
  QCOMPARE(on_handle.key,
           PoseTool::BoneKey(bench.selection.layer(), bone));
  QCOMPARE(tool.HoverAt({95, 30}, kFrame).kind, GrabKind::kNone);
}

}  // namespace snapper

QTEST_MAIN(snapper::PoseToolGrabTests)
#include "pose_tool_grab_tests.moc"
