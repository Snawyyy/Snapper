#ifndef SNAPPER_UI_INSPECTOR_SHOW_H_
#define SNAPPER_UI_INSPECTOR_SHOW_H_

#include <set>

#include "edit/selection_manager.h"
#include "model/project.h"

namespace snapper {

// Which of the inspector's boxes matter for what is picked. Pure, so the
// rules can be read and tested without a widget.
struct InspectorShow final {
  // Nothing picked: the shot and its camera.
  bool shot = false;
  bool camera = false;
  bool pose = false;
  // Only for something that is linked.
  bool link = false;
  // The layer's own settings: for a whole layer, not a piece of it.
  bool layer = false;
  // Saved poses belong to the whole doll, not a limb.
  bool saved_poses = false;
  // Dot groups: on a doll that has dots worth grouping (drag nodes or
  // motions on a picked piece, picked dots, or groups already there).
  bool groups = false;
  // The picked dot's drag node and animation.
  bool drag = false;
  bool dot_animation = false;
};

// has_dot is true while a warp dot is picked, is_linked while the pick
// follows something or is followed, has_groups when a group of a picked
// piece exists.
InspectorShow ShowFor(const Project& project, ShotId shot,
                      const std::set<Pick>& picks, bool has_dot,
                      bool is_linked, bool has_groups);

}  // namespace snapper

#endif  // SNAPPER_UI_INSPECTOR_SHOW_H_
