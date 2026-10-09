#include "anim/timing_chart.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace snapper {
namespace {

// Position of tick i of count along the line, evenly (0 to 1, exclusive).
double Along(int i, int count) {
  assert(count > 0);
  assert(i >= 0 && i < count);
  return (i + 1.0) / (count + 1.0);
}

double OneFraction(Spacing spacing, int i, int count) {
  assert(count > 0);
  assert(i >= 0 && i < count);
  switch (spacing) {
    case Spacing::kEven:
      return Along(i, count);
    case Spacing::kSlowIn:
      return EaseAmount(Ease::kEaseIn, Along(i, count));
    case Spacing::kSlowOut:
      return EaseAmount(Ease::kEaseOut, Along(i, count));
    case Spacing::kSlowInOut:
      return EaseAmount(Ease::kEaseInOut, Along(i, count));
    case Spacing::kHalvesA:
      return std::pow(0.5, count - i);
    case Spacing::kHalvesB:
      return 1.0 - std::pow(0.5, i + 1);
  }
  return Along(i, count);
}

}  // namespace

std::vector<double> SpacingFractions(Spacing spacing, int count) {
  assert(count >= 0 && count <= kMaxChartFrames);
  assert(static_cast<int>(spacing) < kSpacingCount);
  std::vector<double> fractions;
  for (int i = 0; i < count; ++i) {
    fractions.push_back(OneFraction(spacing, i, count));
  }
  // Presets can crowd ticks onto A, B or each other on long charts;
  // push them apart so the chart stays valid.
  for (int i = 0; i < count; ++i) {
    auto& tick = fractions[static_cast<size_t>(i)];
    const double low = kMinTickGap * (i + 1);
    const double high = 1.0 - kMinTickGap * (count - i);
    tick = std::clamp(tick, low, std::max(low, high));
  }
  for (int i = 1; i < count; ++i) {
    auto& tick = fractions[static_cast<size_t>(i)];
    tick = std::max(tick, fractions[static_cast<size_t>(i - 1)] + kMinTickGap);
  }
  return fractions;
}

std::vector<int> TickFrames(KeyMode mode, int a, int b,
                            const std::vector<int>& existing) {
  assert(a >= 0);
  assert(b - a >= 2 && b - a - 1 <= kMaxChartFrames);
  std::vector<int> frames;
  const bool is_existing = mode == KeyMode::kExisting;
  if (is_existing) {
    for (const int frame : existing) {
      const bool is_between = frame > a && frame < b;
      if (is_between) {
        frames.push_back(frame);
      }
    }
    std::ranges::sort(frames);
    frames.erase(std::ranges::unique(frames).begin(), frames.end());
    return frames;
  }
  const int step = mode == KeyMode::kOnes ? 1 : mode == KeyMode::kTwos ? 2 : 3;
  for (int frame = a + step; frame < b; frame += step) {
    frames.push_back(frame);
  }
  return frames;
}

const char* KeyModeKey(KeyMode mode) {
  assert(static_cast<int>(mode) < kKeyModeCount);
  assert(static_cast<int>(mode) >= 0);
  switch (mode) {
    case KeyMode::kExisting:
      return "existing";
    case KeyMode::kOnes:
      return "ones";
    case KeyMode::kTwos:
      return "twos";
    case KeyMode::kThrees:
      return "threes";
  }
  return "ones";
}

KeyMode KeyModeFromKey(std::string_view key, KeyMode fallback) {
  assert(static_cast<int>(fallback) < kKeyModeCount);
  assert(static_cast<int>(fallback) >= 0);
  for (int i = 0; i < kKeyModeCount; ++i) {
    const auto mode = static_cast<KeyMode>(i);
    const bool is_match = key == KeyModeKey(mode);
    if (is_match) {
      return mode;
    }
  }
  return fallback;
}

std::vector<double> MoveTick(std::vector<double> fractions, int index,
                             double wanted) {
  const int count = static_cast<int>(fractions.size());
  assert(index >= 0 && index < count);
  assert(std::isfinite(wanted));
  const double low = index == 0
                         ? kMinTickGap
                         : fractions[static_cast<size_t>(index - 1)] +
                               kMinTickGap;
  const double high = index == count - 1
                          ? 1.0 - kMinTickGap
                          : fractions[static_cast<size_t>(index + 1)] -
                                kMinTickGap;
  const bool has_room = low <= high;
  if (has_room) {
    fractions[static_cast<size_t>(index)] = std::clamp(wanted, low, high);
  }
  return fractions;
}

int NearestTick(const std::vector<double>& fractions, double fraction,
                double reach) {
  assert(std::isfinite(fraction));
  assert(reach >= 0.0);
  int best = -1;
  double best_gap = reach;
  for (size_t i = 0; i < fractions.size(); ++i) {
    const double gap = std::abs(fractions[i] - fraction);
    const bool is_closer = gap <= best_gap && (best < 0 || gap < best_gap);
    if (is_closer) {
      best = static_cast<int>(i);
      best_gap = gap;
    }
  }
  return best;
}

bool IsValidSpacing(const std::vector<double>& fractions) {
  assert(fractions.size() <= static_cast<size_t>(kMaxChartFrames));
  assert(kSpacingCount == 6);
  double last = 0.0;
  for (const double fraction : fractions) {
    const bool is_ok = fraction >= last + kMinTickGap - 1e-9 &&
                       fraction <= 1.0 - kMinTickGap + 1e-9;
    if (!is_ok) {
      return false;
    }
    last = fraction;
  }
  return true;
}

}  // namespace snapper
