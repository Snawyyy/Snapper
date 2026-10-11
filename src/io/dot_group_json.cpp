#include "io/dot_group_json.h"

#include <QJsonObject>

#include <algorithm>
#include <cassert>

namespace snapper {
namespace {

GroupDot DotFromJson(const QJsonValue& value) {
  const QJsonObject object = value.toObject();
  const int layer = object.value("layer").toInt();
  // A bad id reads as no layer.
  GroupDot dot;
  dot.layer = LayerId(std::max(layer, 0));
  dot.piece = object.value("piece").toString();
  dot.point = object.value("point").toInt(-1);
  assert(dot.layer.value() >= 0);
  assert(dot.layer.IsValid() == (layer > 0));
  return dot;
}

DotGroup GroupFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  DotGroup group;
  group.name = object.value("name").toString();
  const QJsonArray dots =
      Bounded(object.value("dots"), kMaxGroupDots, "group dots", issues);
  for (const QJsonValue& item : dots) {
    const GroupDot dot = DotFromJson(item);
    const bool is_whole = dot.point >= 0 && !dot.piece.isEmpty();
    const bool is_repeat =
        std::ranges::find(group.dots, dot) != group.dots.end();
    const bool is_new = is_whole && !is_repeat;
    if (is_new) {
      group.dots.push_back(dot);
    }
  }
  assert(group.dots.size() <= static_cast<size_t>(kMaxGroupDots));
  return group;
}

}  // namespace

QJsonArray DotGroupsToJson(const std::vector<DotGroup>& groups) {
  assert(groups.size() <= static_cast<size_t>(kMaxDotGroups));
  QJsonArray array;
  for (const DotGroup& group : groups) {
    QJsonArray dots;
    for (const GroupDot& dot : group.dots) {
      dots.append(QJsonObject{{"layer", dot.layer.value()},
                              {"piece", dot.piece},
                              {"point", dot.point}});
    }
    array.append(QJsonObject{{"name", group.name}, {"dots", dots}});
  }
  assert(array.size() == static_cast<qsizetype>(groups.size()));
  return array;
}

std::vector<DotGroup> DotGroupsFromJson(const QJsonValue& value,
                                        JsonIssues* issues) {
  assert(issues != nullptr);
  const QJsonArray array =
      Bounded(value, kMaxDotGroups, "dot groups", issues);
  std::vector<DotGroup> groups;
  for (const QJsonValue& item : array) {
    DotGroup group = GroupFromJson(item.toObject(), issues);
    const bool is_usable =
        !group.name.isEmpty() && FindGroup(groups, group.name) == nullptr;
    if (is_usable) {
      groups.push_back(std::move(group));
    }
  }
  assert(groups.size() <= static_cast<size_t>(kMaxDotGroups));
  return groups;
}

}  // namespace snapper
