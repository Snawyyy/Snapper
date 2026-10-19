#include <QTest>

#include <cassert>

#include "anim/sampler.h"
#include "bench.h"
#include "pose_stage.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/pose_tool.h"

namespace snapper {
namespace {

using posestage::Body;
using posestage::kFrame;
using posestage::Stage;

// A second 20x20 picture layer ("Eye") 40 pixels right of the first.
void AddEye(Bench* bench) {
  assert(bench != nullptr);
  Project project = bench->history.current();
  auto shot = std::make_shared<Shot>(*project.shots[0]);
  Layer eye = shot->layers[0];
  eye.id = LayerId(2);
  eye.name = "Eye";
  LayerPose right;
  right.offset = QPointF(40, 0);
  SetKey(&eye.transform, {Frame(0), right, Ease::kStep});
  shot->layers.push_back(eye);
  project.shots[0] = shot;
  bench->history.Reset(project);
  assert(project.shots[0]->layers.size() == 2);
}

LayerPose EyeAt0(const Bench& bench) {
  assert(bench.history.current().shots[0]->layers.size() == 2);
  return Sample(bench.history.current().shots[0]->layers[1].transform,
                Frame(0), LayerPose());
}

}  // namespace

class PoseToolTests final : public QObject {
  Q_OBJECT

 private slots:
  void ClickPicksAndDragMoves();
  void ShiftLocksTheAxis();
  void CtrlScales();
  void WheelTurns();
  void EscapePutsItBack();
  void ClickingNothingDropsThePick();
  void ManyPicksMoveAndTurnTogether();
  void DoubleClickPicksTheLayer();
};

void PoseToolTests::ClickPicksAndDragMoves() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press({60, 60}, false, false, kFrame);
  QCOMPARE(bench.selection.layer(), LayerId(1));
  QCOMPARE(bench.selection.picks(), std::set<Pick>({Pick{LayerId(1)}}));
  QVERIFY(tool.IsDragging());
  tool.Move({70, 65}, false);
  tool.Move({75, 62}, false);
  tool.Release();
  QCOMPARE(Body(bench).offset, QPointF(15, 2));
  QCOMPARE(bench.history.UndoLabel(), QString("Move layer"));
  bench.history.Undo();
  QCOMPARE(Body(bench).offset, QPointF(0, 0));
}

void PoseToolTests::ShiftLocksTheAxis() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press({60, 60}, false, false, kFrame);
  tool.Move({70, 63}, true);
  tool.Release();
  QVERIFY(bench.history.CanUndo());
  QCOMPARE(Body(bench).offset, QPointF(10, 0));
}

void PoseToolTests::CtrlScales() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press({60, 60}, false, true, kFrame);
  tool.Move({60 + kScalePixels, 60}, false);
  tool.Release();
  QCOMPARE(Body(bench).scale_x, 2.0);
  QCOMPARE(Body(bench).scale_y, 2.0);
  QCOMPARE(Body(bench).offset, QPointF(0, 0));
}

void PoseToolTests::WheelTurns() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Wheel(1, false, kFrame);
  QVERIFY(!tool.problem().isEmpty());
  tool.Press({60, 60}, false, false, kFrame);
  tool.Release();
  tool.Wheel(1, false, kFrame);
  tool.Wheel(-2, true, kFrame);
  QCOMPARE(Body(bench).rotation, -3.0);
  QVERIFY(tool.problem().isEmpty());
}

void PoseToolTests::EscapePutsItBack() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press({60, 60}, false, false, kFrame);
  tool.Move({90, 90}, false);
  QCOMPARE(Body(bench).offset, QPointF(30, 30));
  tool.Cancel();
  QVERIFY(!tool.IsDragging());
  QCOMPARE(Body(bench).offset, QPointF(0, 0));
  QVERIFY(!bench.history.CanUndo());
}

void PoseToolTests::ClickingNothingDropsThePick() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press({60, 60}, false, false, kFrame);
  tool.Release();
  tool.Press({12, 12}, false, false, kFrame);
  tool.Release();
  QVERIFY(!tool.IsDragging());
  QVERIFY(!bench.selection.layer().IsValid());
  QVERIFY(!bench.history.CanUndo());
}

void PoseToolTests::ManyPicksMoveAndTurnTogether() {
  Bench bench;
  Stage(&bench);
  AddEye(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.SetGuides(false);
  // Box from above-left of the body to inside the eye catches both.
  tool.Press({12, 12}, false, false, kFrame);
  tool.Move({105, 65}, false);
  QVERIFY(!tool.Box().isEmpty());
  tool.Release();
  QCOMPARE(bench.selection.picks().size(), size_t{2});
  tool.Press({60, 60}, false, false, kFrame);
  tool.Move({64, 60}, false);
  tool.Release();
  QCOMPARE(Body(bench).offset, QPointF(4, 0));
  QCOMPARE(EyeAt0(bench).offset, QPointF(44, 0));
  QCOMPARE(bench.history.UndoLabel(), QString("Move 2 parts"));
  tool.Wheel(1, false, kFrame);
  QCOMPARE(EyeAt0(bench).rotation, -5.0);
  // Ctrl-click flips the eye out; Shift-click adds it back.
  tool.Press({104, 60}, false, true, kFrame);
  tool.Release();
  QCOMPARE(bench.selection.picks().size(), size_t{1});
  tool.Press({104, 60}, true, false, kFrame);
  tool.Release();
  QCOMPARE(bench.selection.picks().size(), size_t{2});
}

void PoseToolTests::DoubleClickPicksTheLayer() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.PickWhole({60, 60}, kFrame);
  QCOMPARE(bench.selection.picks(), std::set<Pick>({Pick{LayerId(1)}}));
  const QPolygonF box =
      LayerShape(bench.history.current(), *bench.history.current().shots[0],
                 LayerId(1), Frame(0), 1.0, &cache);
  QCOMPARE(box.boundingRect(), QRectF(40, 40, 20, 20));
  // Dragging it moves the layer.
  tool.Press({60, 60}, false, false, kFrame);
  tool.Move({70, 60}, false);
  tool.Release();
  QCOMPARE(bench.history.current().shots[0]->layers[0].transform.keys.back()
               .value.offset,
           QPointF(10, 0));
}

}  // namespace snapper

QTEST_MAIN(snapper::PoseToolTests)
#include "pose_tool_tests.moc"
