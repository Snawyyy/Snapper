#ifndef SNAPPER_EDIT_STORYBOARD_FOLDER_H_
#define SNAPPER_EDIT_STORYBOARD_FOLDER_H_

#include <QString>

#include <set>

#include "base/error.h"

namespace snapper {

// Where a project's storyboard frame pictures live: "<project>
// storyboard" beside the saved project file, or "storyboard" in
// data_folder while the project (project_path empty) was never saved.
QString StoryboardFolder(const QString& project_path,
                         const QString& data_folder);

// Copies each picture of pictures (paths below from) to the same place
// below to, making folders and leaving any file already there.
Result<void> CopyStoryboardPictures(const std::set<QString>& pictures,
                                    const QString& from, const QString& to);

}  // namespace snapper

#endif  // SNAPPER_EDIT_STORYBOARD_FOLDER_H_
