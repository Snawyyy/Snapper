#include "io/project_file.h"

#include <QFileInfo>
#include <QJsonArray>

#include <algorithm>
#include <cassert>

#include "base/text.h"
#include "io/character_json.h"
#include "io/doll_file.h"
#include "io/doll_json.h"
#include "io/json_file.h"
#include "io/layer_json.h"
#include "io/reel_json.h"
#include "io/storyboard_paths.h"

namespace snapper {
namespace {

// 3 moved transitions from shots to the reel's cuts; 2 added the reel;
// 1 files open with an empty one.
constexpr int kProjectVersion = 3;

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
    AddOwnDrawings(&doll);
    project->dolls[doll.name] = std::make_shared<const Doll>(doll);
  }
}

void ReadCharacters(const QJsonArray& characters, Project* project,
                    JsonIssues* issues) {
  assert(project != nullptr);
  assert(issues != nullptr);
  const bool is_too_many = characters.size() > kMaxProjectCharacters;
  if (is_too_many) {
    issues->Note(Tr("too many characters"));
    return;
  }
  for (const QJsonValue& item : characters) {
    const QJsonObject object = item.toObject();
    Character character = CharacterFromJson(object, issues);
    character.folder = object.value("folder").toString();
    if (!character.name.isEmpty()) {
      project->characters[character.name] =
          std::make_shared<const Character>(std::move(character));
    }
  }
}

// Shots grown at the front before 2026-10-14 pushed every layer's start
// back with the keys, leaving the new first frames blank. A shot whose
// layers all start on one frame past 0 is that: they start at 0 again,
// and a layer shown for part of the shot keeps its end.
// ponytail: a shot whose every layer was meant to start late is
// changed too; rare, and the layer's start drag puts it back.
Shot WithFrontRepaired(Shot shot) {
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  assert(shot.length.index() >= 0);
  const bool is_empty = shot.layers.empty();
  if (is_empty) {
    return shot;
  }
  const Frame first = shot.layers.front().start;
  const bool is_shifted =
      first.index() > 0 &&
      std::ranges::all_of(shot.layers, [first](const Layer& layer) {
        return layer.start == first;
      });
  if (!is_shifted) {
    return shot;
  }
  for (Layer& layer : shot.layers) {
    const bool has_span = layer.length.index() > 0;
    layer.length = has_span ? Frame(layer.length.index() + first.index())
                            : layer.length;
    layer.start = Frame(0);
  }
  return shot;
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
        WithFrontRepaired(ShotFromJson(item.toObject(), issues))));
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
  project.new_shots_transparent =
      read->value("new_shots_transparent").toBool(false);
  ReadDolls(read->value("dolls").toArray(), &project, &issues);
  ReadCharacters(read->value("characters").toArray(), &project, &issues);
  ReadShots(read->value("shots").toArray(), &project, &issues);
  project.reel = ReelFromJson(read->value("reel"), &issues);
  MoveShotTransitions(read->value("shots").toArray(), &project.reel,
                      &issues);
  project.reel.markers = MarkersFromJson(read->value("cut_markers"), &issues);
  ReelTextFromJson(read->value("reel_text"), &project.reel, &issues);
  const bool is_damaged = issues.HasIssue() || !IsValidCanvas(project.canvas);
  if (is_damaged) {
    const QString why = issues.HasIssue() ? issues.first() : Tr("bad size");
    return std::unexpected(Error{Tr("%1 is damaged: %2").arg(path, why)});
  }
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  // Frame pictures are named from the file's folder in the file.
  return ResolvedStoryboardPaths(std::move(project),
                                 QFileInfo(path).absolutePath());
}

Result<void> WriteProject(const QString& path, const Project& in_memory) {
  assert(!path.isEmpty());
  assert(IsValidCanvas(in_memory.canvas));
  const Project project = RelativeStoryboardPaths(
      in_memory, QFileInfo(path).absolutePath());
  QJsonArray dolls;
  for (const auto& [name, doll] : project.dolls) {
    dolls.append(QJsonObject{{"name", name},
                             {"folder", doll->folder},
                             {"art", ArtToJson(doll->art)},
                             {"rig", RigToJson(doll->rig)}});
  }
  QJsonArray characters;
  for (const auto& [name, character] : project.characters) {
    QJsonObject json = CharacterToJson(*character);
    json.insert("folder", character->folder);
    characters.append(json);
  }
  QJsonArray shots;
  for (const auto& shot : project.shots) {
    shots.append(ShotToJson(*shot));
  }
  QJsonObject object{
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
      {"characters", characters},
      {"shots", shots},
      {"reel", ReelToJson(project.reel)},
      {"cut_markers", MarkersToJson(project.reel.markers)},
      {"reel_text", ReelTextToJson(project.reel)}};
  const bool starts_transparent = project.new_shots_transparent;
  if (starts_transparent) {
    object.insert("new_shots_transparent", true);
  }
  return WriteJsonFile(path, object);
}

}  // namespace snapper
