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

// Scaling by factor around middle, after transform.
QTransform ScaledAround(const QTransform& transform, double factor,
                        QPointF middle) {
  return transform * QTransform::fromTranslate(-middle.x(), -middle.y()) *
         QTransform::fromScale(factor, factor) *
         QTransform::fromTranslate(middle.x(), middle.y());
}

}  // namespace

class LeanTests final : public QObject {
  Q_OBJECT

 private slots:
  void NothingLeansAtZero();
  void EachPieceScalesAroundTheMiddleByItsWeight();
  void TheAmountStaysInRange();
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

void LeanTests::EachPieceScalesAroundTheMiddleByItsWeight() {
  const Doll doll = Body();
  PoseMap poses;
  poses["torso"].rotation = 20.0;
  poses["torso"].offset = QPointF(3, -2);
  poses["head"].rotation = -10.0;
  poses["head"].scale_x = 1.5;
  poses["leg"].skew = 12.0;
  const auto before = PieceTransforms(doll, poses);
  const QPointF middle = BoxOf(doll, before).center();
  // Down (above 0) tips the top toward the camera: the head grows, the
  // torso on the middle line stays, the leg shrinks.
  for (const double amount : {0.6, -0.45}) {
    const PoseMap leaned = LeanPoses(doll, poses, amount);
    const auto after = PieceTransforms(doll, leaned);
    QVERIFY(IsSame(after.at("torso"), before.at("torso")));
    QVERIFY(IsSame(after.at("head"),
                   ScaledAround(before.at("head"), 1.0 + amount / 3.0,
                                middle)));
    QVERIFY(IsSame(after.at("leg"),
                   ScaledAround(before.at("leg"), 1.0 - amount / 3.0,
                                middle)));
    QCOMPARE(leaned.at("head").rotation, -10.0);
  }
}

void LeanTests::TheAmountStaysInRange() {
  const Doll doll = Body();
  const PoseMap far = LeanPoses(doll, {}, 50.0);
  const PoseMap most = LeanPoses(doll, {}, kMaxLean);
  QCOMPARE(far.at("leg").scale_x, most.at("leg").scale_x);
  QVERIFY(most.at("leg").scale_x > 0.0);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::LeanTests)
#include "lean_tests.moc"
