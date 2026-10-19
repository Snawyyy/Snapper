#include "ui/inspector_title.h"

#include <cassert>

#include "base/text.h"

namespace snapper {
namespace {

// The words for one pick: its layer's name.
QString NameOf(const Shot& shot, const Pick& pick) {
  assert(pick.layer.value() >= 0);
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  const Layer* layer =
      pick.layer.IsValid() ? FindLayer(shot, pick.layer) : nullptr;
  const bool has_layer = layer != nullptr;
  return has_layer ? layer->name : Tr("Camera");
}

}  // namespace

QString InspectorTitle(const Project& project, ShotId id,
                       const std::set<Pick>& picks,
                       const std::optional<Frame>& local) {
  assert(picks.size() < 10000000);
  const Shot* shot = FindShot(project, id);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return QString();
  }
  const bool is_nothing = picks.empty();
  if (is_nothing) {
    return shot->name.isEmpty() ? Tr("Shot") : shot->name;
  }
  QString title = picks.size() > 1
                      ? Tr("%1 picked").arg(picks.size())
                      : NameOf(*shot, *picks.begin());
  const bool has_frame = local.has_value();
  if (has_frame) {
    title += Tr(", frame %1").arg(local->index());
  }
  return title;
}

}  // namespace snapper
