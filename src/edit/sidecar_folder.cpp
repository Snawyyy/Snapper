#include "edit/sidecar_folder.h"

#include <QDir>
#include <QFileInfo>

#include <cassert>

namespace snapper {

QString SidecarFolder(const QString& project_path, const QString& data_folder,
                      const QString& saved_format,
                      const QString& unsaved_name) {
  assert(!unsaved_name.isEmpty());
  assert(saved_format.contains(QStringLiteral("%1")));
  const bool is_saved = !project_path.isEmpty();
  if (!is_saved) {
    return QDir(data_folder).filePath(unsaved_name);
  }
  const QFileInfo project(project_path);
  return project.dir().filePath(saved_format.arg(project.completeBaseName()));
}

}  // namespace snapper
