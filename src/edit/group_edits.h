#ifndef SNAPPER_EDIT_GROUP_EDITS_H_
#define SNAPPER_EDIT_GROUP_EDITS_H_

#include <QString>

#include <optional>
#include <set>
#include <vector>

#include "base/error.h"
#include "edit/selection_manager.h"
#include "model/project.h"

namespace snapper {

// A dot group as a shot shows it: its dots are on layers of that shot.
struct ShownGroup final {
  QString name;
  std::set<WarpDot> dots;
  // From the dolls (every shot shows it) and not from the shot alone.
  bool is_default = false;
};

// The groups shot shows: its own, then the defaults of the dolls on its
// layers (a shot's group of the same name hides a default). Dots that
// are no longer on the stage are left out.
std::vector<ShownGroup> ShownGroupsOf(const Project& project, ShotId shot);

// The project with the group called old_name (if any) gone from the shot
// and from the defaults of the dolls on its layers, and group written in
// its place: as a default of each doll when group->is_default, else in
// the shot. No group just removes it. Refuses a name already used by
// another group, or too many groups or dots.
Result<Project> WithGroupWritten(const Project& project, ShotId shot,
                                 const QString& old_name,
                                 const std::optional<ShownGroup>& group);

}  // namespace snapper

#endif  // SNAPPER_EDIT_GROUP_EDITS_H_
