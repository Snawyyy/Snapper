// ReelManager's cut markers: where cuts should fall, marked by ear.

#include <algorithm>
#include <cassert>
#include <utility>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/reel_manager.h"

namespace snapper {

Result<void> ReelManager::ToggleMarker(Frame at) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  Project next = history_->current();
  std::vector<Frame>& markers = next.reel.markers;
  const auto place = std::lower_bound(markers.begin(), markers.end(), at);
  const bool is_there = place != markers.end() && *place == at;
  if (is_there) {
    markers.erase(place);
    return history_->Apply(Tr("Remove cut marker"), std::move(next));
  }
  const bool is_full =
      markers.size() >= static_cast<size_t>(kMaxCutMarkers);
  if (is_full) {
    return std::unexpected(Error{
        Tr("The video holds at most %1 cut markers.").arg(kMaxCutMarkers)});
  }
  markers.insert(place, at);
  assert(std::is_sorted(markers.begin(), markers.end()));
  return history_->Apply(Tr("Add cut marker"), std::move(next));
}

}  // namespace snapper
