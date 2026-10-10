#include <QTest>

#include <cassert>
#include <cmath>
#include <variant>

#include "anim/doll_lean.h"
#include "anim/poke_layer.h"
#include "anim/sampler.h"

namespace snapper {
namespace {

// A 20 by 20 chest on a 2 by 2 grid: the top row hangs it from the
// body, the six points below are jelly.
Doll Chest() {
  Doll doll;
  doll.art.pieces = {{"chest", {"c.png"}, 0, {0, 0}, {20, 20}}};
  doll.rig.pieces = {{"chest", "", {10, 0}, 0, -1, {2, 2}}};
  for (int point = 3; point < 9; ++point) {
    DragNode node{point};
    node.kind = NodeKind::kJelly;
    doll.rig.pieces[0].drag_nodes.push_back(node);
  }
  assert(doll.rig.pieces[0].warp.PointCount() == 9);
  assert(doll.rig.pieces[0].drag_nodes.size() == 6);
  return doll;
}

// Moves right at frame 2, rotates a piece at frame 6, eases at 8.
Layer Walker() {
  Layer layer;
  layer.content = DollLayer{"Chest", {}, false};
  PiecePose moved;
  moved.offset = QPointF(12, 0);
  SetKey(&layer.transform, {Frame(0), PiecePose(), Ease::kStep});
  SetKey(&layer.transform, {Frame(2), moved, Ease::kStep});
  SetKey(&layer.transform, {Frame(8), PiecePose(), Ease::kLinear});
  PiecePose turned;
  turned.rotation = 30.0;
  auto& pieces = std::get<DollLayer>(layer.content).pieces;
  SetKey(&pieces["chest"], {Frame(0), PiecePose(), Ease::kStep});
  SetKey(&pieces["chest"], {Frame(6), turned, Ease::kStep});
  assert(layer.transform.keys.size() == 3);
  assert(pieces["chest"].keys.size() == 2);
  return layer;
}

// Hops right at frame 2 and stays: the jelly has settled by frame 80.
Layer Hop() {
  Layer layer;
  layer.content = DollLayer{"Chest", {}, false};
  PiecePose moved;
  moved.offset = QPointF(12, 0);
  SetKey(&layer.transform, {Frame(0), PiecePose(), Ease::kStep});
  SetKey(&layer.transform, {Frame(2), moved, Ease::kStep});
  assert(layer.transform.keys.size() == 2);
  assert(std::holds_alternative<DollLayer>(layer.content));
  return layer;
}

double Wobble(const PoseMap& poses) {
  assert(poses.size() <= 16);
  assert(Frame(0).index() == 0);
  const auto found = poses.find("chest");
  double most = 0.0;
  const bool has_warp = found != poses.end();
  for (size_t i = 0; has_warp && i < found->second.warp.size(); ++i) {
    most = std::max(most, std::hypot(found->second.warp[i].x(),
                                     found->second.warp[i].y()));
  }
  return most;
}

// The poke of layer at frame at: one shove, then calm for the rest of
// frames, shown at the last virtual frame.
PoseMap Poked(const Doll& doll, const Layer& layer, int at, QPointF shove,
              int frames, DragTrail* trail = nullptr) {
  assert(frames >= 1);
  assert(at >= 0);
  std::vector<QPointF> shoves(static_cast<size_t>(frames));
  shoves[0] = shove;
  const Layer poke = PokeLayer(layer, Frame(at), shoves);
  return ShownPoses(doll, poke, Frame(at + frames), {}, trail);
}

}  // namespace

class PokeLayerTests final : public QObject {
  Q_OBJECT

 private slots:
  void KeepsKeysBeforeAndHoldsThePoseAfter();
  void ShovesTheLayersOwnMoveFrameByFrame();
  void LeavesTheOriginalAlone();
  void ShoveIsSmallAndCapped();
  void APokedJellyWobblesThenSettlesToTheHeldPose();
  void NoShoveIsJustTheHeldPose();
};

void PokeLayerTests::KeepsKeysBeforeAndHoldsThePoseAfter() {
  const Layer layer = Walker();
  const Layer poke = PokeLayer(layer, Frame(5), {});
  // Keys 0 and 2 stay; frame 5 holds the sampled pose; 8 is gone.
  QCOMPARE(poke.transform.keys.size(), size_t(3));
  QCOMPARE(poke.transform.keys[0], layer.transform.keys[0]);
  QCOMPARE(poke.transform.keys[1], layer.transform.keys[1]);
  QCOMPARE(poke.transform.keys[2].frame.index(), 5);
  QCOMPARE(poke.transform.keys[2].value.offset, QPointF(12, 0));
  const auto& chest = std::get<DollLayer>(poke.content).pieces.at("chest");
  QCOMPARE(chest.keys.size(), size_t(2));
  QCOMPARE(chest.keys[1].frame.index(), 5);
  QCOMPARE(chest.keys[1].value.rotation, 0.0);
  QCOMPARE(Sample(poke.transform, Frame(40), PiecePose()).offset,
           QPointF(12, 0));
}

void PokeLayerTests::ShovesTheLayersOwnMoveFrameByFrame() {
  const Layer poke = PokeLayer(
      Walker(), Frame(5), {QPointF(4, 0), QPointF(2, 1), QPointF()});
  QCOMPARE(poke.transform.keys.size(), size_t(6));
  QCOMPARE(poke.transform.keys[3].frame.index(), 6);
  QCOMPARE(poke.transform.keys[3].value.offset, QPointF(16, 0));
  QCOMPARE(poke.transform.keys[4].value.offset, QPointF(14, 1));
  QCOMPARE(poke.transform.keys[5].frame.index(), 8);
  QCOMPARE(poke.transform.keys[5].value.offset, QPointF(12, 0));
}

void PokeLayerTests::LeavesTheOriginalAlone() {
  const Layer layer = Walker();
  const Layer copy = layer;
  const Layer poke = PokeLayer(layer, Frame(3), {QPointF(9, 9)});
  QVERIFY(layer == copy);
  QVERIFY(!(poke == layer));
}

void PokeLayerTests::ShoveIsSmallAndCapped() {
  QCOMPARE(PokeShove(QPointF(20, 0)), QPointF(10, 0));
  const QPointF far = PokeShove(QPointF(5000, 0));
  QCOMPARE(far.x(), kPokeReach);
  QVERIFY(std::abs(far.y()) < 1e-9);
}

void PokeLayerTests::APokedJellyWobblesThenSettlesToTheHeldPose() {
  const Doll doll = Chest();
  const Layer layer = Hop();
  // Poked at frame 80, long after the keyed motion has settled.
  const PoseMap held = Poked(doll, layer, 80, QPointF(), 90);
  const double rest = Wobble(held);
  QVERIFY2(rest < 0.02, qPrintable(QString::number(rest)));
  const PoseMap hit = Poked(doll, layer, 80, QPointF(60, 0), 2);
  QVERIFY2(Wobble(hit) > 1.0, qPrintable(QString::number(Wobble(hit))));
  // Later frames of the same poke wobble and then come back.
  const PoseMap settled = Poked(doll, layer, 80, QPointF(60, 0), 140);
  QVERIFY(PoseDistance(settled, held) < 0.05);
  // The same poke through a trail gives the same poses.
  DragTrail trail;
  Poked(doll, layer, 80, QPointF(60, 0), 30, &trail);
  const PoseMap resumed = Poked(doll, layer, 80, QPointF(60, 0), 140, &trail);
  QVERIFY(PoseDistance(resumed, settled) < 1e-9);
}

void PokeLayerTests::NoShoveIsJustTheHeldPose() {
  const Doll doll = Chest();
  const Layer layer = Hop();
  const PoseMap shown = ShownPoses(doll, layer, Frame(80));
  const PoseMap held = Poked(doll, layer, 80, QPointF(), 12);
  QVERIFY(!shown.empty());
  const double gap = PoseDistance(shown, held);
  QVERIFY2(gap < 0.05, qPrintable(QString::number(gap)));
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::PokeLayerTests)
#include "poke_layer_tests.moc"
