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
  // The layer's own settings: for a whole layer, not a piece of it.
  bool layer = false;
  // Saved poses belong to the whole doll, not a limb (folded at first).
  bool saved_poses = false;
  // Smart dials: for a doll that has some, whole or by piece.
  bool dials = false;
  // A doll with no dials yet: only a "+" tile that makes the first one.
  bool add_dial = false;
  // Skins: for a doll picked whole that has skins to wear.
  bool skins = false;
  // References: only while one of the shot's references is picked.
  bool references = false;
  // The storyboard box: while a storyboard layer or some of its frames
  // are picked.
  bool storyboard = false;
  // Dot groups: on a doll that has dots worth grouping (drag nodes or
  // motions on a picked piece, picked dots, or groups already there).
  bool groups = false;
  // The picked dot's drag node and animation.
  bool drag = false;
  bool dot_animation = false;
  // An object (a prop from a picture) picked whole: how it plays and
  // what holds it.
  bool object = false;
};

// What the pick has beside the stage pick itself.
struct PickExtras final {
  // A warp dot is picked.
  bool has_dot = false;
  // The pick follows something or is followed.
  bool is_linked = false;
  // A group of a picked piece exists.
  bool has_groups = false;
  // One of the shot's references is picked.
  bool has_reference = false;
  // A storyboard layer, or frames of one, are picked.
  bool has_storyboard = false;
};

InspectorShow ShowFor(const Project& project, ShotId shot,
                      const std::set<Pick>& picks, const PickExtras& extras);

}  // namespace snapper

#endif  // SNAPPER_UI_INSPECTOR_SHOW_H_
