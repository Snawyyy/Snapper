#include "edit/achievements.h"

#include <QSet>

#include <algorithm>
#include <cassert>
#include <variant>

#include "anim/master_timeline.h"

namespace snapper {
namespace {

int& At(AchievementStats* stats, AchievementStat stat) {
  assert(stats != nullptr);
  return (*stats)[static_cast<size_t>(stat)];
}

void Raise(AchievementStats* stats, AchievementStat stat, int value) {
  int& now = At(stats, stat);
  now = std::max(now, value);
}

void MeasureCharacter(const Character& character, AchievementStats* stats) {
  assert(stats != nullptr);
  QSet<QRgb> colors;
  QSet<int> brushes;
  int shapes = 0;
  int bones = 0;
  int smart = 0;
  for (const CharacterPart& part : character.parts) {
    At(stats, AchievementStat::kStrokes) +=
        static_cast<int>(part.drawing.curves.size());
    for (const VectorShape& shape : part.drawing.shapes) {
      ++shapes;
      At(stats, AchievementStat::kFills) += shape.has_fill ? 1 : 0;
      if (shape.has_fill) {
        colors.insert(shape.fill.rgb());
      }
      if (shape.has_stroke) {
        colors.insert(shape.stroke.rgb());
      }
      brushes.insert(static_cast<int>(shape.brush));
    }
    bones += static_cast<int>(part.bones.size());
    smart += static_cast<int>(std::count_if(
        part.bones.begin(), part.bones.end(),
        [](const Bone& bone) { return bone.is_smart; }));
    const bool is_switch = part.kind == PartKind::kSwitch;
    if (is_switch) {
      const auto children = std::count_if(
          character.parts.begin(), character.parts.end(),
          [&part](const CharacterPart& child) {
            return child.parent == part.id;
          });
      Raise(stats, AchievementStat::kSwitchDrawings,
            static_cast<int>(children));
    }
  }
  Raise(stats, AchievementStat::kColors, static_cast<int>(colors.size()));
  Raise(stats, AchievementStat::kStyles,
        static_cast<int>(character.styles.size()));
  Raise(stats, AchievementStat::kBrushes, static_cast<int>(brushes.size()));
  Raise(stats, AchievementStat::kShapes, shapes);
  Raise(stats, AchievementStat::kBones, bones);
  Raise(stats, AchievementStat::kSmartBones, smart);
}

void MeasureShot(const Shot& shot, AchievementStats* stats) {
  assert(stats != nullptr);
  bool is_done = false;
  for (const Layer& layer : shot.layers) {
    const int keys = static_cast<int>(layer.transform.keys.size());
    At(stats, AchievementStat::kKeys) += keys;
    Raise(stats, AchievementStat::kKeysOnLayer, keys);
    At(stats, AchievementStat::kEasedKeys) += static_cast<int>(
        std::count_if(layer.transform.keys.begin(),
                      layer.transform.keys.end(),
                      [](const auto& key) { return key.ease != Ease::kStep; }));
    is_done = is_done || keys >= 2;
  }
  At(stats, AchievementStat::kShotsDone) += is_done ? 1 : 0;
}

}  // namespace

int StatOf(const AchievementStats& stats, AchievementStat stat) {
  assert(static_cast<int>(stat) < kAchievementStatCount);
  return stats[static_cast<size_t>(stat)];
}

bool IsEarned(const Achievement& achievement, const AchievementStats& stats) {
  assert(achievement.goal > 0);
  return StatOf(stats, achievement.stat) >= achievement.goal;
}

AchievementStats Measure(const Project& project) {
  AchievementStats stats{};
  for (const auto& [name, character] : project.characters) {
    assert(character != nullptr);
    MeasureCharacter(*character, &stats);
  }
  At(&stats, AchievementStat::kCharacters) =
      static_cast<int>(project.characters.size());
  for (const auto& shot : project.shots) {
    MeasureShot(*shot, &stats);
  }
  At(&stats, AchievementStat::kShots) = static_cast<int>(project.shots.size());
  At(&stats, AchievementStat::kReelSeconds) =
      TotalLength(project).index() / kFramesPerSecond;
  int cues = 0;
  for (const ReelTrack& track : project.reel.tracks) {
    cues += static_cast<int>(std::count_if(
        track.clips.begin(), track.clips.end(), [](const Clip& clip) {
          return std::holds_alternative<TextSource>(clip.source);
        }));
  }
  At(&stats, AchievementStat::kCues) = cues;
  return stats;
}

bool IsTallied(AchievementStat stat) {
  return stat == AchievementStat::kStrokes ||
         stat == AchievementStat::kFills || stat == AchievementStat::kKeys;
}

}  // namespace snapper
