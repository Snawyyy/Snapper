#include <QTest>

#include <cassert>
#include <vector>

#include "anim/timing_chart.h"

namespace snapper {
namespace {

// Bakes a one-frame-each chart, as the old behaviour did.
bool BakeOnes(Channel<double>* channel, int a, int b,
              const std::vector<double>& fractions) {
  assert(channel != nullptr);
  assert(a < b);
  return BakeChannel(channel, Frame(a), Frame(b),
                     TickFrames(KeyMode::kOnes, a, b, {}), fractions,
                     KeyMode::kOnes);
}

std::vector<int> FramesOf(const Channel<double>& channel) {
  assert(channel.keys.size() < 1000);
  assert(kKeyModeCount == 4);
  std::vector<int> frames;
  for (const auto& key : channel.keys) {
    frames.push_back(key.frame.index());
  }
  return frames;
}

}  // namespace

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
  void BakeKeepsEveryFrameOutsideTheRange();
  void TickFramesPerMode();
  void KeyModeNamesRoundTrip();
  void BakeOnTwosAndThreesDropsOffGridKeys();
  void BakeExistingKeysOnlyAddsNoneAndMovesNone();
  void BakeKeepsFramesOutsideForEveryMode();
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
  QVERIFY(BakeOnes(&channel, 0, 4, {0.5, 0.75, 0.9}));
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
  QVERIFY(BakeOnes(&channel, 0, 4, {0.1, 0.2, 0.9}));
  QCOMPARE(Sample(channel, Frame(1), -1.0), 10.0);
  QCOMPARE(Sample(channel, Frame(2), -1.0), 20.0);
  QCOMPARE(Sample(channel, Frame(3), -1.0), 90.0);
}

void TimingChartTests::BakeLeavesAnEmptyChannelAlone() {
  Channel<double> channel;
  QVERIFY(BakeOnes(&channel, 0, 2, {0.5}));
  QVERIFY(channel.keys.empty());
}

void TimingChartTests::BakeKeepsEveryFrameOutsideTheRange() {
  const Ease eases[] = {Ease::kStep, Ease::kLinear, Ease::kEaseIn,
                        Ease::kEaseOut, Ease::kEaseInOut};
  for (const Ease ease : eases) {
    Channel<double> channel;
    SetKey(&channel, {Frame(0), 0.0, ease});
    SetKey(&channel, {Frame(3), 30.0, ease});
    SetKey(&channel, {Frame(16), 100.0, ease});
    SetKey(&channel, {Frame(20), 7.0, ease});
    // A on 6 and B on 12 have no key; neighbours are eased.
    const Channel<double> before = channel;
    QVERIFY(BakeOnes(&channel, 6, 12, {0.1, 0.2, 0.3, 0.4, 0.9}));
    for (int f = 0; f <= 30; ++f) {
      const bool is_outside = f < 6 || f > 12;
      if (is_outside) {
        QCOMPARE(Sample(channel, Frame(f), -1.0),
                 Sample(before, Frame(f), -1.0));
      }
    }
    QCOMPARE(Sample(channel, Frame(6), -1.0),
             Sample(before, Frame(6), -1.0));
    QCOMPARE(Sample(channel, Frame(12), -1.0),
             Sample(before, Frame(12), -1.0));
  }
}

void TimingChartTests::TickFramesPerMode() {
  const std::vector<int> existing = {0, 3, 3, 5, 9, 12, 40};
  QCOMPARE(TickFrames(KeyMode::kExisting, 0, 12, existing),
           std::vector<int>({3, 5, 9}));
  QVERIFY(TickFrames(KeyMode::kExisting, 0, 12, {0, 12}).empty());
  QCOMPARE(TickFrames(KeyMode::kOnes, 2, 6, existing),
           std::vector<int>({3, 4, 5}));
  QCOMPARE(TickFrames(KeyMode::kTwos, 2, 9, existing),
           std::vector<int>({4, 6, 8}));
  QCOMPARE(TickFrames(KeyMode::kTwos, 2, 8, existing),
           std::vector<int>({4, 6}));
  QCOMPARE(TickFrames(KeyMode::kThrees, 0, 10, existing),
           std::vector<int>({3, 6, 9}));
  QVERIFY(TickFrames(KeyMode::kThrees, 0, 3, existing).empty());
}

void TimingChartTests::KeyModeNamesRoundTrip() {
  for (int i = 0; i < kKeyModeCount; ++i) {
    const auto mode = static_cast<KeyMode>(i);
    QCOMPARE(KeyModeFromKey(KeyModeKey(mode), KeyMode::kOnes), mode);
  }
  QCOMPARE(KeyModeFromKey("junk", KeyMode::kTwos), KeyMode::kTwos);
  QCOMPARE(KeyModeFromKey("", KeyMode::kOnes), KeyMode::kOnes);
}

void TimingChartTests::BakeOnTwosAndThreesDropsOffGridKeys() {
  Channel<double> channel;
  SetKey(&channel, {Frame(0), 0.0, Ease::kStep});
  SetKey(&channel, {Frame(3), 50.0, Ease::kStep});
  SetKey(&channel, {Frame(8), 80.0, Ease::kStep});
  Channel<double> threes = channel;
  const auto twos_frames = TickFrames(KeyMode::kTwos, 0, 8, {});
  QVERIFY(BakeChannel(&channel, Frame(0), Frame(8), twos_frames,
                      {0.25, 0.5, 0.75}, KeyMode::kTwos));
  QCOMPARE(FramesOf(channel), std::vector<int>({0, 2, 4, 6, 8}));
  QCOMPARE(Sample(channel, Frame(4), -1.0), 40.0);
  const auto threes_frames = TickFrames(KeyMode::kThrees, 0, 8, {});
  QVERIFY(BakeChannel(&threes, Frame(0), Frame(8), threes_frames,
                      {0.25, 0.5}, KeyMode::kThrees));
  QCOMPARE(FramesOf(threes), std::vector<int>({0, 3, 6, 8}));
  QCOMPARE(Sample(threes, Frame(3), -1.0), 20.0);
}

void TimingChartTests::BakeExistingKeysOnlyAddsNoneAndMovesNone() {
  Channel<double> channel;
  SetKey(&channel, {Frame(0), 0.0, Ease::kStep});
  SetKey(&channel, {Frame(2), 10.0, Ease::kLinear});
  SetKey(&channel, {Frame(6), 20.0, Ease::kStep});
  SetKey(&channel, {Frame(8), 80.0, Ease::kEaseIn});
  // A second thing keyed on 4 only: its tick has no key here.
  const std::vector<int> frames = {2, 4, 6};
  QVERIFY(BakeChannel(&channel, Frame(0), Frame(8), frames,
                      {0.1, 0.2, 0.9}, KeyMode::kExisting));
  QCOMPARE(FramesOf(channel), std::vector<int>({0, 2, 6, 8}));
  QCOMPARE(channel.keys[1].value, 8.0);
  QCOMPARE(channel.keys[1].ease, Ease::kLinear);
  QCOMPARE(channel.keys[2].value, 72.0);
  QCOMPARE(channel.keys[0].value, 0.0);
  QCOMPARE(channel.keys[3].value, 80.0);
  QCOMPARE(channel.keys[3].ease, Ease::kEaseIn);
}

void TimingChartTests::BakeKeepsFramesOutsideForEveryMode() {
  const KeyMode modes[] = {KeyMode::kExisting, KeyMode::kOnes,
                           KeyMode::kTwos, KeyMode::kThrees};
  for (const KeyMode mode : modes) {
    Channel<double> channel;
    SetKey(&channel, {Frame(1), 5.0, Ease::kEaseIn});
    SetKey(&channel, {Frame(8), 30.0, Ease::kEaseOut});
    SetKey(&channel, {Frame(10), 90.0, Ease::kEaseInOut});
    SetKey(&channel, {Frame(20), 7.0, Ease::kLinear});
    SetKey(&channel, {Frame(30), 1.0, Ease::kStep});
    const Channel<double> before = channel;
    // A on 4 and B on 14 carry no keys; 8 and 10 sit between them.
    const auto frames = TickFrames(mode, 4, 14, {8, 10});
    const std::vector<double> fractions = SpacingFractions(
        Spacing::kSlowIn, static_cast<int>(frames.size()));
    QVERIFY(BakeChannel(&channel, Frame(4), Frame(14), frames, fractions,
                        mode));
    for (int f = 0; f <= 40; ++f) {
      const bool is_outside = f < 4 || f > 14;
      if (is_outside) {
        QCOMPARE(Sample(channel, Frame(f), -1.0),
                 Sample(before, Frame(f), -1.0));
      }
    }
  }
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::TimingChartTests)
#include "timing_chart_tests.moc"
