#ifndef SNAPPER_UI_INSPECTOR_TITLE_H_
#define SNAPPER_UI_INSPECTOR_TITLE_H_

#include <QString>

#include <optional>
#include <set>

#include "base/frame.h"
#include "edit/selection_manager.h"
#include "model/project.h"

namespace snapper {

// What the inspector's top line says is picked, in few words: "Head,
// frame 24" for a piece, the layer's name for a whole layer, "3 picked"
// for several, and the shot's name
// when nothing is picked. local is the playhead's frame in the shot.
QString InspectorTitle(const Project& project, ShotId shot,
                       const std::set<Pick>& picks,
                       const std::optional<Frame>& local);

}  // namespace snapper

#endif  // SNAPPER_UI_INSPECTOR_TITLE_H_
