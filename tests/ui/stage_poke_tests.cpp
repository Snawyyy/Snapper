#include <QTest>

#include <cassert>
#include <memory>

#include "bench.h"
#include "ui/stage_poke.h"

namespace snapper {
namespace {

const StageFrame kFrame{ShotId(1), Frame(80), 2.0, QPointF(10, 10)};

// Layer 1 is a jelly chest, layer 2 a doll with no springs.
Project Toys() {
  Doll chest;
  chest.name = "Chest";
  chest.art.pieces = {{"chest", {"c.png"}, 0, {0, 0}, {20, 20}}};
  chest.rig.pieces = {{"chest", "", {10, 0}, 0, -1, {2, 2}}};
  for (int point = 3; point < 9; ++point) {
    DragNode node{point};
    node.kind = NodeKind::kJelly;
    chest.rig.pieces[0].drag_nodes.push_back(node);
  }
  Doll plain = chest;
  plain.name = "Plain";
  plain.rig.pieces[0].drag_nodes.clear();
  Project project;
  project.dolls["Chest"] = std::make_shared<const Doll>(chest);
  project.dolls["Plain"] = std::make_shared<const Doll>(plain);
  Shot shot;
  shot.id = ShotId(1);
  shot.length = Frame(400);
  for (int i = 0; i < 2; ++i) {
    Layer layer;
    layer.id = LayerId(i + 1);
    layer.content = DollLayer{i == 0 ? "Chest" : "Plain", {}, false};
    shot.layers.push_back(layer);
  }
  project.shots = {std::make_shared<const Shot>(shot)};
  assert(project.shots[0]->layers.size() == 2);
  assert(FindDoll(project, "Plain") != nullptr);
  return project;
}

QString LastToast(const Bench& bench) {
  assert(!bench.dir.path().isEmpty());
  assert(bench.toasts.queue().toasts().size() < 1000);
  const auto& toasts = bench.toasts.queue().toasts();
  return toasts.empty() ? QString() : toasts.back().text;
}

}  // namespace

class StagePokeTests final : public QObject {
  Q_OBJECT

 private slots:
  void POverEmptyStageSaysToHoverADoll();
  void POverADollWithoutSpringsSaysWhy();
  void HandsShowWhereAPokeCanStart();
  void APressShovesTheDollAndTheMemoShowsIt();
  void AClickWithoutPEndsAPoke();
  void EscapeEndsAPoke();
  void PokingLeavesTheProjectAlone();
};

void StagePokeTests::POverEmptyStageSaysToHoverADoll() {
  Bench bench;
  bench.history.Reset(Toys());
  StagePoke poke(bench.All());
  poke.Arm(kFrame, LayerId());
  QVERIFY(poke.IsArmed());
  QCOMPARE(LastToast(bench), QString("Hover a doll to poke it."));
  QVERIFY(poke.Press(QPointF(50, 50), LayerId(), kFrame));
  QVERIFY(!bench.poke.IsActive());
}

void StagePokeTests::POverADollWithoutSpringsSaysWhy() {
  Bench bench;
  bench.history.Reset(Toys());
  StagePoke poke(bench.All());
  poke.Arm(kFrame, LayerId(2));
  QVERIFY(LastToast(bench).contains("no drag"));
  QVERIFY(poke.Press(QPointF(50, 50), LayerId(2), kFrame));
  QVERIFY(!bench.poke.IsActive());
}

void StagePokeTests::HandsShowWhereAPokeCanStart() {
  Bench bench;
  bench.history.Reset(Toys());
  StagePoke poke(bench.All());
  QCOMPARE(poke.CursorOver(LayerId(1), kFrame), Qt::ArrowCursor);
  poke.Arm(kFrame, LayerId(1));
  QCOMPARE(poke.CursorOver(LayerId(1), kFrame), Qt::OpenHandCursor);
  QCOMPARE(poke.CursorOver(LayerId(2), kFrame), Qt::ArrowCursor);
  QCOMPARE(poke.CursorOver(LayerId(), kFrame), Qt::ArrowCursor);
  QVERIFY(poke.Press(QPointF(50, 50), LayerId(1), kFrame));
  QCOMPARE(poke.CursorOver(LayerId(1), kFrame), Qt::ClosedHandCursor);
  poke.Release();
  QCOMPARE(poke.CursorOver(LayerId(1), kFrame), Qt::OpenHandCursor);
}

void StagePokeTests::APressShovesTheDollAndTheMemoShowsIt() {
  Bench bench;
  bench.history.Reset(Toys());
  StagePoke poke(bench.All());
  PoseMemo memo;
  poke.Arm(kFrame, LayerId(1));
  QVERIFY(poke.Press(QPointF(50, 50), LayerId(1), kFrame));
  QVERIFY(poke.IsPoking());
  QVERIFY(LastToast(bench).isEmpty());
  poke.Move(QPointF(250, 50), kFrame);
  for (int i = 0; i < 6; ++i) {
    bench.poke.Tick();
  }
  poke.Sync(&memo);
  QVERIFY(memo.OverrideOf(LayerId(1)) != nullptr);
  QVERIFY(memo.OverrideOf(LayerId(2)) == nullptr);
  poke.Release();
  poke.Disarm();
  QVERIFY(!poke.IsArmed());
  bench.poke.End();
  poke.Sync(&memo);
  QVERIFY(memo.OverrideOf(LayerId(1)) == nullptr);
}

void StagePokeTests::AClickWithoutPEndsAPoke() {
  Bench bench;
  bench.history.Reset(Toys());
  StagePoke poke(bench.All());
  poke.Arm(kFrame, LayerId(1));
  QVERIFY(poke.Press(QPointF(50, 50), LayerId(1), kFrame));
  poke.Release();
  poke.Disarm();
  QVERIFY(bench.poke.IsActive());
  // The next click is a normal one: it ends the poke, the tool gets it.
  QVERIFY(!poke.Press(QPointF(50, 50), LayerId(1), kFrame));
  QVERIFY(!bench.poke.IsActive());
}

void StagePokeTests::EscapeEndsAPoke() {
  Bench bench;
  bench.history.Reset(Toys());
  StagePoke poke(bench.All());
  QVERIFY(!poke.Cancel());
  poke.Arm(kFrame, LayerId(1));
  QVERIFY(poke.Press(QPointF(50, 50), LayerId(1), kFrame));
  QVERIFY(poke.Cancel());
  QVERIFY(!bench.poke.IsActive());
  QVERIFY(!poke.Cancel());
}

void StagePokeTests::PokingLeavesTheProjectAlone() {
  Bench bench;
  bench.history.Reset(Toys());
  const Project before = bench.history.current();
  StagePoke poke(bench.All());
  poke.Arm(kFrame, LayerId(1));
  QVERIFY(poke.Press(QPointF(50, 50), LayerId(1), kFrame));
  poke.Move(QPointF(300, 90), kFrame);
  for (int i = 0; i < 10; ++i) {
    bench.poke.Tick();
  }
  poke.Release();
  poke.Disarm();
  QVERIFY(bench.history.current() == before);
  QVERIFY(!bench.history.CanUndo());
}

}  // namespace snapper

QTEST_MAIN(snapper::StagePokeTests)
#include "stage_poke_tests.moc"
