#include "render/comp_plugin.h"

#include <cassert>

#include "anim/comp_fsp.h"
#include "fsp/fsp_effects.h"

namespace snapper {

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
  const QImage out = RunFspEffect(*info, values, source, scale);
  const bool is_usable = !out.isNull() && out.size() == source.size();
  if (!is_usable) {
    return QImage();
  }
  const bool is_same_format = out.format() == source.format();
  return is_same_format ? out : out.convertToFormat(source.format());
}

}  // namespace snapper
