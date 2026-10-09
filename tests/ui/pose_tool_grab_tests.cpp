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
using posestage::kFrame;
using posestage::Stage;

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
  QCOMPARE(bench.history.UndoLabel(), QString("Move body"));
  tool.Press({60, 60}, false, false, kFrame);
  tool.Move({62, 61}, false);
  tool.Release();
  QCOMPARE(Body(bench).offset, QPointF(1, 0));
}

void PoseToolGrabTests::HandlesGrabFromTwelvePixels() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.PickWhole({60, 60}, kFrame);
  // The lean handle sits at (82, 50) in the widget.
  const QPointF lean(10 + 60 + kTurnGap, 10 + 40);
  tool.Press(lean + QPointF(kGrabReach + 1, 0), false, false, kFrame);
  tool.Move(lean + QPointF(kGrabReach + 1, 40), false);
  tool.Release();
  QVERIFY(!bench.history.CanUndo());
  tool.Press(lean + QPointF(0, kGrabReach), false, false, kFrame);
  QVERIFY(tool.IsDragging());
  tool.Move(lean + QPointF(0, kGrabReach + 40), false);
  tool.Release();
  QCOMPARE(bench.history.UndoLabel(), QString("Lean Dot"));
}

void PoseToolGrabTests::HiddenHandlesStayGrabbableBriefly() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.PickWhole({60, 60}, kFrame);
  tool.Observe();
  bench.selection.Clear();
  tool.Observe();
  tool.Press({82, 50}, false, false, kFrame);
  tool.Move({82, 100}, false);
  tool.Release();
  QCOMPARE(bench.history.UndoLabel(), QString("Lean Dot"));
  bench.history.Undo();
  tool.PickWhole({60, 60}, kFrame);
  tool.Observe();
  bench.selection.Clear();
  tool.Observe();
  QTest::qWait(kGraceMs + 60);
  tool.Press({82, 50}, false, false, kFrame);
  tool.Move({82, 100}, false);
  tool.Release();
  QVERIFY(!bench.history.CanUndo());
}

void PoseToolGrabTests::HoverNamesTheHandleOrTheThing() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  const auto over = tool.HoverAt({60, 60}, kFrame);
  QCOMPARE(over.kind, GrabKind::kNone);
  QVERIFY(over.pick.has_value());
  QCOMPARE(over.pick->piece, QString("body"));
  tool.PickWhole({60, 60}, kFrame);
  // Picked things show their red outline, not the hover one.
  QVERIFY(!tool.HoverAt({60, 60}, kFrame).pick.has_value());
  const auto on_handle = tool.HoverAt({82, 50}, kFrame);
  QCOMPARE(on_handle.kind, GrabKind::kLean);
  QCOMPARE(on_handle.key, PoseTool::TurnKey(LayerId(1)));
  QCOMPARE(tool.HoverAt({90, 90}, kFrame).kind, GrabKind::kNone);
}

}  // namespace snapper

QTEST_MAIN(snapper::PoseToolGrabTests)
#include "pose_tool_grab_tests.moc"
