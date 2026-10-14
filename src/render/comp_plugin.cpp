#include "render/comp_plugin.h"

#include <cassert>

#include "anim/comp_fsp.h"
#include "fsp/fsp_effects.h"
#include "render/comp_color_snap.h"
#include "render/comp_targets.h"

namespace snapper {
namespace {

// Clears every pixel of out (straight alpha) that the plugin left as it
// found it in before (same size and format). What it left is already in
// the frame; drawing it again would thicken every soft edge.
void ClearUnchanged(const QImage& before, QImage* out) {
  assert(out != nullptr && before.size() == out->size());
  assert(before.format() == QImage::Format_ARGB32 &&
         out->format() == QImage::Format_ARGB32);
  const int width = out->width();
  for (int y = 0; y < out->height(); ++y) {
    const QRgb* from = reinterpret_cast<const QRgb*>(before.constScanLine(y));
    QRgb* to = reinterpret_cast<QRgb*>(out->scanLine(y));
    for (int x = 0; x < width; ++x) {
      const bool is_unchanged = to[x] == from[x];
      if (is_unchanged) {
        to[x] = 0;
      }
    }
  }
}

}  // namespace

QImage RunCompPlugin(const CompEffect& effect, const QImage& source,
                     Frame local, double scale) {
  assert(effect.kind == CompKind::kFsp);
  assert(scale > 0.0 && local.index() >= 0);
  const FspEffectInfo* info = FspInfoOf(effect);
  const bool can_run = info != nullptr && !source.isNull();
  if (!can_run) {
    return QImage();
  }
  const FspValues values = FspValuesAt(effect, *info, local);
  // Plugins that match colors exactly get colors a few steps off (warped
  // in-between frames) snapped to their targets first.
  const QImage straight = SnapToTargetColors(
      *info, values, source.convertToFormat(QImage::Format_ARGB32),
      kCompColorReach);
  QImage out = RunFspEffect(*info, values, straight, scale);
  const bool is_usable = !out.isNull() && out.size() == source.size();
  if (!is_usable) {
    return QImage();
  }
  ClearUnchanged(straight, &out);
  const bool is_same_format = out.format() == source.format();
  return is_same_format ? out : out.convertToFormat(source.format());
}

}  // namespace snapper
