#include <QTest>

#include <cassert>
#include <cmath>

#include "anim/links.h"
#include "anim/warp.h"
#include "bench.h"
#include "pose_stage.h"
#include "render/stage_geometry.h"
#include "ui/pose_tool.h"

namespace snapper {
namespace {

using posestage::kFrame;
using posestage::Stage;

}  // namespace

class PoseToolDotsTests final : public QObject {
  Q_OBJECT

 private slots:
  void ABoxPicksWarpDotsAndShiftAndCtrlEditThePick();
  void DAndDeleteActOnEveryPickedDot();
  void TheGridShowsTheDotsWhereTheDrawingHasThem();
};

void PoseToolDotsTests::ABoxPicksWarpDotsAndShiftAndCtrlEditThePick() {
  Bench bench;
  Stage(&bench);
  Project project = bench.history.current();
  Doll doll = *project.dolls.at("Dot");
  doll.rig.pieces[0].warp = {2, 2};
  project.dolls["Dot"] = std::make_shared<const Doll>(doll);
  bench.history.Reset(project);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  // Pick the body, so its grid shows.
  tool.Press({60, 60}, false, false, kFrame);
  tool.Release();
  const auto warp = PieceWarpOnScreen(
      bench.history.current(), *bench.history.current().shots[0], LayerId(1),
      "body", Frame(0), 1.0);
  QVERIFY(warp.has_value());
  // box starts well outside the grid and reaches in.
  const QPointF empty(2, 2);
  const auto drag_box = [&](QPointF from, QPointF to, bool shift, bool ctrl) {
    tool.Press(from, shift, ctrl, kFrame);
    QVERIFY(tool.IsBoxing());
    tool.Move(to, false);
    QVERIFY(tool.Box().width() > 0.0);
    tool.Release();
  };
  // A box round the whole grid picks all nine dots, not pieces.
  drag_box({36, 36}, {84, 84}, false, false);
  QCOMPARE(bench.selection.dots().size(), size_t{9});
  QCOMPARE(bench.selection.pieces(), std::set<QString>({"body"}));
  // Ctrl takes out the four it covers.
  drag_box({36, 36}, {65, 65}, false, true);

  QCOMPARE(bench.selection.dots().size(), size_t{5});
  // A plain box replaces the pick; Shift adds to it.
  drag_box({36, 36}, {55, 55}, false, false);
  QCOMPARE(bench.selection.dots(),
           (std::vector<WarpDot>{{LayerId(1), "body", 0}}));
  drag_box({84, 84}, {65, 65}, true, false);
  QCOMPARE(bench.selection.dots().size(), size_t{2});
  // Escape in the middle of a box changes nothing.
  tool.Press({36, 84}, false, true, kFrame);
  tool.Move({55, 55}, false);
  tool.Cancel();
  QCOMPARE(bench.selection.dots().size(), size_t{2});
  // A box over no dots still picks pieces, as before.
  tool.Press(empty, false, false, kFrame);
  tool.Move(empty + QPointF(4, 4), false);
  tool.Release();
  QVERIFY(bench.selection.dots().empty());
  QVERIFY(bench.selection.pieces().empty());
}

void PoseToolDotsTests::DAndDeleteActOnEveryPickedDot() {
  Bench bench;
  Stage(&bench);
  Project project = bench.history.current();
  Doll doll = *project.dolls.at("Dot");
  doll.rig.pieces[0].warp = {2, 2};
  project.dolls["Dot"] = std::make_shared<const Doll>(doll);
  bench.history.Reset(project);
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press({60, 60}, false, false, kFrame);
  tool.Release();
  tool.Press({36, 36}, false, false, kFrame);
  tool.Move({84, 84}, false);
  tool.Release();
  QCOMPARE(bench.selection.dots().size(), size_t{9});
  const auto drags = [&bench] {
    return bench.history.current().dolls.at("Dot")->rig.pieces[0].drag_nodes;
  };
  tool.ToggleDrag(kFrame);
  QCOMPARE(drags().size(), size_t{9});
  QCOMPARE(bench.history.UndoLabel(), QString("Drag 9 dots"));
  // All drag now, so D stops them all.
  tool.ToggleDrag(kFrame);
  QVERIFY(drags().empty());
  tool.ToggleDrag(kFrame);
  QVERIFY(tool.ClearDots(kFrame));
  QVERIFY(drags().empty());
  QCOMPARE(bench.history.UndoLabel(), QString("Clear 9 dots"));
  bench.history.Undo();
  QCOMPARE(drags().size(), size_t{9});
  bench.selection.PickDot(std::nullopt);
  QVERIFY(!tool.ClearDots(kFrame));
}

void PoseToolDotsTests::TheGridShowsTheDotsWhereTheDrawingHasThem() {
  Bench bench;
  Stage(&bench);
  Project project = bench.history.current();
  Doll doll = *project.dolls.at("Dot");
  // The middle dot sways by itself, so it is not where its key is.
  doll.rig.pieces[0].warp = {2, 2};
  doll.rig.pieces[0].point_motions = {
      {4, WarpMotionKind::kSway, 4.0, 24, 0.0, 0.0}};
  project.dolls["Dot"] = std::make_shared<const Doll>(doll);
  bench.history.Reset(project);
  const StageFrame at{ShotId(1), Frame(6), 1.0, QPointF(10, 10)};
  const auto shown = [&bench, &at] {
    const Project& now = bench.history.current();
    return PieceWarpOnScreen(now, *now.shots[0], LayerId(1), "body",
                             at.local, at.scale);
  };
  const auto evaluated = [&bench, &at] {
    const Project& now = bench.history.current();
    LinkSolver solver(now, *now.shots[0], nullptr);
    return WarpedPoints({20, 20}, {2, 2},
                        solver.Poses(*now.dolls.at("Dot"),
                                     now.shots[0]->layers[0], at.local)
                            .at("body")
                            .warp);
  };
  auto warp = shown();
  QVERIFY(warp.has_value());
  // The dots are drawn where the drawing has them; the keys alone (the
  // faint rings) are at rest.
  QCOMPARE(warp->points, evaluated());
  QCOMPARE(warp->keyed, WarpedPoints({20, 20}, {2, 2}, {}));
  QVERIFY(std::abs(warp->points[4].x() - warp->keyed[4].x()) > 1.0);
  // Dragging the dot where it shows moves its key by the drag.
  ImageCache cache;
  PoseTool tool(bench.All(), &cache);
  tool.Press({60, 60}, false, false, at);
  tool.Release();
  const QPointF spot = warp->to_screen.map(warp->points[4]) + at.corner;
  tool.Press(spot, false, false, at);
  tool.Move(spot + QPointF(6, 0), false);
  tool.Release();
  warp = shown();
  QVERIFY(warp.has_value());
  QCOMPARE(warp->keyed[4], QPointF(16, 10));
  QCOMPARE(warp->points, evaluated());
}

}  // namespace snapper

QTEST_MAIN(snapper::PoseToolDotsTests)
#include "pose_tool_dots_tests.moc"
