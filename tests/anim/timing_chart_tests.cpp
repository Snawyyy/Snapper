#include <QTest>

#include <vector>

#include "anim/timing_chart.h"

namespace snapper {

class TimingChartTests final : public QObject {
  Q_OBJECT

 private slots:
  void EvenTicksShareTheLine();
  void PresetsBunchTheRightEnd();
  void EveryPresetIsValidAtEveryLength();
  void DraggedTickStaysBetweenNeighbours();
  void NearestTickWithinReach();
  void BakeKeysEveryFrameAndKeepsOutsideKeys();
  void BakeFollowsTheSpacing();
  void BakeLeavesAnEmptyChannelAlone();
};

void TimingChartTests::EvenTicksShareTheLine() {
  const auto fractions = SpacingFractions(Spacing::kEven, 3);
  QCOMPARE(fractions.size(), size_t(3));
  QCOMPARE(fractions[0], 0.25);
  QCOMPARE(fractions[1], 0.5);
  QCOMPARE(fractions[2], 0.75);
  QVERIFY(SpacingFractions(Spacing::kEven, 0).empty());
}

void TimingChartTests::PresetsBunchTheRightEnd() {
  const auto even = SpacingFractions(Spacing::kEven, 3);
  const auto in = SpacingFractions(Spacing::kSlowIn, 3);
  const auto out = SpacingFractions(Spacing::kSlowOut, 3);
  QVERIFY(in[0] < even[0]);
  QVERIFY(out[2] > even[2]);
  const auto both = SpacingFractions(Spacing::kSlowInOut, 3);
  QVERIFY(both[0] < even[0]);
  QVERIFY(both[2] > even[2]);
  const auto halves_a = SpacingFractions(Spacing::kHalvesA, 3);
  QCOMPARE(halves_a, std::vector<double>({0.125, 0.25, 0.5}));
  const auto halves_b = SpacingFractions(Spacing::kHalvesB, 3);
  QCOMPARE(halves_b, std::vector<double>({0.5, 0.75, 0.875}));
}

void TimingChartTests::EveryPresetIsValidAtEveryLength() {
  for (int kind = 0; kind < kSpacingCount; ++kind) {
    for (int count = 1; count <= kMaxChartFrames; ++count) {
      const auto fractions =
          SpacingFractions(static_cast<Spacing>(kind), count);
      QCOMPARE(static_cast<int>(fractions.size()), count);
      QVERIFY(IsValidSpacing(fractions));
    }
  }
}

void TimingChartTests::DraggedTickStaysBetweenNeighbours() {
  const std::vector<double> start = {0.25, 0.5, 0.75};
  const auto past = MoveTick(start, 1, 0.9);
  QVERIFY(past[1] < 0.75);
  QVERIFY(IsValidSpacing(past));
  const auto before_a = MoveTick(start, 0, -3.0);
  QVERIFY(before_a[0] > 0.0);
  const auto after_b = MoveTick(start, 2, 3.0);
  QVERIFY(after_b[2] < 1.0);
  QCOMPARE(MoveTick(start, 1, 0.6)[1], 0.6);
  QCOMPARE(MoveTick(start, 1, 0.6)[0], 0.25);
}

void TimingChartTests::NearestTickWithinReach() {
  const std::vector<double> fractions = {0.25, 0.5, 0.75};
  QCOMPARE(NearestTick(fractions, 0.52, 0.05), 1);
  QCOMPARE(NearestTick(fractions, 0.6, 0.05), -1);
  QCOMPARE(NearestTick({}, 0.5, 1.0), -1);
}

void TimingChartTests::BakeKeysEveryFrameAndKeepsOutsideKeys() {
  Channel<double> channel;
  SetKey(&channel, {Frame(0), 0.0, Ease::kStep});
  SetKey(&channel, {Frame(2), 99.0, Ease::kStep});
  SetKey(&channel, {Frame(4), 100.0, Ease::kEaseIn});
  SetKey(&channel, {Frame(9), 7.0, Ease::kStep});
  QVERIFY(BakeChannel(&channel, Frame(0), Frame(4),
                      std::vector<double>({0.5, 0.75, 0.9})));
  std::vector<int> frames;
  for (const auto& key : channel.keys) {
    frames.push_back(key.frame.index());
  }
  QCOMPARE(frames, std::vector<int>({0, 1, 2, 3, 4, 9}));
  QCOMPARE(channel.keys[0].value, 0.0);
  QCOMPARE(channel.keys[4].value, 100.0);
  QCOMPARE(channel.keys[4].ease, Ease::kEaseIn);
  QCOMPARE(channel.keys[5].value, 7.0);
}

void TimingChartTests::BakeFollowsTheSpacing() {
  Channel<double> channel;
  SetKey(&channel, {Frame(0), 0.0, Ease::kStep});
  SetKey(&channel, {Frame(4), 100.0, Ease::kStep});
  QVERIFY(BakeChannel(&channel, Frame(0), Frame(4),
                      std::vector<double>({0.1, 0.2, 0.9})));
  QCOMPARE(Sample(channel, Frame(1), -1.0), 10.0);
  QCOMPARE(Sample(channel, Frame(2), -1.0), 20.0);
  QCOMPARE(Sample(channel, Frame(3), -1.0), 90.0);
}

void TimingChartTests::BakeLeavesAnEmptyChannelAlone() {
  Channel<double> channel;
  QVERIFY(BakeChannel(&channel, Frame(0), Frame(2),
                      std::vector<double>({0.5})));
  QVERIFY(channel.keys.empty());
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::TimingChartTests)
#include "timing_chart_tests.moc"
