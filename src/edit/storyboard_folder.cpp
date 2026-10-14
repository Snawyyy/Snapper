#include "edit/storyboard_folder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cassert>

#include "base/text.h"
#include "edit/sidecar_folder.h"

namespace snapper {

QString StoryboardFolder(const QString& project_path,
                         const QString& data_folder) {
  assert(!data_folder.isEmpty());
  assert(project_path.size() < 100000);
  return SidecarFolder(project_path, data_folder, Tr("%1 storyboard"),
                       QStringLiteral("storyboard"));
}

Result<void> CopyStoryboardPictures(const std::set<QString>& pictures,
                                    const QString& from, const QString& to) {
  assert(!from.isEmpty() && !to.isEmpty());
  assert(from != to);
  for (const QString& picture : pictures) {
    const QString target = QDir(to).filePath(
        QDir(from).relativeFilePath(picture));
    const bool is_there = QFile::exists(target);
    if (is_there) {
      continue;
    }
    const bool is_made = QDir().mkpath(QFileInfo(target).absolutePath());
    const bool is_copied = is_made && QFile::copy(picture, target);
    if (!is_copied) {
      return std::unexpected(
          Error{Tr("Can't copy %1 to %2.").arg(picture, target)});
    }
  }
  return {};
}

}  // namespace snapper
