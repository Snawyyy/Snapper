#include "edit/doll_library_manager.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

#include <cassert>
#include <variant>

#include "base/text.h"
#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/mesh_edits.h"
#include "io/ora_file.h"

namespace snapper {
namespace {

constexpr char kDollSuffix[] = ".doll";

}  // namespace

DollLibraryManager::DollLibraryManager(HistoryManager* history,
                                       QString library_folder)
    : history_(history), folder_(std::move(library_folder)) {
  assert(history_ != nullptr);
  assert(!folder_.isEmpty());
  QDir().mkpath(folder_);
  ConvertOldDolls();
  watcher_.addPath(folder_);
  connect(&watcher_, &QFileSystemWatcher::directoryChanged, this,
          &DollLibraryManager::Rescan);
}

QStringList DollLibraryManager::Available() const {
  assert(!folder_.isEmpty());
  const QStringList folders =
      QDir(folder_).entryList({QStringLiteral("*") + kDollSuffix},
                              QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  QStringList names;
  for (const QString& entry : folders) {
    names.append(entry.chopped(static_cast<qsizetype>(sizeof(kDollSuffix)) -
                               1));
  }
  assert(names.size() == folders.size());
  return names;
}

QString DollLibraryManager::WhyNoImport(const QString& name) const {
  assert(history_ != nullptr);
  assert(name.size() < 100000);
  const Project& project = history_->current();
  const bool is_unpicked = name.isEmpty();
  const bool is_present = !is_unpicked && FindDoll(project, name) != nullptr;
  const bool is_full =
      project.dolls.size() >= static_cast<size_t>(kMaxProjectDolls);
  if (is_unpicked) {
    return Tr("Pick a doll in the library first.");
  }
  if (is_present) {
    return Tr("%1 is already in the project; reload it for the latest art.")
        .arg(name);
  }
  return is_full ? Tr("A project holds at most %1 dolls.")
                       .arg(kMaxProjectDolls)
                 : QString();
}

Result<ReconcileReport> DollLibraryManager::Import(const QString& name) {
  assert(!name.isEmpty());
  assert(history_ != nullptr);
  const QString why_not = WhyNoImport(name);
  const bool can_import = why_not.isEmpty();
  if (!can_import) {
    return std::unexpected(Error{why_not});
  }
  const Project& project = history_->current();
  auto loaded = LoadDoll(FolderOf(name));
  if (!loaded) {
    return std::unexpected(loaded.error());
  }
  Project next = project;
  next.dolls[name] = std::make_shared<const Doll>(loaded->doll);
  auto applied = history_->Apply(Tr("Import %1").arg(name), std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  stamps_[name] = ArtStamp(name);
  return loaded->report;
}

Result<ReconcileReport> DollLibraryManager::Reload(const QString& name) {
  assert(history_ != nullptr);
  assert(!name.isEmpty());
  const Doll* doll = FindDoll(history_->current(), name);
  const bool is_present = doll != nullptr;
  if (!is_present) {
    return std::unexpected(
        Error{Tr("%1 isn't in the project.").arg(name)});
  }
  auto art = ReadArt(FolderOf(name));
  if (!art) {
    return std::unexpected(art.error());
  }
  ReconcileReport report;
  Doll fresh = *doll;
  fresh.art = *art;
  fresh.rig = Reconcile(*art, doll->rig, &report);
  report.removed_drawings = RemovedDrawings(doll->art, *art);
  Project next = history_->current();
  next.dolls[name] = std::make_shared<const Doll>(std::move(fresh));
  // Redrawn pieces get their shape mesh made again, keeping their dots.
  CarryCount carried;
  next = WithFreshMeshes(next, name, &carried, &report.remeshed);
  report.carried = carried.carried;
  report.lost = carried.dropped;
  auto applied = history_->Apply(Tr("Reload %1").arg(name), std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  stamps_[name] = ArtStamp(name);
  return report;
}

Result<std::map<QString, ReconcileReport>> DollLibraryManager::ImportAll(
    const QStringList& names) {
  assert(history_ != nullptr);
  assert(names.size() <= kMaxProjectDolls);
  EditScope batch(history_, Tr("Import dolls"));
  std::map<QString, ReconcileReport> reports;
  for (const QString& name : names) {
    auto report = Import(name);
    if (!report) {
      batch.Cancel();
      return std::unexpected(report.error());
    }
    reports[name] = *report;
  }
  return reports;
}

Result<std::map<QString, ReconcileReport>> DollLibraryManager::ReloadAll(
    const QStringList& names) {
  assert(history_ != nullptr);
  assert(names.size() <= kMaxProjectDolls);
  EditScope batch(history_, Tr("Reload dolls"));
  std::map<QString, ReconcileReport> reports;
  for (const QString& name : names) {
    auto report = Reload(name);
    if (!report) {
      batch.Cancel();
      return std::unexpected(report.error());
    }
    reports[name] = *report;
  }
  return reports;
}

Result<void> DollLibraryManager::SaveRig(const QString& name) {
  assert(history_ != nullptr);
  assert(!name.isEmpty());
  const Doll* doll = FindDoll(history_->current(), name);
  const bool is_present = doll != nullptr;
  if (!is_present) {
    return std::unexpected(
        Error{Tr("%1 isn't in the project.").arg(name)});
  }
  // The library's own note of the doll's Krita file is not the
  // project's to drop.
  Rig rig = doll->rig;
  rig.krita_file = KritaFileOf(name);
  return WriteRig(FolderOf(name), rig);
}

QString DollLibraryManager::KritaFileOf(const QString& name) const {
  assert(!name.isEmpty());
  const auto rig = ReadRig(FolderOf(name));
  return rig ? rig->krita_file : QString();
}

QString DollLibraryManager::SuggestedKritaFile(const QString& name) const {
  assert(!name.isEmpty());
  assert(!folder_.isEmpty());
  const QString kept = KritaFileOf(name);
  if (!kept.isEmpty()) {
    return kept;
  }
  return QFileInfo(folder_).dir().filePath(name + QStringLiteral(".ora"));
}

bool DollLibraryManager::IsKritaFileStale(const QString& name) const {
  assert(!name.isEmpty());
  const QFileInfo file(KritaFileOf(name));
  const bool is_missing = file.filePath().isEmpty() || !file.exists();
  if (is_missing) {
    return true;
  }
  return file.lastModified() < ArtStamp(name);
}

Result<void> DollLibraryManager::ExportToKrita(const QString& name,
                                               const QString& path) {
  assert(!name.isEmpty());
  assert(history_ != nullptr);
  if (path.isEmpty()) {
    return std::unexpected(Error{Tr("Pick where to save the Krita file.")});
  }
  auto loaded = LoadDoll(FolderOf(name));
  if (!loaded) {
    return std::unexpected(loaded.error());
  }
  Doll doll = loaded->doll;
  const Doll* in_project = FindDoll(history_->current(), name);
  if (in_project != nullptr) {
    doll.rig = in_project->rig;
  }
  auto library_rig = ReadRig(FolderOf(name));
  if (!library_rig) {
    return std::unexpected(library_rig.error());
  }
  library_rig->krita_file = QFileInfo(path).absoluteFilePath();
  // The note goes in first so the Krita file is the newer of the two.
  auto noted = WriteRig(FolderOf(name), *library_rig);
  if (!noted) {
    return noted;
  }
  return WriteDollOra(doll, path);
}

QString DollLibraryManager::KritaProgram() const {
  assert(!folder_.isEmpty());
  const QSettings settings(SettingsPath(), QSettings::IniFormat);
  const QString set = settings.value(QStringLiteral("krita_program")).toString();
  const bool is_set = !set.isEmpty() && QFileInfo(set).isExecutable();
  return is_set ? set : QStandardPaths::findExecutable(QStringLiteral("krita"));
}

void DollLibraryManager::SetKritaProgram(const QString& program) {
  assert(!folder_.isEmpty());
  QSettings settings(SettingsPath(), QSettings::IniFormat);
  settings.setValue(QStringLiteral("krita_program"), program);
}

Result<void> DollLibraryManager::OpenInKrita(const QString& path) const {
  assert(!path.isEmpty());
  const QString program = KritaProgram();
  if (program.isEmpty()) {
    return std::unexpected(Error{
        Tr("Krita wasn't found. Set its program under File > Krita "
           "program.")});
  }
  const bool is_started = QProcess::startDetached(program, {path});
  if (!is_started) {
    return std::unexpected(Error{Tr("Can't start %1.").arg(program)});
  }
  return {};
}

QString DollLibraryManager::SettingsPath() const {
  assert(!folder_.isEmpty());
  // The library sits in the data folder, beside the settings file
  // DocumentManager keeps there.
  return QFileInfo(folder_).dir().filePath(QStringLiteral("settings.ini"));
}

Result<void> DollLibraryManager::Remove(const QString& name) {
  assert(history_ != nullptr);
  assert(!name.isEmpty());
  const int uses = UseCount(name);
  const bool is_used = uses > 0;
  if (is_used) {
    return std::unexpected(Error{
        Tr("%1 is still on %2 layer(s); remove those first.")
            .arg(name)
            .arg(uses)});
  }
  Project next = history_->current();
  const bool is_present = next.dolls.erase(name) > 0;
  if (!is_present) {
    return std::unexpected(
        Error{Tr("%1 isn't in the project.").arg(name)});
  }
  stamps_.erase(name);
  return history_->Apply(Tr("Remove %1").arg(name), std::move(next));
}

int DollLibraryManager::UseCount(const QString& name) const {
  assert(history_ != nullptr);
  assert(!name.isEmpty());
  int uses = 0;
  for (const auto& shot : history_->current().shots) {
    for (const Layer& layer : shot->layers) {
      const auto* doll = std::get_if<DollLayer>(&layer.content);
      const bool is_use = doll != nullptr && doll->doll == name;
      uses += is_use ? 1 : 0;
    }
  }
  return uses;
}

QString DollLibraryManager::FolderOf(const QString& name) const {
  assert(!name.isEmpty());
  assert(!folder_.isEmpty());
  return QDir(folder_).filePath(name + QLatin1String(kDollSuffix));
}

QDateTime DollLibraryManager::ArtStamp(const QString& name) const {
  assert(!name.isEmpty());
  const QDir dir(FolderOf(name));
  const QString art = dir.filePath(QLatin1String(kArtFileName));
  const QString legacy = dir.filePath(QLatin1String(kLegacyFileName));
  const QFileInfo info(QFileInfo::exists(art) ? art : legacy);
  assert(!info.filePath().isEmpty());
  return info.lastModified();
}

void DollLibraryManager::ConvertOldDolls() {
  assert(!folder_.isEmpty());
  problems_.clear();
  const QStringList names = Available();
  for (const QString& name : names) {
    const auto converted = ConvertLegacyDoll(FolderOf(name));
    if (!converted) {
      problems_.append(converted.error().message);
    }
  }
  assert(problems_.size() <= names.size());
}

void DollLibraryManager::Rescan() {
  assert(history_ != nullptr);
  assert(stamps_.size() <= static_cast<size_t>(kMaxProjectDolls));
  ConvertOldDolls();
  emit LibraryChanged();
  for (auto& [name, stamp] : stamps_) {
    const QDateTime now = ArtStamp(name);
    const bool is_reexported = now.isValid() && now != stamp;
    if (is_reexported) {
      stamp = now;
      emit ArtChanged(name);
    }
  }
}

}  // namespace snapper
