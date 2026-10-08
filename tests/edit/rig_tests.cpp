#include <QTest>

#include "edit/history_manager.h"
#include "edit/rig_manager.h"

namespace snapper {
namespace {

Project ArmProject() {
  Doll doll;
  doll.name = "Bob";
  doll.art.pieces = {{"body", {"b.png"}, 0, {}, {10, 10}},
                     {"upper", {"u.png"}, 0, {}, {10, 2}},
                     {"lower", {"l.png", "l2.png"}, 0, {}, {10, 2}}};
  doll.rig.pieces = {{"body", "", {}, 0, -1, {}},
                     {"upper", "body", {}, 1, -1, {}},
                     {"lower", "upper", {}, 2, -1, {2, 2}}};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  PiecePose warped;
  warped.warp = std::vector<QPointF>(9, QPointF(1, 1));
  Layer layer;
  layer.id = LayerId(1);
  DollLayer posed{"Bob", {}, false};
  SetKey(&posed.pieces["lower"], {Frame(0), warped, Ease::kStep});
  layer.content = posed;
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {layer};
  project.shots = {std::make_shared<const Shot>(shot)};
  return project;
}

const Rig& BobRig(const HistoryManager& history) {
  return FindDoll(history.current(), "Bob")->rig;
}

}  // namespace

class RigTests final : public QObject {
  Q_OBJECT

 private slots:
  void ParentsRefuseLoops();
  void PivotOrderAndDrawingAreUndoable();
  void NewWarpGridClearsOldWarpKeys();
  void ChainsNeedLinkedPieces();
  void UnknownDollsAndPiecesSayWhy();
  void OneSideCopiesToTheOther();
  void ManyPiecesChangeTogether();
  void PointMotionsAreOneStepEach();
};

void RigTests::ParentsRefuseLoops() {
  HistoryManager history(ArmProject());
  RigManager rig(&history);
  QVERIFY(!rig.SetParent("Bob", "body", "lower").has_value());
  QVERIFY(rig.SetParent("Bob", "lower", "body").has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->parent, QString("body"));
  QVERIFY(rig.SetParent("Bob", "lower", "").has_value());
  QVERIFY(FindRig(BobRig(history), "lower")->parent.isEmpty());
}

void RigTests::PivotOrderAndDrawingAreUndoable() {
  HistoryManager history(ArmProject());
  RigManager rig(&history);
  QVERIFY(rig.SetPivot("Bob", "upper", QPointF(3, 1)).has_value());
  QVERIFY(rig.SetOrder("Bob", "upper", 9).has_value());
  QVERIFY(rig.SetDefaultDrawing("Bob", "lower", 1).has_value());
  QVERIFY(!rig.SetDefaultDrawing("Bob", "lower", 2).has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->default_drawing, 1);
  QCOMPARE(history.UndoLabel(), QString("Default drawing of lower"));
  history.Undo();
  history.Undo();
  QCOMPARE(FindRig(BobRig(history), "upper")->order, 1);
  QCOMPARE(FindRig(BobRig(history), "upper")->pivot, QPointF(3, 1));
  QVERIFY(!rig.SetPivot("Bob", "upper", QPointF(qInf(), 0)).has_value());
  QVERIFY(rig.SetRestRotation("Bob", "upper", 30.0).has_value());
  QCOMPARE(FindRig(BobRig(history), "upper")->rest_rotation, 30.0);
  QVERIFY(!rig.SetRestRotation("Bob", "upper", qQNaN()).has_value());
}

void RigTests::NewWarpGridClearsOldWarpKeys() {
  HistoryManager history(ArmProject());
  RigManager rig(&history);
  QVERIFY(!rig.SetWarpGrid("Bob", "lower", {0, 3}).has_value());
  QVERIFY(!rig.SetWarpGrid("Bob", "lower", {9, 1}).has_value());
  QVERIFY(rig.SetWarpReach("Bob", "lower", 4, 1.5).has_value());
  QVERIFY(!rig.SetWarpReach("Bob", "lower", 9, 1.0).has_value());
  QVERIFY(!rig.SetWarpReach("Bob", "upper", 0, 1.0).has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->warp_reach[4], 1.5);
  QVERIFY(rig.SetWarpReach("Bob", "lower", 0, -3.0).has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->warp_reach[0], 0.0);
  QCOMPARE(history.UndoLabel(), QString("Rubber reach of lower"));
  QVERIFY(rig.ToggleDragNode("Bob", "lower", 4).has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->drag_nodes,
           (std::vector<DragNode>{{4, 0.5, 0.5}}));
  QVERIFY(rig.SetDrag("Bob", "lower", {4, 2.0, 0.2}).has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->drag_nodes[0].lag, 1.0);
  QVERIFY(!rig.SetDrag("Bob", "lower", {5, 0.5, 0.5}).has_value());
  QVERIFY(!rig.ToggleDragNode("Bob", "upper", 0).has_value());
  QVERIFY(rig.ToggleDragNode("Bob", "lower", 4).has_value());
  QVERIFY(FindRig(BobRig(history), "lower")->drag_nodes.empty());
  QCOMPARE(history.UndoLabel(), QString("Stop dragging point of lower"));
  QVERIFY(rig.ToggleDragNode("Bob", "lower", 4).has_value());
  // A new grid starts with no reach and no drag nodes.
  QVERIFY(rig.SetWarpGrid("Bob", "lower", {3, 1}).has_value());
  QVERIFY(FindRig(BobRig(history), "lower")->warp_reach.empty());
  QVERIFY(FindRig(BobRig(history), "lower")->drag_nodes.empty());
  const auto& layer = std::get<DollLayer>(
      history.current().shots[0]->layers[0].content);
  QVERIFY(layer.pieces.at("lower").keys[0].value.warp.empty());
  history.Undo();
  const auto& before = std::get<DollLayer>(
      history.current().shots[0]->layers[0].content);
  QCOMPARE(before.pieces.at("lower").keys[0].value.warp.size(), size_t{9});
}

void RigTests::ChainsNeedLinkedPieces() {
  HistoryManager history(ArmProject());
  RigManager rig(&history);
  QVERIFY(!rig.AddChain("Bob", {"arm", "body", "lower", {}, true})
               .has_value());
  QVERIFY(rig.AddChain("Bob", {"arm", "upper", "lower", {}, true})
              .has_value());
  QVERIFY(!rig.AddChain("Bob", {"arm", "upper", "lower", {}, true})
               .has_value());
  QVERIFY(rig.SetChainBend("Bob", "arm", false).has_value());
  QVERIFY(rig.SetChainTip("Bob", "arm", QPointF(10, 1)).has_value());
  QCOMPARE(BobRig(history).chains[0].tip, QPointF(10, 1));
  QVERIFY(!BobRig(history).chains[0].bends_clockwise);
  QVERIFY(rig.RemoveChain("Bob", "arm").has_value());
  QVERIFY(!rig.RemoveChain("Bob", "arm").has_value());
}

void RigTests::UnknownDollsAndPiecesSayWhy() {
  HistoryManager history(ArmProject());
  RigManager rig(&history);
  const auto doll = rig.SetOrder("Ann", "body", 1);
  QVERIFY(doll.error().message.contains("Ann"));
  const auto piece = rig.SetOrder("Bob", "tail", 1);
  QVERIFY(piece.error().message.contains("tail"));
  QVERIFY(!history.CanUndo());
}

void RigTests::OneSideCopiesToTheOther() {
  // A chest at 0..20 with arms drawn either side, off-centre by 100.
  Doll doll;
  doll.art.pieces = {{"chest", {"c"}, 0, {100, 0}, {20, 20}},
                     {"arm_l", {"a"}, 0, {80, 0}, {20, 6}},
                     {"arm_r", {"a"}, 0, {120, 0}, {20, 6}},
                     {"hand_l", {"h"}, 0, {70, 0}, {10, 6}},
                     {"hand_r", {"h"}, 0, {140, 0}, {10, 6}}};
  doll.rig.pieces = {{"chest", "", {10, 10}, 0, -1, {}},
                     {"arm_l", "chest", {18, 3}, 1, -1, {}, 12.0},
                     {"arm_r", "", {10, 3}, 2, -1, {}},
                     {"hand_l", "arm_l", {9, 3}, 3, -1, {2, 2}},
                     {"hand_r", "", {5, 3}, 4, -1, {}}};
  doll.rig.chains = {{"reach_l", "arm_l", "hand_l", {0, 3}, true}};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  HistoryManager history(project);
  RigManager rig(&history);
  QVERIFY(!rig.WhyNoCopy("Bob", "chest").isEmpty());
  QVERIFY(rig.CopyToOtherSide("Bob", "arm_l", false).has_value());
  const RigPiece* arm = FindRig(BobRig(history), "arm_r");
  // Joint at doll x 98 mirrors across x 110 to 122: 2 into arm_r.
  QCOMPARE(arm->pivot, QPointF(2, 3));
  QCOMPARE(arm->rest_rotation, -12.0);
  QCOMPARE(arm->parent, QString("chest"));
  QCOMPARE(arm->order, 2);
  QVERIFY(FindRig(BobRig(history), "hand_r")->parent.isEmpty());
  QVERIFY(rig.SetPointMotion("Bob", "hand_l",
                             {3, WarpMotionKind::kSway, 6.0, 24, 30.0, 0.5})
              .has_value());
  QVERIFY(rig.CopyToOtherSide("Bob", "hand_l", true).has_value());
  const RigPiece* hand = FindRig(BobRig(history), "hand_r");
  // Point 3 starts the middle row; mirrored it ends it, swinging the
  // other way across.
  QCOMPARE(hand->point_motions.size(), size_t{1});
  QCOMPARE(hand->point_motions[0].point, 5);
  QCOMPARE(hand->point_motions[0].angle, 150.0);
  QCOMPARE(hand->point_motions[0].delay, 0.5);
  QCOMPARE(hand->parent, QString("arm_r"));
  QCOMPARE(hand->warp, (WarpGrid{2, 2}));
  QCOMPARE(BobRig(history).chains.size(), size_t{2});
  QCOMPARE(BobRig(history).chains[1].lower, QString("hand_r"));
  QCOMPARE(BobRig(history).chains[1].tip, QPointF(10, 3));
  QVERIFY(!BobRig(history).chains[1].bends_clockwise);
  QCOMPARE(history.UndoLabel(), QString("Copy side to the other"));
}

void RigTests::ManyPiecesChangeTogether() {
  HistoryManager history(ArmProject());
  RigManager rig(&history);
  QVERIFY(rig.SetRestRotation("Bob", "upper", 10.0).has_value());
  QVERIFY(rig.ShiftRestAll("Bob", {"upper", "lower"}, 5.0).has_value());
  QCOMPARE(FindRig(BobRig(history), "upper")->rest_rotation, 15.0);
  QCOMPARE(FindRig(BobRig(history), "lower")->rest_rotation, 5.0);
  QVERIFY(rig.ShiftOrderAll("Bob", {"upper", "lower"}, 2).has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->order, 4);
  QVERIFY(rig.SetParentAll("Bob", {"upper", "lower"}, "").has_value());
  QVERIFY(FindRig(BobRig(history), "lower")->parent.isEmpty());
  QVERIFY(!rig.SetParentAll("Bob", {"body", "upper"}, "upper").has_value());
  QVERIFY(rig.MovePivots("Bob", {{"upper", QPointF(1, 2)},
                                 {"lower", QPointF(-1, 0)}})
              .has_value());
  QCOMPARE(FindRig(BobRig(history), "upper")->pivot, QPointF(1, 2));
  QVERIFY(rig.SetWarpAll("Bob", {"upper", "lower"}, {2, 2}).has_value());
  QCOMPARE(FindRig(BobRig(history), "upper")->warp, (WarpGrid{2, 2}));
  QVERIFY(rig.SetKeepShapeAll("Bob", {"upper", "lower"}, true).has_value());
  QVERIFY(FindRig(BobRig(history), "lower")->keeps_shape);
  QVERIFY(!FindRig(BobRig(history), "body")->keeps_shape);
  QCOMPARE(history.UndoLabel(), QString("Keep shape"));
  QVERIFY(!rig.ShiftRestAll("Bob", {}, 1.0).has_value());
  QVERIFY(!rig.CopyAllToOtherSide("Bob", {"body"}).has_value());
}

void RigTests::PointMotionsAreOneStepEach() {
  HistoryManager history(ArmProject());
  RigManager rig(&history);
  PointMotion wave{4, WarpMotionKind::kWave, 600.0, 1, 270.0, 2.0};
  QVERIFY(!rig.SetPointMotion("Bob", "upper", wave).has_value());
  QVERIFY(!rig.SetPointMotion("Bob", "lower", {9}).has_value());
  QVERIFY(rig.SetPointMotion("Bob", "lower", wave).has_value());
  QCOMPARE(history.UndoLabel(), QString("Animate point of lower"));
  const PointMotion kept =
      FindRig(BobRig(history), "lower")->point_motions.at(0);
  QCOMPARE(kept.size, kMaxWarpMotionSize);
  QCOMPARE(kept.cycle, kMinWarpCycle);
  QCOMPARE(kept.angle, -90.0);
  QCOMPARE(kept.delay, 1.0);
  wave.size = 4.0;
  QVERIFY(rig.SetPointMotion("Bob", "lower", wave).has_value());
  QCOMPARE(history.UndoLabel(), QString("Point animation of lower"));
  QCOMPARE(FindRig(BobRig(history), "lower")->point_motions.size(),
           size_t{1});
  wave.angle = qQNaN();
  QVERIFY(!rig.SetPointMotion("Bob", "lower", wave).has_value());
  wave.kind = WarpMotionKind::kNone;
  wave.angle = 0.0;
  QVERIFY(rig.SetPointMotion("Bob", "lower", wave).has_value());
  QCOMPARE(history.UndoLabel(), QString("Stop animating point of lower"));
  QVERIFY(FindRig(BobRig(history), "lower")->point_motions.empty());
  // Stopping a still point again changes nothing, so adds no step.
  QVERIFY(rig.SetPointMotion("Bob", "lower", wave).has_value());
  history.Undo();
  QCOMPARE(FindRig(BobRig(history), "lower")->point_motions.size(),
           size_t{1});
  QVERIFY(rig.SetWarpGrid("Bob", "lower", {3, 3}).has_value());
  QVERIFY(FindRig(BobRig(history), "lower")->point_motions.empty());
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::RigTests)
#include "rig_tests.moc"
