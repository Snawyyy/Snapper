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
  void PhysicsIsOneNamedUndoStep();
  void ColliderEditsAreNamedUndoSteps();
  void ManyDotsEditInOneNamedStep();
  void MirrorNodesAndDotsOntoTheTwin();
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
  const std::vector<DotRef> four = {{"Bob", "lower", 4}};
  QVERIFY(rig.SetDragNodes(four, true).has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->drag_nodes,
           (std::vector<DragNode>{{4, 0.5, 0.5}}));
  QVERIFY(rig.SetDragField(four, DragField::kLag, 2.0).has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->drag_nodes[0].lag, 1.0);
  QVERIFY(!rig.SetDragField({{"Bob", "lower", 5}}, DragField::kLag, 0.5)
               .has_value());
  QVERIFY(!rig.SetDragNodes({{"Bob", "upper", 0}}, true).has_value());
  QVERIFY(rig.SetDragNodes(four, false).has_value());
  QVERIFY(FindRig(BobRig(history), "lower")->drag_nodes.empty());
  QCOMPARE(history.UndoLabel(), QString("Stop dragging 1 dot"));
  QVERIFY(rig.SetDragNodes(four, true).has_value());
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

void RigTests::PhysicsIsOneNamedUndoStep() {
  HistoryManager history(ArmProject());
  RigManager rig(&history);
  QVERIFY(rig.SetWarpGrid("Bob", "lower", {2, 2}).has_value());
  const std::vector<DotRef> four = {{"Bob", "lower", 4}};
  // Only a drag node can become physics.
  QVERIFY(!rig.SetPhysicsAll(four, true).has_value());
  QVERIFY(rig.SetDragNodes(four, true).has_value());
  QVERIFY(rig.SetPhysicsAll(four, true).has_value());
  QVERIFY(FindRig(BobRig(history), "lower")->drag_nodes[0].physics);
  QCOMPARE(history.UndoLabel(), QString("Physics on for 1 dot"));
  QVERIFY(rig.SetDragField(four, DragField::kStretch, 3.0).has_value());
  QVERIFY(rig.SetDragField(four, DragField::kAir, -1.0).has_value());
  QVERIFY(rig.SetDragField(four, DragField::kGravity, 0.9).has_value());
  const DragNode& kept = FindRig(BobRig(history), "lower")->drag_nodes[0];
  QCOMPARE(kept.stretch, 1.0);
  QCOMPARE(kept.air, 0.0);
  QCOMPARE(kept.gravity, 0.9);
  QVERIFY(kept.physics);
  QCOMPARE(history.UndoLabel(), QString("Set gravity on 1 dot"));
  history.Undo();
  QCOMPARE(FindRig(BobRig(history), "lower")->drag_nodes[0].gravity, 0.3);
  QVERIFY(rig.SetPhysicsAll(four, false).has_value());
  QVERIFY(!FindRig(BobRig(history), "lower")->drag_nodes[0].physics);
  QCOMPARE(history.UndoLabel(), QString("Physics off for 1 dot"));
  history.Undo();
  QVERIFY(FindRig(BobRig(history), "lower")->drag_nodes[0].physics);
}

void RigTests::ColliderEditsAreNamedUndoSteps() {
  HistoryManager history(ArmProject());
  RigManager rig(&history);
  QImage blank(40, 40, QImage::Format_ARGB32);
  blank.fill(Qt::transparent);
  QVERIFY(!rig.MakeCollider("Bob", "lower", blank).has_value());
  QImage block = blank;
  block.fill(Qt::transparent);
  for (int y = 10; y < 30; ++y) {
    for (int x = 5; x < 35; ++x) {
      block.setPixelColor(x, y, QColor(10, 10, 10, 255));
    }
  }
  QVERIFY(rig.MakeCollider("Bob", "lower", block).has_value());
  QCOMPARE(history.UndoLabel(), QString("Make collider of lower"));
  const Collider made = FindRig(BobRig(history), "lower")->collider;
  QVERIFY(made.IsOn());
  QCOMPARE(made.ring_size, static_cast<int>(made.dots.size()));
  QCOMPARE(made.depth, 10.0);
  QVERIFY(!rig.DeleteColliderDots("Bob", "lower", {999}).has_value());
  QVERIFY(rig.DeleteColliderDots("Bob", "lower", {0, 1}).has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->collider.dots.size(),
           made.dots.size() - 2);
  QCOMPARE(history.UndoLabel(), QString("Delete collider dots of lower"));
  QVERIFY(rig.SetColliderPushes("Bob", "lower", "upper", false).has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->collider.spared,
           (std::vector<QString>{"upper"}));
  QVERIFY(rig.SetColliderPushes("Bob", "lower", "upper", true).has_value());
  QVERIFY(FindRig(BobRig(history), "lower")->collider.spared.empty());
  QVERIFY(!rig.SetColliderPushes("Bob", "upper", "lower", false).has_value());
  history.Undo();
  history.Undo();
  history.Undo();
  QCOMPARE(FindRig(BobRig(history), "lower")->collider, made);
  QVERIFY(rig.RemoveCollider("Bob", "lower").has_value());
  QVERIFY(!FindRig(BobRig(history), "lower")->collider.IsOn());
  QCOMPARE(history.UndoLabel(), QString("Remove collider of lower"));
  QVERIFY(!rig.RemoveCollider("Bob", "lower").has_value());
}

void RigTests::ManyDotsEditInOneNamedStep() {
  HistoryManager history(ArmProject());
  RigManager rig(&history);
  QVERIFY(rig.SetWarpGrid("Bob", "lower", {2, 2}).has_value());
  std::vector<DotRef> dots;
  for (int point = 0; point < 6; ++point) {
    dots.push_back({"Bob", "lower", point});
  }
  QVERIFY(!rig.SetDragField(dots, DragField::kBounce, 0.7).has_value());
  QVERIFY(rig.SetDragNodes(dots, true).has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->drag_nodes.size(), size_t{6});
  QCOMPARE(history.UndoLabel(), QString("Drag 6 dots"));
  QVERIFY(rig.SetDragField(dots, DragField::kBounce, 1.7).has_value());
  QCOMPARE(history.UndoLabel(), QString("Set bounce on 6 dots"));
  for (const DragNode& node : FindRig(BobRig(history), "lower")->drag_nodes) {
    QCOMPARE(node.bounce, 1.0);
  }
  // One undo takes the whole change back.
  history.Undo();
  QCOMPARE(FindRig(BobRig(history), "lower")->drag_nodes[3].bounce, 0.5);
  QVERIFY(rig.SetPhysicsAll({dots[0], dots[1]}, true).has_value());
  QCOMPARE(history.UndoLabel(), QString("Physics on for 2 dots"));
  QVERIFY(FindRig(BobRig(history), "lower")->drag_nodes[1].physics);
  QVERIFY(!FindRig(BobRig(history), "lower")->drag_nodes[2].physics);
  // Motions: a kind starts one on each dot, a number then edits them.
  QVERIFY(rig.SetMotionAll(dots, MotionField::kKind,
                           static_cast<double>(WarpMotionKind::kWave))
              .has_value());
  QCOMPARE(FindRig(BobRig(history), "lower")->point_motions.size(),
           size_t{6});
  QVERIFY(rig.SetMotionAll(dots, MotionField::kSize, 20.0).has_value());
  QCOMPARE(history.UndoLabel(), QString("Set size on 6 dots"));
  QCOMPARE(FindRig(BobRig(history), "lower")->point_motions[2].size, 20.0);
  QVERIFY(!rig.SetMotionAll(dots, MotionField::kKind, 99.0).has_value());
  QVERIFY(rig.ClearDots({dots[0], dots[5]}).has_value());
  QCOMPARE(history.UndoLabel(), QString("Clear 2 dots"));
  QCOMPARE(FindRig(BobRig(history), "lower")->drag_nodes.size(), size_t{4});
  QCOMPARE(FindRig(BobRig(history), "lower")->point_motions.size(),
           size_t{4});
  QVERIFY(!rig.SetDragNodes({}, true).has_value());
  QVERIFY(!rig.SetDragNodes({{"Bob", "lower", 40}}, true).has_value());
}

void RigTests::MirrorNodesAndDotsOntoTheTwin() {
  Doll doll;
  doll.art.pieces = {{"arm_l", {"a.png"}, 0, {0, 0}, {20, 40}},
                     {"arm_r", {"a.png"}, 0, {60, 0}, {20, 40}},
                     {"head", {"h.png"}, 0, {30, 0}, {20, 20}}};
  doll.rig.pieces = {{"arm_l", "", {0, 0}, 0, -1, {2, 2}},
                     {"arm_r", "", {0, 0}, 0, -1, {2, 2}},
                     {"head", "", {0, 0}, 0, -1, {2, 2}}};
  RigPiece& left = doll.rig.pieces[0];
  left.drag_nodes = {{0, 0.5, 0.5, true, 0.1, 0.6, 0.5, 0.3, 0.2},
                     {1, 0.25, 0.75}};
  left.collider = {4,
                   {{{0, 0}, 0}, {{20, 0}, 1}, {{20, 40}, 2}, {{0, 40}, 3}},
                   5.0,
                   {"skirt"}};
  doll.rig.pieces[1].drag_nodes = {{8, 0.9, 0.9}};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  HistoryManager history(project);
  RigManager rig(&history);
  QVERIFY(!rig.WhyNoMirror("Bob", "head").isEmpty());
  QVERIFY(rig.WhyNoMirror("Bob", "arm_l").isEmpty());
  QVERIFY(!rig.MirrorNodes("Bob", "head").has_value());
  QVERIFY(rig.MirrorNodes("Bob", "arm_l").has_value());
  QCOMPARE(history.UndoLabel(), QString("Mirror arm_l to arm_r"));
  const RigPiece& right = *FindRig(BobRig(history), "arm_r");
  // Dots land on their mirrored points; the old node is gone.
  QCOMPARE(right.drag_nodes.size(), size_t{2});
  QCOMPARE(right.drag_nodes[0].point, 2);
  QVERIFY(right.drag_nodes[0].physics);
  QCOMPARE(right.drag_nodes[0].stretch, 0.1);
  QCOMPARE(right.drag_nodes[1].point, 1);
  // The outline is mirrored across the middle, its ring turned round.
  QCOMPARE(right.collider.ring_size, 4);
  QCOMPARE(right.collider.dots[0], (ColliderDot{{20, 40}, 0}));
  QCOMPARE(right.collider.dots[2], (ColliderDot{{0, 0}, 2}));
  QCOMPARE(right.collider.dots[3], (ColliderDot{{20, 0}, 3}));
  QCOMPARE(right.collider.spared, (std::vector<QString>{"skirt"}));
  history.Undo();
  QCOMPARE(FindRig(BobRig(history), "arm_r")->drag_nodes.size(), size_t{1});
  QVERIFY(!FindRig(BobRig(history), "arm_r")->collider.IsOn());
  // Just the picked dots: dot 0 has a node, dot 2 has none.
  QVERIFY(rig.MirrorDots({{"Bob", "arm_l", 0}, {"Bob", "arm_l", 2}})
              .has_value());
  QCOMPARE(history.UndoLabel(), QString("Mirror 2 dots to the other side"));
  const RigPiece& twin = *FindRig(BobRig(history), "arm_r");
  QCOMPARE(twin.drag_nodes.size(), size_t{2});
  QCOMPARE(twin.drag_nodes[1].point, 2);
  QVERIFY(twin.drag_nodes[1].physics);
  // A twin with another grid is refused, with the reason.
  QVERIFY(rig.SetWarpGrid("Bob", "arm_r", {3, 3}).has_value());
  QVERIFY(!rig.WhyNoMirror("Bob", "arm_l").isEmpty());
  QVERIFY(!rig.MirrorDots({{"Bob", "arm_l", 0}}).has_value());
  QVERIFY(!rig.MirrorDots({}).has_value());
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
