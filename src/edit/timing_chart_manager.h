#ifndef SNAPPER_EDIT_TIMING_CHART_MANAGER_H_
#define SNAPPER_EDIT_TIMING_CHART_MANAGER_H_

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

#include "anim/timing_chart.h"
#include "base/error.h"
#include "base/frame.h"
#include "edit/edit_scope.h"
#include "edit/track_ref.h"

namespace snapper {

class HistoryManager;
class KeyManager;
class PlaybackManager;
class SelectionManager;

// The timing chart session: what is picked and the frames picked on
// the timeline (first is A, last is B) become a chart whose ticks the
// user spaces. Every change previews live through one EditScope, so the
// stage shows the real result; Apply lands it as one undo step, Cancel
// puts everything back. Frames and keys never move in time.
class TimingChartManager final : public QObject {
  Q_OBJECT

 public:
  TimingChartManager(HistoryManager* history, SelectionManager* selection,
                     KeyManager* keys, PlaybackManager* playback);
  ~TimingChartManager() override;

  // Why a chart can't open now; empty when it can.
  QString WhyNoChart() const;
  Result<void> Open();
  bool IsOpen() const { return scope_ != nullptr; }

  Frame a() const { return a_; }
  Frame b() const { return b_; }
  // What the chart moves, for the window title and the undo label.
  const QString& name() const { return name_; }
  const std::vector<double>& fractions() const { return fractions_; }
  // The frame of each tick, rising; same count as fractions.
  const std::vector<int>& frames() const { return frames_; }
  KeyMode key_mode() const { return key_mode_; }
  // The preset the ticks were last set to (or dragged away from).
  Spacing spacing() const { return spacing_; }

  // Why Apply can't act now; empty when it can.
  QString WhyNoApply() const;

  // Which frames get keys. Rebuilds the ticks at once with the current
  // preset, so any custom drag is dropped. Kept for the next chart.
  Result<void> SetKeyMode(KeyMode mode);
  // Notes the preset the ticks are heading for, when a window glides to
  // it through SetFractions instead of calling SetSpacing.
  void ChoosePreset(Spacing spacing);

  // Replaces every tick with a preset or a custom spacing.
  Result<void> SetSpacing(Spacing spacing);
  Result<void> SetFractions(const std::vector<double>& fractions);
  // Drags tick index to wanted (0 to 1), held between its neighbours.
  Result<void> MoveTick(int index, double wanted);
  // A tick drag remembers the start; Cancel puts the ticks back.
  void BeginTickDrag();
  Result<void> CancelTickDrag();
  // Mouse let go: the drag stays.
  void EndTickDrag();
  // Shows the stage at tick index's frame: -1 is A, the tick count is B.
  void ShowTick(int index);

  // Lands the chart as one step and closes.
  void Apply();
  // Puts everything back and closes.
  void Cancel();

 signals:
  void Opened();
  void Changed();
  void Closed();

 private:
  void Close(bool is_kept);

  HistoryManager* history_;
  SelectionManager* selection_;
  KeyManager* keys_;
  PlaybackManager* playback_;
  std::unique_ptr<EditScope> scope_;
  std::vector<TrackRef> tracks_;
  ShotId shot_;
  Frame a_;
  Frame b_;
  QString name_;
  QString label_;
  std::vector<double> fractions_;
  std::vector<int> frames_;
  std::vector<int> existing_;
  KeyMode key_mode_ = KeyMode::kOnes;
  Spacing spacing_ = Spacing::kEven;
  std::vector<double> drag_start_;
  Frame playhead_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_TIMING_CHART_MANAGER_H_
