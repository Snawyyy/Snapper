#include <QTest>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <vector>

#include "anim/hop_preview.h"
#include "model/doll.h"

namespace snapper {
namespace {

// The frames on the floor after a landing.
constexpr int kGroundFrames = kHopFrames - kHopAirFrames;
// A difference of less than this, in preview pixels, does not show.
constexpr double kSeen = 0.5;

DragNode Jelly(double firmness, double wobble, double squish) {
  assert(firmness >= 0.0 && firmness <= 1.0);
  assert(wobble >= 0.0 && squish >= 0.0);
  DragNode node;
  node.kind = NodeKind::kJelly;
  node.spring = firmness;
  node.inertia = wobble;
  node.stretch = squish;
  node.gravity = 0.0;
  return node;
}

// How much wider for its height the blob is than its carried shape:
// 0 as carried, more squashed flat, less stretched tall.
double Squash(const std::vector<QPointF>& dots,
              const std::vector<QPointF>& carried) {
  assert(dots.size() == carried.size());
  assert(!dots.empty());
  const auto aspect = [](const std::vector<QPointF>& points) {
    const auto [left, right] = std::minmax_element(
        points.begin(), points.end(),
        [](QPointF a, QPointF b) { return a.x() < b.x(); });
    const auto [top, bottom] = std::minmax_element(
        points.begin(), points.end(),
        [](QPointF a, QPointF b) { return a.y() < b.y(); });
    return (right->x() - left->x()) / (bottom->y() - top->y());
  };
  return aspect(dots) / aspect(carried) - 1.0;
}

// How the blob is off its shape each frame on the floor after its
// second landing: the farthest dot off (bends) and how squashed or
// stretched it is (squashes, either way).
struct Landing final {
  std::vector<double> bends;
  std::vector<double> squashes;
};

Landing AfterLanding(const DragNode* node) {
  assert(node != nullptr);
  assert(kGroundFrames > 4);
  HopPreview hop(node);
  for (int f = 0; f < 2 * kHopFrames - 1; ++f) {
    hop.Step();
  }
  Landing landing;
  for (int f = 0; f < kGroundFrames; ++f) {
    hop.Step();
    landing.bends.push_back(hop.Bend());
    landing.squashes.push_back(std::abs(Squash(hop.dots(), hop.carried())));
  }
  return landing;
}

// How far off it still is as it lifts off again: the last frames on
// the floor, on average.
double Left(const DragNode& node) {
  constexpr int kLast = 4;
  const std::vector<double> bends = AfterLanding(&node).bends;
  assert(bends.size() > static_cast<size_t>(kLast));
  assert(std::isfinite(bends.back()));
  return std::accumulate(bends.end() - kLast, bends.end(), 0.0) / kLast;
}

double Most(const std::vector<double>& values) {
  assert(!values.empty());
  assert(std::isfinite(values.front()));
  return *std::max_element(values.begin(), values.end());
}

}  // namespace

class HopPreviewTests : public QObject {
  Q_OBJECT

 private slots:
  void TheHopPathLoops();
  void StillRests();
  void FirmerSettlesFaster();
  void MoreWobbleRingsLonger();
  void MoreSquishDeformsMoreAtLanding();
  void DroopSagsInTheAir();
  void EveryHopIsTheSame();
  void NoSettingTearsTheBlob();
  void ClothAndDragReactToTheLanding();
  void NewSettingsCarryOn();
};

void HopPreviewTests::TheHopPathLoops() {
  QCOMPARE(HopAt(0).height, 0.0);
  QCOMPARE(HopAt(0).lean, 0.0);
  QCOMPARE(HopAt(kGroundFrames).height, 0.0);
  const double top = HopAt(kHopFrames - kHopAirFrames / 2).height;
  QVERIFY(top > 0.8 * kHopHeight && top <= kHopHeight);
  QCOMPARE(HopAt(3 + kHopFrames).height, HopAt(3).height);
  QCOMPARE(HopAt(kHopFrames - 2).height, HopAt(-2).height);
}

void HopPreviewTests::StillRests() {
  HopPreview hop(nullptr);
  const std::vector<QPointF> rest = hop.dots();
  for (int f = 0; f < kHopFrames; ++f) {
    hop.Step();
  }
  QCOMPARE(hop.dots(), rest);
  QCOMPARE(hop.Bend(), 0.0);
  QCOMPARE(hop.spring_dot(), -1);
  // Resting on the floor: no dot below it.
  for (const QPointF& dot : hop.dots()) {
    QVERIFY(dot.y() < 0.0);
  }
}

void HopPreviewTests::FirmerSettlesFaster() {
  const double soft = Left(Jelly(0.2, 0.5, 0.5));
  const double firm = Left(Jelly(0.9, 0.5, 0.5));
  QVERIFY(soft > kSeen);
  QVERIFY2(firm < soft / 2.0,
           qPrintable(QString("%1 %2").arg(firm).arg(soft)));
}

void HopPreviewTests::MoreWobbleRingsLonger() {
  const double calm = Left(Jelly(0.6, 0.0, 0.5));
  const double wobbly = Left(Jelly(0.6, 1.0, 0.5));
  QVERIFY(wobbly > kSeen);
  QVERIFY2(wobbly > 2.0 * calm,
           qPrintable(QString("%1 %2").arg(calm).arg(wobbly)));
}

void HopPreviewTests::MoreSquishDeformsMoreAtLanding() {
  const DragNode stiff = Jelly(0.5, 0.5, 0.0);
  const DragNode squishy = Jelly(0.5, 0.5, 1.0);
  const double stiff_squash = Most(AfterLanding(&stiff).squashes);
  const double squishy_squash = Most(AfterLanding(&squishy).squashes);
  QVERIFY(stiff_squash > 0.0);
  QVERIFY2(squishy_squash > 1.3 * stiff_squash,
           qPrintable(QString("%1 %2").arg(stiff_squash).arg(squishy_squash)));
}

void HopPreviewTests::DroopSagsInTheAir() {
  const DragNode upright = Jelly(0.5, 0.5, 0.5);
  DragNode droopy = upright;
  droopy.gravity = 1.0;
  HopPreview up(&upright);
  HopPreview down(&droopy);
  const int top = kHopFrames - kHopAirFrames / 2;
  for (int f = 0; f < kHopFrames + top; ++f) {
    up.Step();
    down.Step();
  }
  QVERIFY(HopAt(up.frame()).lean > 0.0);
  // Leaning, the droopy blob sags down the way the upright one does not.
  double sag = 0.0;
  for (size_t k = 0; k < up.dots().size(); ++k) {
    sag = std::max(sag, down.dots()[k].y() - up.dots()[k].y());
  }
  QVERIFY2(sag > kSeen, qPrintable(QString::number(sag)));
}

void HopPreviewTests::EveryHopIsTheSame() {
  // Even the softest, wobbliest blob does not swing higher each loop.
  const DragNode node = Jelly(0.0, 1.0, 1.0);
  HopPreview hop(&node);
  // The first loop starts at rest, not landing.
  for (int f = 0; f < kHopFrames; ++f) {
    hop.Step();
  }
  std::vector<std::vector<QPointF>> first;
  for (int f = 0; f < kHopFrames; ++f) {
    hop.Step();
    first.push_back(hop.dots());
  }
  QVERIFY(Most(AfterLanding(&node).bends) > kSeen);
  for (int f = 0; f < kHopFrames; ++f) {
    hop.Step();
    QCOMPARE(hop.dots(), first[static_cast<size_t>(f)]);
  }
}

void HopPreviewTests::NoSettingTearsTheBlob() {
  // The blob is about 40 by 32: no dot strays by its height.
  constexpr double kMostBend = 32.0;
  double most = 0.0;
  for (int corner = 0; corner < 32; ++corner) {
    DragNode node;
    node.kind = corner < 16 ? NodeKind::kJelly : NodeKind::kCloth;
    node.spring = (corner & 1) != 0 ? 1.0 : 0.0;
    node.inertia = (corner & 2) != 0 ? 1.0 : 0.0;
    node.stretch = (corner & 4) != 0 ? 1.0 : 0.0;
    node.gravity = (corner & 8) != 0 ? 1.0 : 0.0;
    node.air = node.gravity;
    HopPreview hop(&node);
    for (int f = 0; f < 3 * kHopFrames; ++f) {
      hop.Step();
      most = std::max(most, hop.Bend());
    }
  }
  QVERIFY(most > kSeen);
  QVERIFY2(most < kMostBend, qPrintable(QString::number(most)));
}

void HopPreviewTests::ClothAndDragReactToTheLanding() {
  DragNode cloth;
  cloth.kind = NodeKind::kCloth;
  DragNode drag;
  drag.kind = NodeKind::kDrag;
  drag.lag = 0.7;
  drag.bounce = 0.7;
  QVERIFY(Most(AfterLanding(&cloth).bends) > 1.0);
  QVERIFY(Most(AfterLanding(&drag).bends) > 1.0);
  QVERIFY(HopPreview(&drag).spring_dot() >= 0);
  QCOMPARE(HopPreview(&cloth).spring_dot(), -1);
  // Lag 0 follows the body exactly.
  drag.lag = 0.0;
  QCOMPARE(Most(AfterLanding(&drag).bends), 0.0);
}

void HopPreviewTests::NewSettingsCarryOn() {
  const DragNode soft = Jelly(0.2, 0.5, 0.5);
  const DragNode firm = Jelly(0.9, 0.5, 0.5);
  HopPreview hop(&soft);
  const int mid_air = kHopFrames - kHopAirFrames / 2;
  for (int f = 0; f < mid_air; ++f) {
    hop.Step();
  }
  QVERIFY(hop.Bend() > kSeen);
  // A new feel of the same kind goes on from where the blob is...
  hop.SetNode(&firm);
  QCOMPARE(hop.frame(), mid_air);
  QVERIFY(hop.Bend() > kSeen);
  // ...a new kind starts over at rest.
  DragNode cloth;
  cloth.kind = NodeKind::kCloth;
  hop.SetNode(&cloth);
  QCOMPARE(hop.frame(), 0);
  QCOMPARE(hop.Bend(), 0.0);
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::HopPreviewTests)
#include "hop_preview_tests.moc"
