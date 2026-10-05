#include "edit/doll_library_manager.h"

#include <QDir>
#include <QFileInfo>

#include <cassert>
#include <variant>

#include "base/text.h"
#include "edit/history_manager.h"

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
  Project next = history_->current();
  next.dolls[name] = std::make_shared<const Doll>(std::move(fresh));
  auto applied = history_->Apply(Tr("Reload %1").arg(name), std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  stamps_[name] = ArtStamp(name);
  return report;
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
  return WriteRig(FolderOf(name), doll->rig);
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

void DollLibraryManager::Rescan() {
  assert(history_ != nullptr);
  assert(stamps_.size() <= static_cast<size_t>(kMaxProjectDolls));
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
