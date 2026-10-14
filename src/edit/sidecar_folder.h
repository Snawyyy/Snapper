#ifndef SNAPPER_EDIT_SIDECAR_FOLDER_H_
#define SNAPPER_EDIT_SIDECAR_FOLDER_H_

#include <QString>

namespace snapper {

// Where files made for a project but kept outside it go (recorded
// takes, spoken lines): beside the saved project, in a folder named
// saved_format with the project's name in place of %1 ("%1 takes"), or
// for a project never saved (project_path empty) in unsaved_name under
// the app's data folder.
QString SidecarFolder(const QString& project_path, const QString& data_folder,
                      const QString& saved_format,
                      const QString& unsaved_name);

}  // namespace snapper

#endif  // SNAPPER_EDIT_SIDECAR_FOLDER_H_
