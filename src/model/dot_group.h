#ifndef SNAPPER_MODEL_DOT_GROUP_H_
#define SNAPPER_MODEL_DOT_GROUP_H_

#include <QString>

#include <vector>

#include "model/layer.h"

namespace snapper {

constexpr int kMaxDotGroups = 32;
constexpr int kMaxGroupDots = 2048;

// One warp dot of a group. In a shot's group the layer says which doll
// layer the dot is on; in a doll's default group it is no layer, and the
// dot is on every layer of that doll.
struct GroupDot final {
  LayerId layer;
  QString piece;
  int point = -1;

  auto operator<=>(const GroupDot&) const = default;
  bool operator==(const GroupDot&) const = default;
};

// A named set of dots, so a whole set is picked with one click.
struct DotGroup final {
  QString name;
  std::vector<GroupDot> dots;

  bool operator==(const DotGroup&) const = default;
};

// The group called name, or nullptr.
const DotGroup* FindGroup(const std::vector<DotGroup>& groups,
                          const QString& name);
DotGroup* FindGroup(std::vector<DotGroup>* groups, const QString& name);

// The first "Group 1", "Group 2"... that groups do not use.
QString FreshGroupName(const std::vector<DotGroup>& groups);

}  // namespace snapper

#endif  // SNAPPER_MODEL_DOT_GROUP_H_
