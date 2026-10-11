#include "edit/group_manager.h"

#include <QStringList>

#include <algorithm>
#include <cassert>

#include "base/text.h"
#include "edit/history_manager.h"

namespace snapper {
namespace {

// The longest name a group may have, so its chip stays readable.
constexpr int kMostNameLength = 40;

}  // namespace

GroupManager::GroupManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
}

std::vector<ShownGroup> GroupManager::GroupsOf(ShotId shot) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  return ShownGroupsOf(history_->current(), shot);
}

QString GroupManager::WhyNoGroup(ShotId shot,
                                 const std::set<WarpDot>& dots) const {
  assert(history_ != nullptr);
  assert(dots.size() < 10000000);
  const bool has_dots = !dots.empty();
  if (!has_dots) {
    return Tr("Pick some dots first.");
  }
  const bool is_full =
      GroupsOf(shot).size() >= static_cast<size_t>(kMaxDotGroups);
  return is_full ? Tr("A shot shows at most %1 groups.").arg(kMaxDotGroups)
                 : QString();
}

Result<ShownGroup> GroupManager::Find(ShotId shot,
                                      const QString& name) const {
  assert(history_ != nullptr);
  assert(name.size() < 100000);
  for (ShownGroup& group : GroupsOf(shot)) {
    const bool is_it = group.name == name;
    if (is_it) {
      return std::move(group);
    }
  }
  return std::unexpected(Error{Tr("There is no group called %1.").arg(name)});
}

Result<void> GroupManager::Write(const QString& label, ShotId shot,
                                 const QString& name,
                                 const std::optional<ShownGroup>& group) {
  assert(history_ != nullptr);
  assert(!label.isEmpty());
  return history_->Apply(label,
                         WithGroupWritten(history_->current(), shot, name,
                                          group));
}

QString GroupManager::SuggestedName(ShotId shot) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  std::vector<DotGroup> used;
  for (const ShownGroup& group : GroupsOf(shot)) {
    used.push_back({group.name, {}});
  }
  return FreshGroupName(used);
}

Result<QString> GroupManager::Create(ShotId shot,
                                     const std::set<WarpDot>& dots,
                                     const QString& name) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const QString why_not = WhyNoGroup(shot, dots);
  const bool can_group = why_not.isEmpty();
  if (!can_group) {
    return std::unexpected(Error{why_not});
  }
  const QString trimmed = name.simplified().left(kMostNameLength);
  const QString made = trimmed.isEmpty() ? SuggestedName(shot) : trimmed;
  const Result<void> done =
      Write(Tr("Group %1 dots").arg(dots.size()), shot, QString(),
            ShownGroup{made, dots, false});
  const bool is_failed = !done;
  if (is_failed) {
    return std::unexpected(done.error());
  }
  return made;
}

Result<void> GroupManager::Rename(ShotId shot, const QString& name,
                                  const QString& to) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const QString trimmed = to.simplified().left(kMostNameLength);
  const bool is_named = !trimmed.isEmpty();
  if (!is_named) {
    return std::unexpected(Error{Tr("A group needs a name.")});
  }
  Result<ShownGroup> group = Find(shot, name);
  const bool is_found = group.has_value();
  if (!is_found) {
    return std::unexpected(group.error());
  }
  group->name = trimmed;
  return Write(Tr("Rename group %1").arg(name), shot, name, *group);
}

Result<void> GroupManager::Delete(ShotId shot, const QString& name) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const Result<ShownGroup> group = Find(shot, name);
  const bool is_found = group.has_value();
  if (!is_found) {
    return std::unexpected(group.error());
  }
  return Write(Tr("Delete group %1").arg(name), shot, name, std::nullopt);
}

Result<void> GroupManager::AddDots(ShotId shot, const QString& name,
                                   const std::set<WarpDot>& dots) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  Result<ShownGroup> group = Find(shot, name);
  const bool is_found = group.has_value();
  if (!is_found) {
    return std::unexpected(group.error());
  }
  const bool has_dots = !dots.empty();
  if (!has_dots) {
    return std::unexpected(Error{Tr("Pick some dots first.")});
  }
  group->dots.insert(dots.begin(), dots.end());
  return Write(Tr("Add dots to %1").arg(name), shot, name, *group);
}

Result<void> GroupManager::RemoveDots(ShotId shot, const QString& name,
                                      const std::set<WarpDot>& dots) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  Result<ShownGroup> group = Find(shot, name);
  const bool is_found = group.has_value();
  if (!is_found) {
    return std::unexpected(group.error());
  }
  const bool has_dots = !dots.empty();
  if (!has_dots) {
    return std::unexpected(Error{Tr("Pick some dots first.")});
  }
  for (const WarpDot& dot : dots) {
    group->dots.erase(dot);
  }
  const bool is_empty = group->dots.empty();
  return Write(Tr("Remove dots from %1").arg(name), shot, name,
               is_empty ? std::nullopt : std::optional<ShownGroup>(*group));
}

Result<void> GroupManager::SetDefault(ShotId shot, const QString& name,
                                      bool is_default) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  Result<ShownGroup> group = Find(shot, name);
  const bool is_found = group.has_value();
  if (!is_found) {
    return std::unexpected(group.error());
  }
  const bool is_same = group->is_default == is_default;
  if (is_same) {
    return std::unexpected(Error{is_default
                                     ? Tr("%1 is already a default.").arg(name)
                                     : Tr("%1 is already only this shot's.")
                                           .arg(name)});
  }
  group->is_default = is_default;
  const QString label = is_default ? Tr("Make %1 a default").arg(name)
                                   : Tr("Make %1 only this shot's").arg(name);
  return Write(label, shot, name, *group);
}

}  // namespace snapper
