#ifndef SNAPPER_UI_INSPECTOR_SHOW_H_
#define SNAPPER_UI_INSPECTOR_SHOW_H_

#include <set>

#include "edit/selection_manager.h"
#include "model/project.h"

namespace snapper {

// Which of the inspector's boxes matter for what is picked. Pure, so the
// rules can be read and tested without a widget. Only what the pick can
// use shows: a box with nothing to work on stays away.
struct InspectorShow final {
  // Nothing picked: the shot and its camera (the camera folded closed
  // at first, see Inspector).
  bool shot = false;
  bool camera = false;
  bool pose = false;
  // Only for something that is linked.
  bool link = false;
  // The layer's own settings.
  bool layer = false;
  // References: only while one of the shot's references is picked.
  bool references = false;
  // The storyboard box: while a storyboard layer or some of its frames
  // are picked.
  bool storyboard = false;
  // A character picked whole: its switches, smart bones, reset and flip.
  bool character = false;
};

// What the pick has beside the stage pick itself.
struct PickExtras final {
  // The pick follows something or is followed.
  bool is_linked = false;
  // One of the shot's references is picked.
  bool has_reference = false;
  // A storyboard layer, or frames of one, are picked.
  bool has_storyboard = false;
};

InspectorShow ShowFor(const Project& project, ShotId shot,
                      const std::set<Pick>& picks, const PickExtras& extras);

}  // namespace snapper

#endif  // SNAPPER_UI_INSPECTOR_SHOW_H_
