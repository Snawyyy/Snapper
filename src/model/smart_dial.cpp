#include "model/smart_dial.h"

#include <algorithm>
#include <cassert>

namespace snapper {

const SmartDial* FindDial(const std::vector<SmartDial>& dials,
                          const QString& name) {
  assert(dials.size() <= static_cast<size_t>(kMaxSmartDials));
  assert(name.size() < 100000);
  const auto found = std::ranges::find_if(
      dials, [&name](const SmartDial& d) { return d.name == name; });
  return found == dials.end() ? nullptr : &*found;
}

SmartDial* FindDial(std::vector<SmartDial>* dials, const QString& name) {
  assert(dials != nullptr);
  assert(name.size() < 100000);
  const SmartDial* dial =
      FindDial(static_cast<const std::vector<SmartDial>&>(*dials), name);
  return const_cast<SmartDial*>(dial);
}

double RestValue(const SmartDial& dial) {
  assert(dial.stops.size() <= static_cast<size_t>(kMaxDialStops));
  assert(dial.rest >= 0);
  const bool has_rest = dial.rest < static_cast<int>(dial.stops.size());
  return has_rest ? dial.stops[static_cast<size_t>(dial.rest)].at : 0.5;
}

}  // namespace snapper
