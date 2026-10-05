#include "io/pose_file.h"

#include <QFileInfo>
#include <QJsonArray>

#include <cassert>

#include "base/text.h"
#include "io/json_file.h"
#include "io/json_values.h"
#include "model/layer.h"

namespace snapper {
namespace {

constexpr int kPoseVersion = 1;

SavedPose PoseFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  SavedPose pose;
  pose.name = object.value("name").toString();
  pose.doll = object.value("doll").toString();
  const QJsonObject pieces = object.value("pieces").toObject();
  const bool is_too_many = pieces.size() > kMaxPieceTracks;
  if (is_too_many) {
    issues->Note(Tr("a pose has too many pieces"));
    return pose;
  }
  for (auto it = pieces.begin(); it != pieces.end(); ++it) {
    pose.pieces[it.key()] = ValueFromJson(it.value(), PiecePose(), issues);
  }
  assert(pose.pieces.size() <= static_cast<size_t>(kMaxPieceTracks));
  return pose;
}

}  // namespace

Result<std::vector<SavedPose>> ReadPoses(const QString& path) {
  assert(!path.isEmpty());
  std::vector<SavedPose> poses;
  const bool is_first_use = !QFileInfo::exists(path);
  if (is_first_use) {
    return poses;
  }
  const auto read = ReadJsonFile(path);
  if (!read) {
    return std::unexpected(read.error());
  }
  const QJsonArray list = read->value("poses").toArray();
  const bool is_known =
      read->value("format").toString() == QLatin1String("snapper-poses") &&
      read->value("version").toInt() == kPoseVersion &&
      list.size() <= kMaxSavedPoses;
  if (!is_known) {
    return std::unexpected(Error{Tr("%1 is not a pose file.").arg(path)});
  }
  JsonIssues issues;
  for (const QJsonValue& item : list) {
    poses.push_back(PoseFromJson(item.toObject(), &issues));
  }
  const bool is_damaged = issues.HasIssue();
  if (is_damaged) {
    return std::unexpected(Error{Tr("%1: %2").arg(path, issues.first())});
  }
  assert(poses.size() <= static_cast<size_t>(kMaxSavedPoses));
  return poses;
}

Result<void> WritePoses(const QString& path,
                        const std::vector<SavedPose>& poses) {
  assert(!path.isEmpty());
  assert(poses.size() <= static_cast<size_t>(kMaxSavedPoses));
  QJsonArray list;
  for (const SavedPose& pose : poses) {
    QJsonObject pieces;
    for (const auto& [name, piece] : pose.pieces) {
      pieces.insert(name, ValueToJson(piece));
    }
    list.append(QJsonObject{
        {"name", pose.name}, {"doll", pose.doll}, {"pieces", pieces}});
  }
  return WriteJsonFile(path, QJsonObject{{"format", "snapper-poses"},
                                         {"version", kPoseVersion},
                                         {"poses", list}});
}

}  // namespace snapper
