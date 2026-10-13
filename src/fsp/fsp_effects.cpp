#include "fsp/fsp_effects.h"

#include <cassert>

namespace snapper {

// A stub until the host stand-in lands: no plugins yet.
const std::vector<FspEffectInfo>& AllFspEffects() {
  static const std::vector<FspEffectInfo> none;
  assert(none.empty());
  return none;
}

const FspEffectInfo* FindFspEffect(const QString& id) {
  assert(!id.isEmpty());
  for (const FspEffectInfo& info : AllFspEffects()) {
    const bool is_it = info.id == id;
    if (is_it) {
      return &info;
    }
  }
  return nullptr;
}

FspValues DefaultFspValues(const FspEffectInfo& info) {
  assert(info.slot_count >= 0);
  FspValues values(static_cast<size_t>(info.slot_count), 0.0);
  for (const FspParam& param : info.params) {
    values[static_cast<size_t>(param.slot)] = param.neutral;
  }
  return values;
}

QImage RunFspEffect(const FspEffectInfo& info, const FspValues& values,
                    const QImage& input, double scale) {
  assert(input.width() >= 0);
  assert(scale > 0.0);
  assert(static_cast<int>(values.size()) == info.slot_count);
  return QImage();
}

}  // namespace snapper
