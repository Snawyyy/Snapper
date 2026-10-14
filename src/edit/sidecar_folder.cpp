#include "edit/sidecar_folder.h"

#include <QDir>
#include <QFileInfo>

#include <cassert>

namespace snapper {
namespace {

constexpr int kMaxNumbered = 9999;

}  // namespace

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

QString SafeFileName(const QString& name) {
  assert(kMaxNumbered > 0);
  assert(name.size() < 100000);
  QString safe;
  for (const QChar letter : name.trimmed()) {
    const bool is_plain = letter.isLetterOrNumber() || letter == ' ' ||
                          letter == '-' || letter == '_';
    safe.append(is_plain ? letter : QChar('_'));
  }
  const bool is_hidden = safe.isEmpty() || safe.startsWith('.');
  return is_hidden ? QStringLiteral("_") + safe : safe;
}

QString FreshNumberedPath(const QDir& folder, const QString& stem,
                          const QString& suffix) {
  assert(!folder.path().isEmpty());
  assert(!stem.isEmpty());
  for (int number = 1; number <= kMaxNumbered; ++number) {
    const QString name = QStringLiteral("%1 %2.%3")
                             .arg(stem)
                             .arg(number, 3, 10, QLatin1Char('0'))
                             .arg(suffix);
    const bool is_free = !folder.exists(name);
    if (is_free) {
      return folder.filePath(name);
    }
  }
  return folder.filePath(stem + "." + suffix);
}

}  // namespace snapper
