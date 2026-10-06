#include <QRectF>
#include <QTest>

#include <cmath>

#include "anim/doll_lean.h"
#include "anim/doll_pose.h"

namespace snapper {
namespace {

// A torso with a head on top and a leg below, 60 pixels tall in all:
// the head's joint is a third of the way up from the middle, the leg's
// a third of the way down, the torso's on the middle line.
Doll Body() {
  Doll doll;
  doll.art.pieces = {{"torso", {"t.png"}, 0, {0, 20}, {20, 20}},
                     {"head", {"h.png"}, 0, {5, 0}, {10, 20}},
                     {"leg", {"l.png"}, 0, {5, 40}, {10, 20}}};
  doll.rig.pieces = {{"torso", "", {10, 10}, 1, -1, {}},
                     {"head", "torso", {5, 20}, 2, -1, {}},
                     {"leg", "torso", {5, 0}, 0, -1, {}}};
  return doll;
}

QRectF BoxOf(const Doll& doll, const std::map<QString, QTransform>& placed) {
  QRectF box;
  for (const auto& [name, transform] : placed) {
    box = box.united(transform.mapRect(
        QRectF(QPointF(), QSizeF(FindArt(doll, name)->size))));
  }
  return box;
}

bool IsSame(const QTransform& a, const QTransform& b) {
  return std::abs(a.m11() - b.m11()) < 1e-9 &&
         std::abs(a.m12() - b.m12()) < 1e-9 &&
         std::abs(a.m21() - b.m21()) < 1e-9 &&
         std::abs(a.m22() - b.m22()) < 1e-9 &&
         std::abs(a.dx() - b.dx()) < 1e-9 && std::abs(a.dy() - b.dy()) < 1e-9;
}

// How much bigger after draws than before; 0 when it is not the same
// drawing scaled evenly, turned and skewed alike.
double GrowthOf(const QTransform& before, const QTransform& after) {
  const double grow = after.m11() / before.m11();
  const bool is_even = std::abs(after.m12() - before.m12() * grow) < 1e-9 &&
                       std::abs(after.m21() - before.m21() * grow) < 1e-9 &&
                       std::abs(after.m22() - before.m22() * grow) < 1e-9;
  return is_even ? grow : 0.0;
}

}  // namespace

class LeanTests final : public QObject {
  Q_OBJECT

 private slots:
  void NothingLeansAtZero();
  void LeaningInDropsAndGrowsTheTop();
  void TheAmountStaysInRange();
  void TheNearPartComesInFront();
};

void LeanTests::NothingLeansAtZero() {
  PoseMap poses;
  poses["head"].rotation = 15.0;
  const auto before = PieceTransforms(Body(), poses);
  const auto after = PieceTransforms(Body(), LeanPoses(Body(), poses, 0.0));
  for (const auto& [name, transform] : before) {
    QVERIFY(IsSame(after.at(name), transform));
  }
}

void LeanTests::LeaningInDropsAndGrowsTheTop() {
  const Doll doll = Body();
  PoseMap poses;
  poses["torso"].rotation = 20.0;
  poses["torso"].offset = QPointF(3, -2);
  poses["head"].rotation = -10.0;
  poses["head"].scale_x = 1.5;
  poses["leg"].skew = 12.0;
  const auto before = PieceTransforms(doll, poses);
  const QPointF middle = BoxOf(doll, before).center();
  const auto joint = [&doll](const std::map<QString, QTransform>& placed,
                             const QString& name) {
    return placed.at(name).map(FindRig(doll.rig, name)->pivot);
  };
  // Leaning in: the head comes nearer, so it grows, yet seen tipped it
  // drops toward the middle; the leg goes back, shrinks and rises.
  const auto in = PieceTransforms(doll, LeanPoses(doll, poses, 0.6));
  QVERIFY(GrowthOf(before.at("head"), in.at("head")) > 1.0);
  QVERIFY(GrowthOf(before.at("leg"), in.at("leg")) < 1.0);
  QVERIFY(GrowthOf(before.at("leg"), in.at("leg")) > 0.0);
  QVERIFY(joint(in, "head").y() > joint(before, "head").y());
  QVERIFY(joint(in, "leg").y() < joint(before, "leg").y());
  // The torso's joint sits on the middle line: neither nearer nor
  // further, its height off the middle only squashed.
  QCOMPARE(GrowthOf(before.at("torso"), in.at("torso")), 1.0);
  QVERIFY(std::abs(joint(in, "torso").x() - joint(before, "torso").x()) <
          1e-9);
  QVERIFY(std::abs(joint(in, "torso").y() - middle.y()) <
          std::abs(joint(before, "torso").y() - middle.y()) + 1e-9);
  // Leaning back shrinks the top instead.
  const auto back = PieceTransforms(doll, LeanPoses(doll, poses, -0.6));
  QVERIFY(GrowthOf(before.at("head"), back.at("head")) < 1.0);
  QVERIFY(GrowthOf(before.at("leg"), back.at("leg")) > 1.0);
  QCOMPARE(LeanPoses(doll, poses, 0.6).at("head").drawing, -1);
}

void LeanTests::TheAmountStaysInRange() {
  const Doll doll = Body();
  const PoseMap far = LeanPoses(doll, {}, 50.0);
  const PoseMap most = LeanPoses(doll, {}, kMaxLean);
  QCOMPARE(far.at("leg").scale_x, most.at("leg").scale_x);
  QVERIFY(most.at("leg").scale_x > 0.0);
}

void LeanTests::TheNearPartComesInFront() {
  Doll doll = Body();
  // At rest the leg is drawn over everything.
  doll.rig.pieces[2].order = 5;
  const auto names = [&doll](const PoseMap& poses) {
    QStringList order;
    for (const PlacedPiece& piece : PlaceDoll(doll, poses)) {
      order.append(piece.name);
    }
    return order;
  };
  QCOMPARE(names({}), QStringList({"torso", "head", "leg"}));
  QCOMPARE(names(LeanPoses(doll, {}, 0.0)), names({}));
  // Leaning in brings the top nearer, so the head comes in front;
  // leaning back brings the leg forward.
  QCOMPARE(names(LeanPoses(doll, {}, 0.3)),
           QStringList({"leg", "torso", "head"}));
  QCOMPARE(names(LeanPoses(doll, {}, -0.3)),
           QStringList({"head", "torso", "leg"}));
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::LeanTests)
#include "lean_tests.moc"
