#include "edit/timing_chart_manager.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>
#include <utility>

#include "anim/master_timeline.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/key_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "edit/timeline_rows.h"

namespace snapper {
namespace {

// What a chart would move and between which frames.
struct Plan final {
  ShotId shot;
  std::vector<TrackRef> tracks;
  Frame a;
  Frame b;
  QString name;
};

// The channels of everything picked: a layer brings every channel of its
// timeline row.
std::vector<TrackRef> TracksOf(const Project& project, ShotId shot,
                               const std::set<Pick>& picks) {
  assert(picks.size() < 100000);
  assert(shot.value() >= 0);
  std::set<TrackRef> tracks;
  const auto rows = TimelineRows(project, shot);
  for (const Pick& pick : picks) {
    for (const TimelineRow& row : rows) {
      const bool is_row = row.layer == pick.layer;
      if (is_row) {
        tracks.insert(row.tracks.begin(), row.tracks.end());
      }
    }
  }
  return std::vector<TrackRef>(tracks.begin(), tracks.end());
}

bool HasKeys(const Project& project, const TrackRef& track) {
  assert(track.shot.value() >= 0);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  const Shot* shot = FindShot(project, track.shot);
  bool is_keyed = false;
  const bool has_shot = shot != nullptr;
  if (has_shot) {
    ReadTrack(*shot, track,
              [&](const auto& channel) { is_keyed = !channel.keys.empty(); });
  }
  return is_keyed;
}

// The frames any of the tracks has a key on.
std::vector<int> KeyFramesOf(const Project& project,
                             const std::vector<TrackRef>& tracks) {
  assert(tracks.size() < 100000);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  std::set<int> frames;
  for (const TrackRef& track : tracks) {
    const Shot* shot = FindShot(project, track.shot);
    const bool has_shot = shot != nullptr;
    if (!has_shot) {
      continue;
    }
    ReadTrack(*shot, track, [&](const auto& channel) {
      for (const auto& key : channel.keys) {
        frames.insert(key.frame.index());
      }
    });
  }
  return std::vector<int>(frames.begin(), frames.end());
}

QString NameOf(const Project& project, ShotId shot,
               const std::set<Pick>& picks) {
  assert(!picks.empty());
  assert(shot.value() >= 0);
  const Shot* found = FindShot(project, shot);
  const Layer* layer =
      found != nullptr ? FindLayer(*found, picks.begin()->layer) : nullptr;
  const bool is_one = picks.size() == 1 && layer != nullptr;
  if (!is_one) {
    return Tr("%1 things").arg(picks.size());
  }
  return layer->name;
}

// Frames of the picked keys in the shot: first is A, last is B.
Result<std::pair<Frame, Frame>> RangeOf(const std::set<KeyRef>& keys,
                                        ShotId shot) {
  assert(shot.value() >= 0);
  assert(keys.size() < 10000000);
  std::set<Frame> frames;
  for (const KeyRef& key : keys) {
    const bool is_here = key.track.shot == shot;
    if (is_here) {
      frames.insert(key.frame);
    }
  }
  const bool is_short =
      frames.empty() || frames.rbegin()->index() - frames.begin()->index() < 2;
  if (is_short) {
    return std::unexpected(Error{
        Tr("Pick keys on at least 3 frames in a row first: the first is A, "
           "the last is B.")});
  }
  return std::make_pair(*frames.begin(), *frames.rbegin());
}

Result<Plan> MakePlan(const Project& project,
                      const SelectionManager& selection) {
  assert(kMaxChartFrames > 0);
  const ShotId shot = selection.shot();
  assert(shot.value() >= 0);
  const bool has_picks = !selection.picks().empty();
  if (!has_picks) {
    return std::unexpected(Error{Tr("Pick a layer first.")});
  }
  const auto range = RangeOf(selection.keys(), shot);
  if (!range) {
    return std::unexpected(range.error());
  }
  const bool is_long =
      range->second.index() - range->first.index() - 1 > kMaxChartFrames;
  if (is_long) {
    return std::unexpected(Error{Tr("A timing chart covers at most %1 "
                                    "frames in between.")
                                     .arg(kMaxChartFrames)});
  }
  Plan plan{shot, TracksOf(project, shot, selection.picks()), range->first,
            range->second, NameOf(project, shot, selection.picks())};
  const bool is_keyed = std::ranges::any_of(
      plan.tracks, [&](const TrackRef& t) { return HasKeys(project, t); });
  if (!is_keyed) {
    return std::unexpected(
        Error{Tr("What is picked has no keys to follow.")});
  }
  return plan;
}

}  // namespace

TimingChartManager::TimingChartManager(HistoryManager* history,
                                       SelectionManager* selection,
                                       KeyManager* keys,
                                       PlaybackManager* playback)
    : history_(history),
      selection_(selection),
      keys_(keys),
      playback_(playback) {
  assert(history_ != nullptr && selection_ != nullptr);
  assert(keys_ != nullptr && playback_ != nullptr);
}

TimingChartManager::~TimingChartManager() = default;

QString TimingChartManager::WhyNoChart() const {
  assert(history_ != nullptr);
  assert(selection_ != nullptr);
  const bool is_busy = IsOpen() || history_->IsScopeOpen();
  if (is_busy) {
    return IsOpen() ? Tr("The timing chart is already open.")
                    : Tr("Finish the drag first.");
  }
  const auto plan = MakePlan(history_->current(), *selection_);
  return plan ? QString() : plan.error().message;
}

Result<void> TimingChartManager::Open() {
  assert(history_ != nullptr);
  assert(!IsOpen());
  const QString why_not = WhyNoChart();
  const bool is_refused = !why_not.isEmpty();
  if (is_refused) {
    return std::unexpected(Error{why_not});
  }
  const auto plan = MakePlan(history_->current(), *selection_);
  assert(plan.has_value());
  shot_ = plan->shot;
  tracks_ = plan->tracks;
  a_ = plan->a;
  b_ = plan->b;
  name_ = plan->name;
  label_ = Tr("Timing chart on %1").arg(name_);
  playhead_ = playback_->frame();
  playback_->Pause();
  scope_ = std::make_unique<EditScope>(history_, label_);
  existing_ = KeyFramesOf(history_->current(), tracks_);
  spacing_ = Spacing::kEven;
  frames_ = TickFrames(key_mode_, a_.index(), b_.index(), existing_);
  const auto set = SetFractions(SpacingFractions(
      spacing_, static_cast<int>(frames_.size())));
  const bool has_failed = !set.has_value();
  if (has_failed) {
    Close(false);
    return set;
  }
  emit Opened();
  return {};
}

Result<void> TimingChartManager::SetSpacing(Spacing spacing) {
  assert(IsOpen());
  assert(static_cast<int>(spacing) < kSpacingCount);
  spacing_ = spacing;
  return SetFractions(
      SpacingFractions(spacing, static_cast<int>(frames_.size())));
}

void TimingChartManager::ChoosePreset(Spacing spacing) {
  assert(IsOpen());
  assert(static_cast<int>(spacing) < kSpacingCount);
  spacing_ = spacing;
}

Result<void> TimingChartManager::SetKeyMode(KeyMode mode) {
  assert(IsOpen());
  assert(static_cast<int>(mode) < kKeyModeCount);
  const KeyMode old_mode = key_mode_;
  const std::vector<int> old_frames = frames_;
  frames_ = TickFrames(mode, a_.index(), b_.index(), existing_);
  key_mode_ = mode;
  const auto set = SetSpacing(spacing_);
  const bool has_failed = !set.has_value();
  if (has_failed) {
    key_mode_ = old_mode;
    frames_ = old_frames;
  }
  return set;
}

QString TimingChartManager::WhyNoApply() const {
  assert(IsOpen());
  assert(frames_.size() == fractions_.size());
  const bool is_empty =
      key_mode_ == KeyMode::kExisting && frames_.empty();
  return is_empty ? Tr("No keys between A and B to respace") : QString();
}

Result<void> TimingChartManager::SetFractions(
    const std::vector<double>& fractions) {
  assert(IsOpen());
  assert(fractions.size() <= static_cast<size_t>(kMaxChartFrames));
  const bool is_valid =
      IsValidSpacing(fractions) && fractions.size() == frames_.size();
  if (!is_valid) {
    return std::unexpected(Error{Tr("Those ticks don't fit the chart.")});
  }
  const auto baked = keys_->BakeTiming(tracks_, a_, b_, key_mode_,
                                      frames_, fractions, label_);
  const bool has_failed = !baked.has_value();
  if (has_failed) {
    return baked;
  }
  fractions_ = fractions;
  emit Changed();
  return {};
}

Result<void> TimingChartManager::MoveTick(int index, double wanted) {
  assert(IsOpen());
  assert(fractions_.size() <= static_cast<size_t>(kMaxChartFrames));
  const bool is_tick = index >= 0 &&
                       index < static_cast<int>(fractions_.size());
  const bool is_usable = is_tick && std::isfinite(wanted);
  if (!is_usable) {
    return std::unexpected(Error{Tr("There is no such tick.")});
  }
  return SetFractions(snapper::MoveTick(fractions_, index, wanted));
}

void TimingChartManager::BeginTickDrag() {
  assert(IsOpen());
  assert(fractions_.size() <= static_cast<size_t>(kMaxChartFrames));
  drag_start_ = fractions_;
}

Result<void> TimingChartManager::CancelTickDrag() {
  assert(IsOpen());
  assert(drag_start_.size() <= static_cast<size_t>(kMaxChartFrames));
  const bool was_dragging = !drag_start_.empty();
  if (!was_dragging) {
    return {};
  }
  const auto start = std::exchange(drag_start_, {});
  return SetFractions(start);
}

void TimingChartManager::EndTickDrag() {
  assert(IsOpen());
  assert(drag_start_.size() <= static_cast<size_t>(kMaxChartFrames));
  drag_start_.clear();
}

void TimingChartManager::ShowTick(int index) {
  assert(IsOpen());
  const int count = static_cast<int>(frames_.size());
  assert(count >= 0);
  const int slot = std::clamp(index, -1, count);
  const int at = slot < 0 ? a_.index()
                          : slot >= count ? b_.index()
                                          : frames_[static_cast<size_t>(slot)];
  const int shot_index = ShotIndex(history_->current(), shot_);
  const bool has_shot = shot_index >= 0;
  if (has_shot) {
    playback_->Seek(
        Frame(ShotStart(history_->current(), shot_index).index() + at));
  }
}

void TimingChartManager::Apply() {
  assert(IsOpen());
  assert(scope_ != nullptr);
  const bool is_idle = !WhyNoApply().isEmpty();
  Close(!is_idle);
}

void TimingChartManager::Cancel() {
  assert(IsOpen());
  assert(scope_ != nullptr);
  Close(false);
}

void TimingChartManager::Close(bool is_kept) {
  assert(scope_ != nullptr);
  assert(playback_ != nullptr);
  if (!is_kept) {
    scope_->Cancel();
  }
  scope_.reset();
  drag_start_.clear();
  playback_->Seek(playhead_);
  emit Closed();
}

}  // namespace snapper
