// LibraryFolderManager's edits and its file.

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <cassert>

#include "edit/library_folder_manager.h"
#include "edit/library_folder_paths.h"

namespace snapper {
namespace {

constexpr int kMaxFolders = 2000;
constexpr int kMaxNameLength = 80;

}  // namespace

Result<void> LibraryFolderManager::CheckName(const QString& parent,
                                             const QString& name) const {
  assert(Exists(parent));
  assert(folders_.size() <= kMaxFolders);
  const bool is_blank = name.trimmed().isEmpty();
  if (is_blank) {
    return std::unexpected(Error{QObject::tr("Give the folder a name.")});
  }
  const bool has_bad_char = name.contains('/') || name.contains('\\');
  if (has_bad_char) {
    return std::unexpected(
        Error{QObject::tr("A folder name cannot have a slash in it.")});
  }
  const bool is_long = name.size() > kMaxNameLength;
  if (is_long) {
    return std::unexpected(Error{QObject::tr("That name is too long.")});
  }
  const bool is_taken = folders_.contains(Join(parent, name));
  if (is_taken) {
    return std::unexpected(Error{
        QObject::tr("There is already a folder named %1 here.").arg(name)});
  }
  const bool is_full = static_cast<int>(folders_.size()) >= kMaxFolders;
  if (is_full) {
    return std::unexpected(Error{QObject::tr("Too many folders.")});
  }
  return {};
}

void LibraryFolderManager::Settled() {
  assert(Exists(current_));
  assert(folders_.size() <= kMaxFolders);
  current_ = NearestSeen(current_);
  Save();
  emit Changed();
}

void LibraryFolderManager::SetShowHidden(bool is_shown) {
  assert(hidden_.size() <= kMaxFolders);
  assert(Exists(current_));
  const bool is_same = is_shown == show_hidden_;
  if (is_same) {
    return;
  }
  show_hidden_ = is_shown;
  Settled();
}

Result<void> LibraryFolderManager::SetHidden(const QString& path,
                                             bool is_hidden) {
  assert(hidden_.size() <= kMaxFolders);
  assert(!path.startsWith('/'));
  const bool is_folder = !path.isEmpty() && folders_.contains(path);
  if (!is_folder) {
    return std::unexpected(Error{QObject::tr("There is no such folder.")});
  }
  if (is_hidden) {
    hidden_.insert(path);
  } else {
    hidden_.erase(path);
  }
  Settled();
  return {};
}

Result<QString> LibraryFolderManager::AddFolder(const QString& parent,
                                                const QString& name) {
  assert(!parent.startsWith('/'));
  assert(folders_.size() <= kMaxFolders);
  const bool is_known = Exists(parent);
  if (!is_known) {
    return std::unexpected(Error{QObject::tr("There is no such folder.")});
  }
  const QString clean = name.trimmed();
  const Result<void> fine = CheckName(parent, clean);
  if (!fine) {
    return std::unexpected(fine.error());
  }
  const QString path = Join(parent, clean);
  folders_.insert(path);
  Settled();
  return path;
}

Result<void> LibraryFolderManager::RenameFolder(const QString& path,
                                                const QString& name) {
  assert(!path.startsWith('/'));
  assert(folders_.size() <= kMaxFolders);
  const bool is_folder = !path.isEmpty() && folders_.contains(path);
  if (!is_folder) {
    return std::unexpected(Error{QObject::tr("There is no such folder.")});
  }
  const QString clean = name.trimmed();
  const bool is_same = clean == NameOf(path);
  if (is_same) {
    return {};
  }
  const QString parent = ParentOf(path);
  const Result<void> fine = CheckName(parent, clean);
  if (!fine) {
    return fine;
  }
  const QString to = Join(parent, clean);
  const auto move = [&](const QString& at) {
    return IsWithin(at, path) ? Moved(at, path, to) : at;
  };
  std::set<QString> folders;
  for (const QString& folder : folders_) {
    folders.insert(move(folder));
  }
  folders_ = std::move(folders);
  std::set<QString> hidden;
  for (const QString& folder : hidden_) {
    hidden.insert(move(folder));
  }
  hidden_ = std::move(hidden);
  for (auto& [doll, folder] : doll_folder_) {
    folder = move(folder);
  }
  current_ = move(current_);
  for (QString& at : back_) {
    at = move(at);
  }
  for (QString& at : forward_) {
    at = move(at);
  }
  Settled();
  return {};
}

Result<void> LibraryFolderManager::RemoveFolder(const QString& path) {
  assert(!path.startsWith('/'));
  assert(folders_.size() <= kMaxFolders);
  const bool is_folder = !path.isEmpty() && folders_.contains(path);
  if (!is_folder) {
    return std::unexpected(Error{QObject::tr("There is no such folder.")});
  }
  const QString parent = ParentOf(path);
  for (const QString& folder : folders_) {
    const bool is_child = ParentOf(folder) == path;
    const bool is_clash =
        is_child && folders_.contains(Join(parent, NameOf(folder)));
    if (is_clash) {
      return std::unexpected(Error{
          QObject::tr("%1 already has a folder named %2.")
              .arg(AddressOf(parent), NameOf(folder))});
    }
  }
  const auto move = [&](const QString& at) {
    return IsWithin(at, path) ? Moved(at, path, parent) : at;
  };
  std::set<QString> folders;
  for (const QString& folder : folders_) {
    const bool is_kept = folder != path;
    if (is_kept) {
      folders.insert(move(folder));
    }
  }
  folders_ = std::move(folders);
  hidden_.erase(path);
  std::set<QString> hidden;
  for (const QString& folder : hidden_) {
    hidden.insert(move(folder));
  }
  hidden_ = std::move(hidden);
  for (auto& [doll, folder] : doll_folder_) {
    folder = move(folder);
  }
  current_ = move(current_);
  for (QString& at : back_) {
    at = move(at);
  }
  for (QString& at : forward_) {
    at = move(at);
  }
  Settled();
  return {};
}

Result<void> LibraryFolderManager::MoveDolls(const QStringList& dolls,
                                             const QString& folder) {
  assert(!folder.startsWith('/'));
  assert(dolls.size() < kMaxFolders * 100);
  const bool is_known = Exists(folder);
  if (!is_known) {
    return std::unexpected(Error{QObject::tr("There is no such folder.")});
  }
  for (const QString& doll : dolls) {
    const bool is_top = folder.isEmpty();
    if (is_top) {
      doll_folder_.erase(doll);
    } else {
      doll_folder_[doll] = folder;
    }
  }
  Settled();
  return {};
}

void LibraryFolderManager::Save() const {
  assert(!file_path_.isEmpty());
  assert(folders_.size() <= kMaxFolders);
  // A damaged file is left as it is, so it can be fixed by hand.
  const bool is_safe = load_error_.isEmpty();
  if (!is_safe) {
    return;
  }
  QJsonObject root;
  QJsonArray folders;
  for (const QString& folder : folders_) {
    folders.append(folder);
  }
  root["folders"] = folders;
  QJsonArray hidden;
  for (const QString& folder : hidden_) {
    hidden.append(folder);
  }
  root["hidden"] = hidden;
  QJsonObject dolls;
  for (const auto& [doll, folder] : doll_folder_) {
    dolls[doll] = folder;
  }
  root["dolls"] = dolls;
  root["show_hidden"] = show_hidden_;
  QSaveFile file(file_path_);
  const bool is_open = file.open(QIODevice::WriteOnly);
  if (!is_open) {
    return;
  }
  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  file.commit();
}

void LibraryFolderManager::Load() {
  assert(folders_.empty());
  assert(!file_path_.isEmpty());
  QFile file(file_path_);
  const bool has_file = file.exists();
  if (!has_file) {
    return;
  }
  const bool is_open = file.open(QIODevice::ReadOnly);
  QJsonParseError problem;
  const QJsonDocument document =
      is_open ? QJsonDocument::fromJson(file.readAll(), &problem)
              : QJsonDocument();
  const bool is_readable = is_open && document.isObject();
  if (!is_readable) {
    load_error_ = QObject::tr("The library folders file could not be read: %1")
                      .arg(file_path_);
    return;
  }
  const QJsonObject root = document.object();
  for (const QJsonValue& folder : root["folders"].toArray()) {
    const QString path = folder.toString();
    const bool is_usable = !path.isEmpty() && !path.startsWith('/') &&
                           !path.endsWith('/') &&
                           static_cast<int>(folders_.size()) < kMaxFolders;
    if (is_usable) {
      folders_.insert(path);
    }
  }
  for (const QJsonValue& folder : root["hidden"].toArray()) {
    const bool is_known = folders_.contains(folder.toString());
    if (is_known) {
      hidden_.insert(folder.toString());
    }
  }
  const QJsonObject dolls = root["dolls"].toObject();
  for (auto it = dolls.begin(); it != dolls.end(); ++it) {
    doll_folder_[it.key()] = it.value().toString();
  }
  show_hidden_ = root["show_hidden"].toBool();
}

}  // namespace snapper
