#include <QTest>

#include "edit/history_manager.h"
#include "edit/rig_manager.h"

namespace snapper {
namespace {

// A doll "Bob" with one piece, "lower", to put a warp grid on.
Project DotsProject() {
  Doll doll;
  doll.name = "Bob";
  doll.art.pieces = {{"lower", {"l.png"}, 0, {}, {10, 2}}};
  doll.rig.pieces = {{"lower", "", {}, 0, -1, {2, 2}}};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  assert(project.dolls.size() == 1);
  assert(project.shots.empty());
  return project;
}

const Rig& BobRig(const HistoryManager& history) {
  assert(FindDoll(history.current(), "Bob") != nullptr);
  assert(history.current().dolls.size() >= 1);
  return FindDoll(history.current(), "Bob")->rig;
}

}  // namespace

class RigDotsTests final : public QObject {
  Q_OBJECT

 private slots:
  void ManyDotsEditInOneNamedStep();
  void MirrorNodesAndDotsOntoTheTwin();
  void MathWorksOnEachDotsOwnNumber();
  void MirrorCopiesPointMotionsWithTheirDirectionFlipped();
};

void RigDotsTests::ManyDotsEditInOneNamedStep() {
  HistoryManager history(DotsProject());
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

void RigDotsTests::MirrorNodesAndDotsOntoTheTwin() {
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

void RigDotsTests::MathWorksOnEachDotsOwnNumber() {
  HistoryManager history(DotsProject());
  RigManager rig(&history);
  const std::vector<DotRef> two = {{"Bob", "lower", 0}, {"Bob", "lower", 1}};
  QVERIFY(rig.SetMotionAll(two, MotionField::kKind,
                           static_cast<double>(WarpMotionKind::kWave))
              .has_value());
  QVERIFY(rig.SetMotionAll({two[0]}, MotionField::kSize, 10.0).has_value());
  QVERIFY(rig.SetMotionAll({two[1]}, MotionField::kSize, 30.0).has_value());
  QVERIFY(rig.SetMotionMath(two, MotionField::kSize, "*2").has_value());
  const auto& motions = BobRig(history).pieces[0].point_motions;
  QCOMPARE(motions[0].size, 20.0);
  QCOMPARE(motions[1].size, 60.0);
  QCOMPARE(history.UndoLabel(), QString("Set size on 2 dots"));
  history.Undo();
  QCOMPARE(BobRig(history).pieces[0].point_motions[1].size, 30.0);
  // Text that is not math is refused and changes nothing.
  QVERIFY(!rig.SetMotionMath(two, MotionField::kSize, "oops").has_value());
  QCOMPARE(BobRig(history).pieces[0].point_motions[0].size, 10.0);
  QVERIFY(rig.SetDragNodes(two, true).has_value());
  QVERIFY(!rig.SetDragFieldMath(two, DragField::kLag, "/0").has_value());
  QVERIFY(rig.SetDragFieldMath(two, DragField::kLag, "+0.25").has_value());
  QCOMPARE(BobRig(history).pieces[0].drag_nodes[1].lag, 0.75);
}

void RigDotsTests::MirrorCopiesPointMotionsWithTheirDirectionFlipped() {
  Doll doll;
  doll.art.pieces = {{"arm_l", {"a.png"}, 0, {0, 0}, {20, 40}},
                     {"arm_r", {"a.png"}, 0, {60, 0}, {20, 40}}};
  doll.rig.pieces = {{"arm_l", "", {0, 0}, 0, -1, {2, 2}},
                     {"arm_r", "", {0, 0}, 0, -1, {2, 2}}};
  doll.rig.pieces[0].point_motions = {
      {0, WarpMotionKind::kWave, 8.0, 24, 30.0, 0.25},
      {1, WarpMotionKind::kPulse, 4.0, 12, 90.0, 0.0}};
  doll.rig.pieces[1].point_motions = {
      {5, WarpMotionKind::kShiver, 2.0, 6, 0.0, 0.0}};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  HistoryManager history(project);
  RigManager rig(&history);
  // The whole piece: motions land on mirrored points, left and right
  // swapped; the twin's old motion is gone.
  QVERIFY(rig.MirrorNodes("Bob", "arm_l").has_value());
  const auto& motions = BobRig(history).pieces[1].point_motions;
  QCOMPARE(motions.size(), size_t{2});
  QCOMPARE(motions[0],
           (PointMotion{2, WarpMotionKind::kWave, 8.0, 24, 150.0, 0.25}));
  QCOMPARE(motions[1],
           (PointMotion{1, WarpMotionKind::kPulse, 4.0, 12, 90.0, 0.0}));
  history.Undo();
  // Just picked dots: dot 0 has a wave, dot 3 has none.
  QVERIFY(rig.MirrorDots({{"Bob", "arm_l", 0}, {"Bob", "arm_l", 3}})
              .has_value());
  const auto& some = BobRig(history).pieces[1].point_motions;
  QCOMPARE(some.size(), size_t{1});
  const PointMotion* wave = FindMotion(BobRig(history).pieces[1], 2);
  QVERIFY(wave != nullptr);
  QCOMPARE(wave->angle, 150.0);
  // The twin's own motion on point 5 (the mirror of dot 3) was cleared.
  QVERIFY(FindMotion(BobRig(history).pieces[1], 5) == nullptr);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::RigDotsTests)
#include "rig_dots_tests.moc"
