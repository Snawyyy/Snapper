#include "io/storyboard_paths.h"

#include <QDir>

#include <cassert>
#include <memory>
#include <variant>

namespace snapper {
namespace {

// file below folder as a path relative to it; empty when file is not
// below folder (or is not an absolute path).
QString BelowFolder(const QString& file, const QString& folder) {
  assert(!folder.isEmpty());
  assert(file.size() < 100000);
  const bool is_absolute = QDir::isAbsolutePath(file);
  if (!is_absolute) {
    return QString();
  }
  const QString relative = QDir(folder).relativeFilePath(file);
  const bool is_outside = relative == QLatin1String("..") ||
                          relative.startsWith(QLatin1String("../")) ||
                          QDir::isAbsolutePath(relative);
  return is_outside ? QString() : relative;
}

// project with every frame path replaced by map(path). A shot is copied
// only when one of its paths changes.
template <typename Map>
Project MapFramePaths(Project project, Map map) {
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  assert(project.shots.empty() || project.shots.front() != nullptr);
  for (auto& shared : project.shots) {
    Shot shot = *shared;
    bool is_changed = false;
    for (Layer& layer : shot.layers) {
      auto* board = std::get_if<StoryboardLayer>(&layer.content);
      const bool is_board = board != nullptr;
      if (!is_board) {
        continue;
      }
      for (StoryboardCel& cel : board->cels) {
        const QString mapped = map(cel.path);
        is_changed = is_changed || mapped != cel.path;
        cel.path = mapped;
      }
    }
    if (is_changed) {
      shared = std::make_shared<const Shot>(std::move(shot));
    }
  }
  return project;
}

}  // namespace

Project RelativeStoryboardPaths(Project project, const QString& base_folder) {
  assert(!base_folder.isEmpty());
  assert(QDir::isAbsolutePath(base_folder));
  return MapFramePaths(std::move(project),
                       [&base_folder](const QString& path) {
                         const QString below = BelowFolder(path, base_folder);
                         return below.isEmpty() ? path : below;
                       });
}

Project ResolvedStoryboardPaths(Project project, const QString& base_folder) {
  assert(!base_folder.isEmpty());
  assert(QDir::isAbsolutePath(base_folder));
  return MapFramePaths(std::move(project),
                       [&base_folder](const QString& path) {
                         const bool is_relative =
                             !path.isEmpty() && !QDir::isAbsolutePath(path);
                         return is_relative
                                    ? QDir(base_folder).absoluteFilePath(path)
                                    : path;
                       });
}

Project MovedStoryboardPaths(Project project, const QString& from_folder,
                             const QString& to_folder,
                             std::set<QString>* moved) {
  assert(!from_folder.isEmpty() && !to_folder.isEmpty());
  assert(from_folder != to_folder);
  return MapFramePaths(std::move(project), [&](const QString& path) {
    const QString below = BelowFolder(path, from_folder);
    const bool is_below = !below.isEmpty();
    if (!is_below) {
      return path;
    }
    const bool is_listing = moved != nullptr;
    if (is_listing) {
      moved->insert(path);
    }
    return QDir(to_folder).filePath(below);
  });
}

}  // namespace snapper
