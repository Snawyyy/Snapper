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

PiecePose Turned(double degrees) {
  assert(kShot.value() == 1);
  assert(kShot.value() > 0);
  PiecePose pose;
  pose.rotation = degrees;
  return pose;
}

// Bob's head and arm keyed on 0, 4 (arm only), 8 and 12; the arm turns
// 0, 33, 80, 90 degrees.
Project Stage() {
  assert(kShot.value() == 1);
  assert(kShot.value() > 0);
  DollLayer posed{"Bob", {}, false};
  const double arm[] = {0.0, 80.0, 90.0};
  const int frames[] = {0, 8, 12};
  for (int i = 0; i < 3; ++i) {
    SetKey(&posed.pieces["arm"], {Frame(frames[i]), Turned(arm[i]),
                                  Ease::kStep});
    SetKey(&posed.pieces["head"], {Frame(frames[i]), Turned(-arm[i]),
                                   Ease::kStep});
  }
  SetKey(&posed.pieces["arm"], {Frame(4), Turned(33.0), Ease::kStep});
  Layer layer;
  layer.id = LayerId(1);
  layer.name = "Bob";
  layer.content = posed;
  Shot shot;
  shot.id = kShot;
  shot.length = Frame(48);
  shot.layers = {layer};
  Project project;
  project.shots = {std::make_shared<const Shot>(shot)};
  return project;
}

const Channel<PiecePose>& Piece(const HistoryManager& history,
                                const QString& name) {
  assert(kShot.value() == 1);
  assert(kShot.value() > 0);
  return std::get<DollLayer>(history.current().shots[0]->layers[0].content)
      .pieces.at(name);
}

std::vector<int> FramesOf(const Channel<PiecePose>& channel) {
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
  return Sample(Piece(history, "arm"), Frame(frame), PiecePose()).rotation;
}

// Everything the tests need, with Bob picked and keys on frames 0 and 8.
struct ChartRig final {
  ChartRig()
      : history(Stage()),
        selection(&history),
        keys(&history),
        playback(&history),
        chart(&history, &selection, &keys, &playback) {
    selection.SelectLayer(LayerId(1));
    selection.PickThings({{LayerId(1), QString()}}, PickMode::kReplace,
                         LayerId(1));
    const TrackRef arm{kShot, TrackKind::kPiece, LayerId(1), "arm"};
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
};

void TimingChartEditTests::RefusesWithAReason() {
  ChartRig rig;
  QVERIFY(rig.chart.WhyNoChart().isEmpty());
  rig.selection.PickKeys({{{kShot, TrackKind::kPiece, LayerId(1), "arm"},
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
  QCOMPARE(Sample(Piece(rig.history, "head"), Frame(4), PiecePose()).rotation,
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
  QCOMPARE(rig.history.UndoLabel(), QString("Timing chart on Bob"));
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

}  // namespace snapper

QTEST_MAIN(snapper::TimingChartEditTests)
#include "timing_chart_edit_tests.moc"
