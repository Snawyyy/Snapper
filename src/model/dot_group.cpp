#include "model/dot_group.h"

#include <algorithm>
#include <cassert>

namespace snapper {

const DotGroup* FindGroup(const std::vector<DotGroup>& groups,
                          const QString& name) {
  assert(groups.size() <= static_cast<size_t>(kMaxDotGroups));
  assert(name.size() < 100000);
  const auto found = std::ranges::find_if(
      groups, [&name](const DotGroup& g) { return g.name == name; });
  return found == groups.end() ? nullptr : &*found;
}

DotGroup* FindGroup(std::vector<DotGroup>* groups, const QString& name) {
  assert(groups != nullptr);
  assert(name.size() < 100000);
  const DotGroup* group =
      FindGroup(static_cast<const std::vector<DotGroup>&>(*groups), name);
  return const_cast<DotGroup*>(group);
}

QString FreshGroupName(const std::vector<DotGroup>& groups) {
  assert(groups.size() <= static_cast<size_t>(kMaxDotGroups));
  assert(kMaxDotGroups > 0);
  for (int number = 1; number <= kMaxDotGroups + 1; ++number) {
    const QString name = QStringLiteral("Group %1").arg(number);
    const bool is_free = FindGroup(groups, name) == nullptr;
    if (is_free) {
      return name;
    }
  }
  return QString();
}

}  // namespace snapper
