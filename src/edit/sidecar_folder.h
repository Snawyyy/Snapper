#ifndef SNAPPER_EDIT_SIDECAR_FOLDER_H_
#define SNAPPER_EDIT_SIDECAR_FOLDER_H_

#include <QDir>
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

// A file name made from any text: letters, digits, spaces, dashes and
// underscores stay, everything else becomes an underscore; never empty,
// never starting with a dot.
QString SafeFileName(const QString& name);

// "<stem> NNN.<suffix>" in folder, with the first number (from 001) not
// used there yet.
QString FreshNumberedPath(const QDir& folder, const QString& stem,
                          const QString& suffix);

}  // namespace snapper

#endif  // SNAPPER_EDIT_SIDECAR_FOLDER_H_
