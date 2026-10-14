#include "render/comp_color_snap.h"

#include <QColor>
#include <QString>

#include <cassert>
#include <cmath>
#include <vector>

namespace snapper {
namespace {

constexpr int kMaxTargets = 8;
constexpr char kSelectiveBlurId[] = "SelectiveColorBlur";

// The slot of the plugin's parameter called name; -1 when it has none.
int SlotOf(const FspEffectInfo& info, const QString& name) {
  assert(!name.isEmpty());
  assert(info.slot_count >= 0);
  for (const FspParam& param : info.params) {
    const bool is_match = param.name == name;
    if (is_match) {
      return param.slot;
    }
  }
  return -1;
}

// The colors the plugin has turned on, as straight RGB.
std::vector<QRgb> EnabledTargets(const FspEffectInfo& info,
                                 const FspValues& values) {
  assert(static_cast<int>(values.size()) == info.slot_count);
  assert(kMaxTargets > 0);
  std::vector<QRgb> targets;
  for (int i = 0; i < kMaxTargets; ++i) {
    const int on_slot = SlotOf(info, QStringLiteral("TurnOn%1").arg(i));
    const int color_slot =
        SlotOf(info, QStringLiteral("targetColor%1").arg(i));
    const bool has_slots = on_slot >= 0 && color_slot >= 0;
    if (!has_slots) {
      continue;
    }
    const bool is_on = values[static_cast<size_t>(on_slot)] >= 0.5;
    if (is_on) {
      targets.push_back(0xFF000000u |
                        static_cast<QRgb>(std::lround(
                            values[static_cast<size_t>(color_slot)])));
    }
  }
  return targets;
}

int DistanceSquared(QRgb a, QRgb b) {
  assert(qAlpha(a) >= 0);
  assert(qAlpha(b) >= 0);
  const int dr = qRed(a) - qRed(b);
  const int dg = qGreen(a) - qGreen(b);
  const int db = qBlue(a) - qBlue(b);
  return dr * dr + dg * dg + db * db;
}

// The color pixel becomes: the nearest target within reach_squared, or
// pixel's own.
QRgb SnappedColor(QRgb pixel, const std::vector<QRgb>& targets,
                  int reach_squared) {
  assert(!targets.empty());
  assert(reach_squared >= 0);
  int best = reach_squared;
  QRgb snapped = pixel;
  for (const QRgb target : targets) {
    const int distance = DistanceSquared(pixel, target);
    const bool is_nearer = distance <= best;
    if (is_nearer) {
      best = distance;
      snapped = (pixel & 0xFF000000u) | (target & 0x00FFFFFFu);
    }
  }
  return snapped;
}

}  // namespace

bool MatchesColorsExactly(const FspEffectInfo& info) {
  assert(!info.id.isNull());
  assert(info.slot_count >= 0);
  return info.id == QLatin1String(kSelectiveBlurId);
}

QImage SnapToTargetColors(const FspEffectInfo& info, const FspValues& values,
                          const QImage& source, double reach) {
  assert(reach >= 0.0);
  assert(source.format() == QImage::Format_ARGB32);
  const bool can_snap = static_cast<int>(values.size()) == info.slot_count &&
                        !source.isNull() && MatchesColorsExactly(info);
  if (!can_snap) {
    return source;
  }
  const std::vector<QRgb> targets = EnabledTargets(info, values);
  const bool has_targets = !targets.empty();
  if (!has_targets) {
    return source;
  }
  const int reach_squared = static_cast<int>(reach * reach);
  QImage out = source;
  out.detach();
  const int width = out.width();
  for (int y = 0; y < out.height(); ++y) {
    QRgb* row = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = 0; x < width; ++x) {
      const bool is_seen = qAlpha(row[x]) > 0;
      if (is_seen) {
        row[x] = SnappedColor(row[x], targets, reach_squared);
      }
    }
  }
  return out;
}

}  // namespace snapper
