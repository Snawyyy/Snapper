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
}

void RigTests::NewWarpGridClearsOldWarpKeys() {
  HistoryManager history(ArmProject());
  RigManager rig(&history);
  QVERIFY(!rig.SetWarpGrid("Bob", "lower", {0, 3}).has_value());
  QVERIFY(!rig.SetWarpGrid("Bob", "lower", {9, 1}).has_value());
  QVERIFY(rig.SetWarpGrid("Bob", "lower", {3, 1}).has_value());
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

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::RigTests)
#include "rig_tests.moc"
