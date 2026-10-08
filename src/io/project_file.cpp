#include "io/project_file.h"

#include <QJsonArray>

#include <cassert>

#include "base/text.h"
#include "io/doll_json.h"
#include "io/json_file.h"
#include "io/layer_json.h"
#include "io/reel_json.h"

namespace snapper {
namespace {

// 2 added the reel; 1 files open with an empty one.
constexpr int kProjectVersion = 2;

void ReadDolls(const QJsonArray& dolls, Project* project, JsonIssues* issues) {
  assert(project != nullptr);
  assert(issues != nullptr);
  const bool is_too_many = dolls.size() > kMaxProjectDolls;
  if (is_too_many) {
    issues->Note(Tr("too many dolls"));
    return;
  }
  for (const QJsonValue& item : dolls) {
    const QJsonObject object = item.toObject();
    Doll doll;
    doll.name = object.value("name").toString();
    doll.folder = object.value("folder").toString();
    doll.art = ArtFromJson(object.value("art").toObject(), issues);
    doll.rig = RigFromJson(object.value("rig").toObject(), issues);
    project->dolls[doll.name] = std::make_shared<const Doll>(doll);
  }
}

void ReadShots(const QJsonArray& shots, Project* project, JsonIssues* issues) {
  assert(project != nullptr);
  assert(issues != nullptr);
  const bool is_too_many = shots.size() > kMaxShots;
  if (is_too_many) {
    issues->Note(Tr("too many shots"));
    return;
  }
  for (const QJsonValue& item : shots) {
    project->shots.push_back(std::make_shared<const Shot>(
        ShotFromJson(item.toObject(), issues)));
  }
}

}  // namespace

Result<Project> ReadProject(const QString& path) {
  assert(!path.isEmpty());
  const auto read = ReadJsonFile(path);
  if (!read) {
    return std::unexpected(read.error());
  }
  const int version = read->value("version").toInt();
  const bool is_known =
      read->value("format").toString() == QLatin1String("snapper-project") &&
      version >= 1 && version <= kProjectVersion;
  if (!is_known) {
    return std::unexpected(
        Error{Tr("%1 is not a project this Snapper can open.").arg(path)});
  }
  JsonIssues issues;
  Project project;
  project.name = read->value("name").toString(project.name);
  const QSize canvas = SizeFromJson(read->value("canvas"));
  project.canvas = {canvas.width(), canvas.height()};
  project.song = read->value("song").toString();
  project.next_shot_id = read->value("next_shot_id").toInt(1);
  project.next_layer_id = read->value("next_layer_id").toInt(1);
  project.next_clip_id = read->value("next_clip_id").toInt(1);
  ReadDolls(read->value("dolls").toArray(), &project, &issues);
  ReadShots(read->value("shots").toArray(), &project, &issues);
  project.reel = ReelFromJson(read->value("reel"), &issues);
  const bool is_damaged = issues.HasIssue() || !IsValidCanvas(project.canvas);
  if (is_damaged) {
    const QString why = issues.HasIssue() ? issues.first() : Tr("bad size");
    return std::unexpected(Error{Tr("%1 is damaged: %2").arg(path, why)});
  }
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  return project;
}

Result<void> WriteProject(const QString& path, const Project& project) {
  assert(!path.isEmpty());
  assert(IsValidCanvas(project.canvas));
  QJsonArray dolls;
  for (const auto& [name, doll] : project.dolls) {
    dolls.append(QJsonObject{{"name", name},
                             {"folder", doll->folder},
                             {"art", ArtToJson(doll->art)},
                             {"rig", RigToJson(doll->rig)}});
  }
  QJsonArray shots;
  for (const auto& shot : project.shots) {
    shots.append(ShotToJson(*shot));
  }
  const QJsonObject object{
      {"format", "snapper-project"},
      {"version", kProjectVersion},
      {"name", project.name},
      {"canvas", SizeToJson(QSize(project.canvas.width,
                                  project.canvas.height))},
      {"song", project.song},
      {"next_shot_id", project.next_shot_id},
      {"next_layer_id", project.next_layer_id},
      {"next_clip_id", project.next_clip_id},
      {"dolls", dolls},
      {"shots", shots},
      {"reel", ReelToJson(project.reel)}};
  return WriteJsonFile(path, object);
}

}  // namespace snapper
