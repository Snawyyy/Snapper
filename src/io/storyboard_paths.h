#ifndef SNAPPER_IO_STORYBOARD_PATHS_H_
#define SNAPPER_IO_STORYBOARD_PATHS_H_

#include <QString>

#include <set>

#include "model/project.h"

namespace snapper {

// Where storyboard frame pictures are named from. In memory a frame's
// path is always absolute; in the project file it is relative to the
// file's folder when the picture lies below it, so the project and its
// "<project> storyboard" folder can move together. A picture elsewhere
// (the app data folder while a project is unsaved) stays absolute.

// project with each frame path below base_folder made relative to it.
Project RelativeStoryboardPaths(Project project, const QString& base_folder);
// project with each relative frame path made absolute under base_folder.
Project ResolvedStoryboardPaths(Project project, const QString& base_folder);
// project with each frame path below from_folder moved to the same place
// below to_folder (the pictures are copied there). When moved is given,
// the old path of every picture that moved is added to it.
Project MovedStoryboardPaths(Project project, const QString& from_folder,
                             const QString& to_folder,
                             std::set<QString>* moved = nullptr);

}  // namespace snapper

#endif  // SNAPPER_IO_STORYBOARD_PATHS_H_
