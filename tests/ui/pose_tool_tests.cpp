#include <QTest>

#include "anim/doll_pose.h"
#include "bench.h"
#include "ui/pose_tool.h"

namespace snapper {
namespace {

const StageFrame kFrame{ShotId(1), Frame(0), 1.0, QPointF(10, 10)};

// A 100x100 project with a 20x20 red doll in the middle, its drawing in
// the bench's folder.
void Stage(Bench* bench) {
  QImage red(20, 20, QImage::Format_ARGB32);
  red.fill(Qt::red);
  QVERIFY(red.save(QDir(bench->dir.path()).filePath("body.png")));
  Doll doll;
  doll.folder = bench->dir.path();
  doll.art.pieces = {{"body", {"body.png"}, 0, {-10, -10}, {20, 20}}};
  doll.rig.pieces = {{"body", "", {10, 10}, 0, -1, {}}};
  Layer layer;
  layer.id = LayerId(1);
  layer.content = DollLayer{"Dot", {}, false};
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {layer};
  Project project;
  project.canvas = {100, 100};
  project.dolls["Dot"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  bench->history.Reset(project);
}

// The body's pose at frame 0; rest when it has no keys.
PiecePose Body(const Bench& bench) {
  const PoseMap poses = SamplePoses(
      std::get<DollLayer>(bench.history.current().shots[0]->layers[0].content),
      Frame(0));
  const auto found = poses.find("body");
  return found == poses.end() ? PiecePose() : found->second;
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
};

void PoseToolTests::ClickPicksAndDragMoves() {
  Bench bench;
  Stage(&bench);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press({60, 60}, false, false, kFrame);
  QCOMPARE(bench.selection.layer(), LayerId(1));
  QCOMPARE(bench.selection.pieces(), std::set<QString>({"body"}));
  QVERIFY(tool.IsDragging());
  tool.Move({70, 65}, false);
  tool.Move({75, 62}, false);
  tool.Release();
  QCOMPARE(Body(bench).offset, QPointF(15, 2));
  QCOMPARE(bench.history.UndoLabel(), QString("Move body"));
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
  // A second 20x20 doll piece to the right of the first.
  Project project = bench.history.current();
  Doll doll = *project.dolls.at("Dot");
  doll.art.pieces.push_back({"eye", {"body.png"}, 0, {20, -10}, {20, 20}});
  doll.rig.pieces.push_back({"eye", "", {10, 10}, 1, -1, {}});
  project.dolls["Dot"] = std::make_shared<const Doll>(doll);
  bench.history.Reset(project);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  // Box from above-left of the body to inside the eye catches both.
  tool.Press({12, 12}, false, false, kFrame);
  tool.Move({85, 65}, false);
  QVERIFY(!tool.Box().isEmpty());
  tool.Release();
  QCOMPARE(bench.selection.picks().size(), size_t{2});
  tool.Press({60, 60}, false, false, kFrame);
  tool.Move({64, 60}, false);
  tool.Release();
  const PoseMap poses = SamplePoses(
      std::get<DollLayer>(bench.history.current().shots[0]->layers[0].content),
      Frame(0));
  QCOMPARE(poses.at("body").offset, QPointF(4, 0));
  QCOMPARE(poses.at("eye").offset, QPointF(4, 0));
  QCOMPARE(bench.history.UndoLabel(), QString("Move 2 parts"));
  tool.Wheel(1, false, kFrame);
  const PoseMap turned = SamplePoses(
      std::get<DollLayer>(bench.history.current().shots[0]->layers[0].content),
      Frame(0));
  QCOMPARE(turned.at("eye").rotation, -5.0);
  // Ctrl-click flips the eye out; Shift-click adds it back.
  tool.Press({95, 60}, false, true, kFrame);
  tool.Release();
  QCOMPARE(bench.selection.picks().size(), size_t{1});
  tool.Press({95, 60}, true, false, kFrame);
  tool.Release();
  QCOMPARE(bench.selection.picks().size(), size_t{2});
}

}  // namespace snapper

QTEST_MAIN(snapper::PoseToolTests)
#include "pose_tool_tests.moc"
