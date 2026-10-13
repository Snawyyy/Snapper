#include "edit/library_folder_manager.h"

#include <algorithm>
#include <cassert>

#include "edit/library_folder_paths.h"

namespace snapper {
namespace {

constexpr int kMaxFolders = 2000;
constexpr int kMaxHistory = 64;

QString TopName() {
  assert(kMaxHistory > 0);
  assert(kMaxFolders > 0);
  return QStringLiteral("Library");
}

void SortNames(QStringList* names) {
  assert(names != nullptr);
  assert(names->size() <= kMaxFolders * 100);
  std::ranges::sort(*names, [](const QString& a, const QString& b) {
    return a.compare(b, Qt::CaseInsensitive) < 0;
  });
}

}  // namespace

LibraryFolderManager::LibraryFolderManager(QString file_path)
    : file_path_(std::move(file_path)) {
  assert(!file_path_.isEmpty());
  assert(current_.isEmpty());
  Load();
}

QString LibraryFolderManager::AddressOf(const QString& path) {
  assert(!path.startsWith('/'));
  assert(!path.endsWith('/'));
  return path.isEmpty() ? TopName() : TopName() + '/' + path;
}

bool LibraryFolderManager::Exists(const QString& path) const {
  assert(!path.startsWith('/'));
  assert(folders_.size() <= kMaxFolders);
  return path.isEmpty() || folders_.contains(path);
}

bool LibraryFolderManager::IsHidden(const QString& path) const {
  assert(!path.startsWith('/'));
  assert(hidden_.size() <= kMaxFolders);
  return hidden_.contains(path);
}

bool LibraryFolderManager::IsConcealed(const QString& path) const {
  assert(!path.startsWith('/'));
  assert(hidden_.size() <= kMaxFolders);
  if (show_hidden_) {
    return false;
  }
  for (QString at = path; !at.isEmpty(); at = ParentOf(at)) {
    const bool is_hidden = hidden_.contains(at);
    if (is_hidden) {
      return true;
    }
  }
  return false;
}

QString LibraryFolderManager::NearestSeen(const QString& path) const {
  assert(!path.startsWith('/'));
  assert(folders_.size() <= kMaxFolders);
  QString at = path;
  while (!at.isEmpty() && (!Exists(at) || IsConcealed(at))) {
    at = ParentOf(at);
  }
  return at;
}

QString LibraryFolderManager::FolderOf(const QString& doll) const {
  assert(!doll.isEmpty());
  assert(doll_folder_.size() < kMaxFolders * 100);
  const auto found = doll_folder_.find(doll);
  const bool is_placed = found != doll_folder_.end() && Exists(found->second);
  return is_placed ? found->second : QString();
}

QStringList LibraryFolderManager::FoldersIn(const QString& path) const {
  assert(Exists(path));
  assert(folders_.size() <= kMaxFolders);
  QStringList names;
  for (const QString& folder : folders_) {
    const bool is_here = ParentOf(folder) == path && !IsConcealed(folder);
    if (is_here) {
      names.append(NameOf(folder));
    }
  }
  SortNames(&names);
  return names;
}

QStringList LibraryFolderManager::DollsIn(const QString& path,
                                          const QStringList& all) const {
  assert(Exists(path));
  assert(all.size() < kMaxFolders * 100);
  QStringList dolls;
  for (const QString& doll : all) {
    const bool is_here = FolderOf(doll) == path;
    if (is_here) {
      dolls.append(doll);
    }
  }
  return dolls;
}

QStringList LibraryFolderManager::AllFolders() const {
  assert(folders_.size() <= kMaxFolders);
  assert(hidden_.size() <= kMaxFolders);
  QStringList paths(folders_.begin(), folders_.end());
  SortNames(&paths);
  return paths;
}

Result<void> LibraryFolderManager::Open(const QString& path) {
  assert(!path.startsWith('/'));
  assert(back_.size() <= kMaxHistory);
  const bool is_known = Exists(path);
  if (!is_known) {
    return std::unexpected(Error{QObject::tr("There is no folder %1.")
                                     .arg(AddressOf(path))});
  }
  const bool is_concealed = IsConcealed(path);
  if (is_concealed) {
    return std::unexpected(Error{
        QObject::tr("%1 is hidden. Turn on View > Show hidden folders.")
            .arg(AddressOf(path))});
  }
  const bool is_here = path == current_;
  if (is_here) {
    return {};
  }
  back_.push_back(current_);
  const bool is_long = back_.size() > kMaxHistory;
  if (is_long) {
    back_.erase(back_.begin());
  }
  forward_.clear();
  current_ = path;
  emit Changed();
  return {};
}

Result<void> LibraryFolderManager::OpenAddress(const QString& address) {
  assert(back_.size() <= kMaxHistory);
  assert(forward_.size() <= kMaxHistory);
  QStringList parts = address.trimmed().split('/', Qt::SkipEmptyParts);
  for (QString& part : parts) {
    part = part.trimmed();
  }
  const bool has_top =
      !parts.isEmpty() && parts.front().compare(TopName(), Qt::CaseInsensitive)
                              == 0;
  if (has_top) {
    parts.removeFirst();
  }
  return Open(parts.join('/'));
}

QString LibraryFolderManager::WhyNoBack() const {
  assert(back_.size() <= kMaxHistory);
  assert(forward_.size() <= kMaxHistory);
  return back_.empty() ? QObject::tr("No folder visited before this one.")
                       : QString();
}

QString LibraryFolderManager::WhyNoForward() const {
  assert(back_.size() <= kMaxHistory);
  assert(forward_.size() <= kMaxHistory);
  return forward_.empty() ? QObject::tr("You have not gone back.")
                          : QString();
}

QString LibraryFolderManager::WhyNoUp() const {
  assert(Exists(current_));
  assert(back_.size() <= kMaxHistory);
  return current_.isEmpty() ? QObject::tr("Already at the top of the library.")
                            : QString();
}

void LibraryFolderManager::Back() {
  assert(back_.size() <= kMaxHistory);
  assert(forward_.size() <= kMaxHistory);
  const bool has_back = !back_.empty();
  if (!has_back) {
    return;
  }
  forward_.push_back(current_);
  current_ = NearestSeen(back_.back());
  back_.pop_back();
  emit Changed();
}

void LibraryFolderManager::Forward() {
  assert(back_.size() <= kMaxHistory);
  assert(forward_.size() <= kMaxHistory);
  const bool has_forward = !forward_.empty();
  if (!has_forward) {
    return;
  }
  back_.push_back(current_);
  current_ = NearestSeen(forward_.back());
  forward_.pop_back();
  emit Changed();
}

void LibraryFolderManager::Up() {
  assert(Exists(current_));
  assert(back_.size() <= kMaxHistory);
  const bool is_top = current_.isEmpty();
  if (is_top) {
    return;
  }
  [[maybe_unused]] const bool is_opened = Open(ParentOf(current_)).has_value();
  assert(is_opened);
}

}  // namespace snapper
