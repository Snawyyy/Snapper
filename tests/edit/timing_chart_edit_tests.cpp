#include <QTest>

#include <cassert>

#include <vector>

#include "edit/history_manager.h"
#include "edit/key_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "edit/timing_chart_manager.h"

namespace snapper {
namespace {

const ShotId kShot(1);

LayerPose Turned(double degrees) {
  assert(kShot.value() == 1);
  assert(kShot.value() > 0);
  LayerPose pose;
  pose.rotation = degrees;
  return pose;
}

// The Arm and Head layers keyed on 0, 4 (arm only), 8 and 12; the arm
// turns 0, 33, 80, 90 degrees.
Project Stage() {
  assert(kShot.value() == 1);
  assert(kShot.value() > 0);
  Layer arm_layer;
  arm_layer.id = LayerId(1);
  arm_layer.name = "Arm";
  arm_layer.content = CharacterLayer{"Bob", {}, false};
  Layer head_layer = arm_layer;
  head_layer.id = LayerId(2);
  head_layer.name = "Head";
  const double arm[] = {0.0, 80.0, 90.0};
  const int frames[] = {0, 8, 12};
  for (int i = 0; i < 3; ++i) {
    SetKey(&arm_layer.transform, {Frame(frames[i]), Turned(arm[i]),
                                  Ease::kStep});
    SetKey(&head_layer.transform, {Frame(frames[i]), Turned(-arm[i]),
                                   Ease::kStep});
  }
  SetKey(&arm_layer.transform, {Frame(4), Turned(33.0), Ease::kStep});
  Shot shot;
  shot.id = kShot;
  shot.length = Frame(48);
  shot.layers = {arm_layer, head_layer};
  Project project;
  project.shots = {std::make_shared<const Shot>(shot)};
  return project;
}

const Channel<LayerPose>& Piece(const HistoryManager& history,
                                const QString& name) {
  assert(kShot.value() == 1);
  assert(kShot.value() > 0);
  const size_t at = name == QLatin1String("arm") ? 0 : 1;
  return history.current().shots[0]->layers[at].transform;
}

std::vector<int> FramesOf(const Channel<LayerPose>& channel) {
  assert(kShot.value() == 1);
  assert(kShot.value() > 0);
  std::vector<int> frames;
  for (const auto& key : channel.keys) {
    frames.push_back(key.frame.index());
  }
  return frames;
}

double ArmAt(const HistoryManager& history, int frame) {
  assert(kShot.value() == 1);
  assert(kShot.value() > 0);
  return Sample(Piece(history, "arm"), Frame(frame), LayerPose()).rotation;
}

// Everything the tests need, with both layers picked and the arm's keys
// on frames 0 and 8.
struct ChartRig final {
  ChartRig()
      : history(Stage()),
        selection(&history),
        keys(&history),
        playback(&history),
        chart(&history, &selection, &keys, &playback) {
    selection.SelectLayer(LayerId(1));
    selection.PickThings({Pick{LayerId(1)}, Pick{LayerId(2)}},
                         PickMode::kReplace, LayerId(1));
    const TrackRef arm{kShot, TrackKind::kLayer, LayerId(1)};
    selection.PickKeys({{arm, Frame(0)}, {arm, Frame(8)}},
                       PickMode::kReplace);
  }

  HistoryManager history;
  SelectionManager selection;
  KeyManager keys;
  PlaybackManager playback;
  TimingChartManager chart;
};

}  // namespace

class TimingChartEditTests final : public QObject {
  Q_OBJECT

 private slots:
  void RefusesWithAReason();
  void BakesEveryFrameWithoutMovingKeys();
  void TicksFollowTheDrag();
  void CancelPutsEverythingBack();
  void ApplyIsOneUndoStep();
  void EscapeDuringADragRestoresTheTicks();
  void ExistingKeysOnlyRespacesTheKeysThere();
  void ExistingKeysOnlyWithNoneBetweenCannotApply();
  void TwosAndThreesKeyTheGridAndKeepOutsideFrames();
  void ChangingTheModeRebuildsTheTicksWithThePreset();
  void EveryModeIsOneUndoStepAndKeepsOutsideFrames();
};

void TimingChartEditTests::RefusesWithAReason() {
  ChartRig rig;
  QVERIFY(rig.chart.WhyNoChart().isEmpty());
  rig.selection.PickKeys({{{kShot, TrackKind::kLayer, LayerId(1)},
                           Frame(0)}},
                         PickMode::kReplace);
  QVERIFY(!rig.chart.WhyNoChart().isEmpty());
  QVERIFY(!rig.chart.Open().has_value());
  QVERIFY(!rig.chart.IsOpen());
  rig.selection.Clear();
  QVERIFY(!rig.chart.WhyNoChart().isEmpty());
}

void TimingChartEditTests::BakesEveryFrameWithoutMovingKeys() {
  ChartRig rig;
  const auto head_before = FramesOf(Piece(rig.history, "head"));
  QVERIFY(rig.chart.Open().has_value());
  QCOMPARE(rig.chart.a(), Frame(0));
  QCOMPARE(rig.chart.b(), Frame(8));
  QCOMPARE(rig.chart.fractions().size(), size_t(7));
  // Every frame from A to B is keyed, the key on 12 stayed.
  QCOMPARE(FramesOf(Piece(rig.history, "arm")),
           std::vector<int>({0, 1, 2, 3, 4, 5, 6, 7, 8, 12}));
  QCOMPARE(ArmAt(rig.history, 4), 40.0);
  QCOMPARE(ArmAt(rig.history, 8), 80.0);
  QCOMPARE(ArmAt(rig.history, 12), 90.0);
  // The head follows too; its keys outside the range did not move.
  QCOMPARE(ArmAt(rig.history, 0), 0.0);
  QCOMPARE(Sample(Piece(rig.history, "head"), Frame(4), LayerPose()).rotation,
           -40.0);
  QCOMPARE(FramesOf(Piece(rig.history, "head")).back(), 12);
  QVERIFY(head_before.size() < FramesOf(Piece(rig.history, "head")).size());
  rig.chart.Cancel();
}

void TimingChartEditTests::TicksFollowTheDrag() {
  ChartRig rig;
  QVERIFY(rig.chart.Open().has_value());
  QVERIFY(rig.chart.SetSpacing(Spacing::kSlowIn).has_value());
  QVERIFY(ArmAt(rig.history, 1) < 10.0);
  QVERIFY(rig.chart.MoveTick(0, 0.5).has_value());
  const double first = ArmAt(rig.history, 1);
  QVERIFY(first < 80.0 * rig.chart.fractions()[1]);
  QVERIFY(IsValidSpacing(rig.chart.fractions()));
  QVERIFY(!rig.chart.MoveTick(99, 0.5).has_value());
  rig.chart.Cancel();
}

void TimingChartEditTests::CancelPutsEverythingBack() {
  ChartRig rig;
  const auto before = Piece(rig.history, "arm");
  QVERIFY(rig.chart.Open().has_value());
  QVERIFY(rig.chart.SetSpacing(Spacing::kHalvesA).has_value());
  rig.chart.Cancel();
  QVERIFY(!rig.chart.IsOpen());
  QVERIFY(Piece(rig.history, "arm") == before);
  QVERIFY(!rig.history.CanUndo());
}

void TimingChartEditTests::ApplyIsOneUndoStep() {
  ChartRig rig;
  const auto before = Piece(rig.history, "arm");
  QVERIFY(rig.chart.Open().has_value());
  QVERIFY(rig.chart.SetSpacing(Spacing::kSlowOut).has_value());
  QVERIFY(!rig.history.CanUndo());
  rig.chart.Apply();
  QVERIFY(!rig.chart.IsOpen());
  QCOMPARE(rig.history.UndoLabel(), QString("Timing chart on 2 things"));
  QVERIFY(ArmAt(rig.history, 1) > 10.0);
  rig.history.Undo();
  QVERIFY(Piece(rig.history, "arm") == before);
  QVERIFY(!rig.history.CanUndo());
}

void TimingChartEditTests::EscapeDuringADragRestoresTheTicks() {
  ChartRig rig;
  QVERIFY(rig.chart.Open().has_value());
  const auto start = rig.chart.fractions();
  rig.chart.BeginTickDrag();
  QVERIFY(rig.chart.MoveTick(2, 0.9).has_value());
  QVERIFY(rig.chart.fractions() != start);
  QVERIFY(rig.chart.CancelTickDrag().has_value());
  QCOMPARE(rig.chart.fractions(), start);
  QVERIFY(rig.chart.IsOpen());
  rig.chart.Cancel();
}

void TimingChartEditTests::ExistingKeysOnlyRespacesTheKeysThere() {
  ChartRig rig;
  QVERIFY(rig.chart.Open().has_value());
  QVERIFY(rig.chart.SetKeyMode(KeyMode::kExisting).has_value());
  QCOMPARE(rig.chart.frames(), std::vector<int>({4}));
  QCOMPARE(rig.chart.fractions().size(), size_t(1));
  QVERIFY(rig.chart.SetSpacing(Spacing::kSlowIn).has_value());
  QVERIFY(rig.chart.MoveTick(0, 0.25).has_value());
  // No key was added or moved; only the arm's key on 4 changed pose.
  QCOMPARE(FramesOf(Piece(rig.history, "arm")),
           std::vector<int>({0, 4, 8, 12}));
  QCOMPARE(FramesOf(Piece(rig.history, "head")), std::vector<int>({0, 8, 12}));
  QCOMPARE(ArmAt(rig.history, 4), 20.0);
  QCOMPARE(ArmAt(rig.history, 0), 0.0);
  QCOMPARE(ArmAt(rig.history, 8), 80.0);
  QVERIFY(rig.chart.WhyNoApply().isEmpty());
  rig.chart.Cancel();
}

void TimingChartEditTests::ExistingKeysOnlyWithNoneBetweenCannotApply() {
  ChartRig rig;
  const TrackRef arm{kShot, TrackKind::kLayer, LayerId(1)};
  rig.selection.PickKeys({{arm, Frame(8)}, {arm, Frame(12)}},
                         PickMode::kReplace);
  QCOMPARE(rig.chart.WhyNoChart(), QString());
  QVERIFY(rig.chart.Open().has_value());
  QVERIFY(rig.chart.SetKeyMode(KeyMode::kExisting).has_value());
  QVERIFY(rig.chart.frames().empty());
  QVERIFY(rig.chart.fractions().empty());
  QCOMPARE(rig.chart.WhyNoApply(),
           QString("No keys between A and B to respace"));
  rig.chart.Apply();
  QVERIFY(!rig.chart.IsOpen());
  QVERIFY(!rig.history.CanUndo());
  QCOMPARE(FramesOf(Piece(rig.history, "arm")),
           std::vector<int>({0, 4, 8, 12}));
}

void TimingChartEditTests::TwosAndThreesKeyTheGridAndKeepOutsideFrames() {
  ChartRig rig;
  QVERIFY(rig.chart.Open().has_value());
  QVERIFY(rig.chart.SetKeyMode(KeyMode::kTwos).has_value());
  QCOMPARE(rig.chart.frames(), std::vector<int>({2, 4, 6}));
  QCOMPARE(FramesOf(Piece(rig.history, "arm")),
           std::vector<int>({0, 2, 4, 6, 8, 12}));
  QVERIFY(rig.chart.SetKeyMode(KeyMode::kThrees).has_value());
  QCOMPARE(rig.chart.frames(), std::vector<int>({3, 6}));
  QCOMPARE(FramesOf(Piece(rig.history, "arm")),
           std::vector<int>({0, 3, 6, 8, 12}));
  QCOMPARE(FramesOf(Piece(rig.history, "head")),
           std::vector<int>({0, 3, 6, 8, 12}));
  QCOMPARE(ArmAt(rig.history, 12), 90.0);
  rig.chart.Cancel();
}

void TimingChartEditTests::ChangingTheModeRebuildsTheTicksWithThePreset() {
  ChartRig rig;
  QVERIFY(rig.chart.Open().has_value());
  QVERIFY(rig.chart.SetSpacing(Spacing::kSlowOut).has_value());
  QVERIFY(rig.chart.MoveTick(2, 0.9).has_value());
  QVERIFY(rig.chart.SetKeyMode(KeyMode::kTwos).has_value());
  QCOMPARE(rig.chart.spacing(), Spacing::kSlowOut);
  QCOMPARE(rig.chart.fractions(), SpacingFractions(Spacing::kSlowOut, 3));
  QCOMPARE(rig.chart.key_mode(), KeyMode::kTwos);
  QVERIFY(rig.chart.SetKeyMode(KeyMode::kOnes).has_value());
  QCOMPARE(rig.chart.frames().size(), size_t(7));
  rig.chart.Cancel();
}

void TimingChartEditTests::EveryModeIsOneUndoStepAndKeepsOutsideFrames() {
  const KeyMode modes[] = {KeyMode::kExisting, KeyMode::kOnes, KeyMode::kTwos,
                           KeyMode::kThrees};
  for (const KeyMode mode : modes) {
    ChartRig rig;
    const auto before = Piece(rig.history, "arm");
    QVERIFY(rig.chart.Open().has_value());
    QVERIFY(rig.chart.SetKeyMode(mode).has_value());
    QVERIFY(rig.chart.SetSpacing(Spacing::kSlowOut).has_value());
    QVERIFY(!rig.history.CanUndo());
    rig.chart.Apply();
    QCOMPARE(rig.history.UndoLabel(), QString("Timing chart on 2 things"));
    for (int f = 0; f <= 20; ++f) {
      const bool is_outside = f > 8;
      if (is_outside) {
        QCOMPARE(ArmAt(rig.history, f),
                 Sample(before, Frame(f), LayerPose()).rotation);
      }
    }
    rig.history.Undo();
    QVERIFY(Piece(rig.history, "arm") == before);
    QVERIFY(!rig.history.CanUndo());
  }
}

}  // namespace snapper

QTEST_MAIN(snapper::TimingChartEditTests)
#include "timing_chart_edit_tests.moc"
