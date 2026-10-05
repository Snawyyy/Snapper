#include "edit/document_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

#include <cassert>

#include "base/text.h"
#include "edit/history_manager.h"
#include "io/project_file.h"

namespace snapper {

DocumentManager::DocumentManager(HistoryManager* history,
                                 QString data_folder)
    : history_(history), data_folder_(std::move(data_folder)) {
  assert(history_ != nullptr);
  assert(!data_folder_.isEmpty());
  QDir().mkpath(data_folder_);
  autosave_timer_.setInterval(kAutosaveMs);
  connect(&autosave_timer_, &QTimer::timeout, this, [this] {
    const auto saved = Autosave();
    if (!saved) {
      emit AutosaveFailed(saved.error().message);
    }
  });
  autosave_timer_.start();
}

Result<void> DocumentManager::New(const QString& name, CanvasSize canvas) {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
  const bool is_valid = !name.trimmed().isEmpty() && IsValidCanvas(canvas);
  if (!is_valid) {
    return std::unexpected(Error{
        Tr("A project needs a name and an even size from %1 to %2 pixels.")
            .arg(kMinCanvasSide)
            .arg(kMaxCanvasSide)});
  }
  Project project;
  project.name = name.trimmed();
  project.canvas = canvas;
  history_->Reset(std::move(project));
  path_.clear();
  emit PathChanged();
  return {};
}

Result<void> DocumentManager::Open(const QString& path) {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
  auto project = ReadProject(path);
  if (!project) {
    return std::unexpected(project.error());
  }
  history_->Reset(std::move(*project));
  path_ = QFileInfo(path).absoluteFilePath();
  Remember(path_);
  emit PathChanged();
  return {};
}

Result<void> DocumentManager::Save() {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
  const bool is_unplaced = !HasPath();
  if (is_unplaced) {
    return std::unexpected(Error{Tr("Choose where to save it first.")});
  }
  return SaveAs(path_);
}

Result<void> DocumentManager::SaveAs(const QString& path) {
  assert(history_ != nullptr);
  assert(!path.isEmpty());
  QString target = QFileInfo(path).absoluteFilePath();
  const bool has_suffix =
      QFileInfo(target).suffix() == QLatin1String(kProjectSuffix);
  if (!has_suffix) {
    target += QLatin1Char('.') + QLatin1String(kProjectSuffix);
  }
  auto written = WriteProject(target, history_->current());
  if (!written) {
    return written;
  }
  history_->MarkSaved();
  DiscardAutosave();
  const bool is_moved = target != path_;
  path_ = target;
  Remember(path_);
  if (is_moved) {
    emit PathChanged();
  }
  return {};
}

QStringList DocumentManager::RecentFiles() const {
  assert(!data_folder_.isEmpty());
  const QSettings settings(SettingsPath(), QSettings::IniFormat);
  QStringList recent = settings.value("recent").toStringList();
  recent.removeIf([](const QString& file) { return !QFile::exists(file); });
  assert(recent.size() <= kMaxRecentFiles);
  return recent;
}

Result<void> DocumentManager::Rename(const QString& name) {
  assert(history_ != nullptr);
  assert(name.size() < 100000);
  const bool is_blank = name.trimmed().isEmpty();
  if (is_blank) {
    return std::unexpected(Error{Tr("A project needs a name.")});
  }
  Project next = history_->current();
  next.name = name.trimmed();
  return history_->Apply(Tr("Rename project"), std::move(next));
}

Result<void> DocumentManager::SetCanvas(CanvasSize canvas) {
  assert(history_ != nullptr);
  assert(kMinCanvasSide > 0);
  const bool is_valid = IsValidCanvas(canvas);
  if (!is_valid) {
    return std::unexpected(
        Error{Tr("The size must be even, from %1 to %2 pixels.")
                  .arg(kMinCanvasSide)
                  .arg(kMaxCanvasSide)});
  }
  Project next = history_->current();
  next.canvas = canvas;
  return history_->Apply(Tr("Change size"), std::move(next));
}

Result<void> DocumentManager::SetSong(const QString& song) {
  assert(history_ != nullptr);
  assert(song.size() < 100000);
  const bool is_missing = !song.isEmpty() && !QFile::exists(song);
  if (is_missing) {
    return std::unexpected(Error{Tr("%1 doesn't exist.").arg(song)});
  }
  Project next = history_->current();
  next.song = song.isEmpty() ? QString() : QFileInfo(song).absoluteFilePath();
  return history_->Apply(song.isEmpty() ? Tr("Remove song") : Tr("Set song"),
                         std::move(next));
}

Result<void> DocumentManager::Autosave() {
  assert(history_ != nullptr);
  assert(!data_folder_.isEmpty());
  const bool is_needed = history_->IsDirty() && !history_->IsScopeOpen();
  if (!is_needed) {
    return {};
  }
  return WriteProject(AutosavePath(), history_->current());
}

QString DocumentManager::AutosavePath() const {
  assert(!data_folder_.isEmpty());
  const QString name =
      QStringLiteral("autosave.") + QLatin1String(kProjectSuffix);
  assert(!name.isEmpty());
  return QDir(data_folder_).filePath(name);
}

bool DocumentManager::HasAutosave() const {
  assert(!data_folder_.isEmpty());
  const QString autosave = AutosavePath();
  assert(!autosave.isEmpty());
  return QFile::exists(autosave);
}

Result<void> DocumentManager::Recover() {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
  auto project = ReadProject(AutosavePath());
  if (!project) {
    return std::unexpected(project.error());
  }
  // Recovered work is unsaved until the user saves it somewhere.
  history_->Reset(std::move(*project), false);
  path_.clear();
  emit PathChanged();
  return {};
}

void DocumentManager::DiscardAutosave() {
  assert(!data_folder_.isEmpty());
  const QString autosave = AutosavePath();
  QFile::remove(autosave);
  assert(!QFile::exists(autosave));
}

void DocumentManager::Remember(const QString& path) {
  assert(!path.isEmpty());
  QSettings settings(SettingsPath(), QSettings::IniFormat);
  QStringList recent = settings.value("recent").toStringList();
  recent.removeAll(path);
  recent.prepend(path);
  const qsizetype extra = recent.size() - kMaxRecentFiles;
  const bool is_too_long = extra > 0;
  if (is_too_long) {
    recent.remove(kMaxRecentFiles, extra);
  }
  settings.setValue("recent", recent);
  assert(recent.size() <= kMaxRecentFiles);
  emit RecentFilesChanged();
}

QString DocumentManager::SettingsPath() const {
  assert(!data_folder_.isEmpty());
  const QString path = QDir(data_folder_).filePath("settings.ini");
  assert(!path.isEmpty());
  return path;
}

}  // namespace snapper
