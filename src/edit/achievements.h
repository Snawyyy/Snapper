#ifndef SNAPPER_EDIT_ACHIEVEMENTS_H_
#define SNAPPER_EDIT_ACHIEVEMENTS_H_

#include <array>

#include "model/project.h"

namespace snapper {

// The four shelves of the trophy case, as Terraria sorts its own.
enum class AchievementGroup { kArtist, kRigger, kDirector, kGrinder };
inline constexpr int kAchievementGroupCount = 4;

// What an achievement counts. Some are read off the project as it is
// (the most bones in one character); some add up as you work (strokes
// drawn, undos); some come from what the app sees (exports, the days in
// a row worked).
enum class AchievementStat {
  kStrokes,
  kColors,
  kStyles,
  kBrushes,
  kFills,
  kCharacters,
  kBones,
  kSmartBones,
  kSwitchDrawings,
  kShapes,
  kKeys,
  kKeysOnLayer,
  kEasedKeys,
  kCues,
  kLightTable,
  kShotsDone,
  kShots,
  kReelSeconds,
  kExports,
  kStreak,
  kNightOwl,
  kUndos,
  kNotesDismissed,
  kToolsUsed,
};
inline constexpr int kAchievementStatCount = 24;

// One trophy: its shelf, name, how to earn it, a joke, and the stat and
// goal it needs.
struct Achievement final {
  const char* id;
  AchievementGroup group;
  const char* name;
  const char* how;
  const char* joke;
  AchievementStat stat;
  int goal;
};

inline constexpr Achievement kAchievements[] = {
    {"timber", AchievementGroup::kArtist, "Timber!!",
     "Draw your first stroke.", "Every masterpiece starts with a scratch.",
     AchievementStat::kStrokes, 1},
    {"hand_cramp", AchievementGroup::kArtist, "Hand Cramp",
     "Draw 1,000 strokes.", "Your wrist would like a word.",
     AchievementStat::kStrokes, 1000},
    {"ink_addiction", AchievementGroup::kArtist, "Ink Addiction",
     "Draw 10,000 strokes.", "Seek help. Or more ink.",
     AchievementStat::kStrokes, 10000},
    {"rainbow", AchievementGroup::kArtist, "Taste the Rainbow",
     "Use 10 colors in one character.", "Subtle, it is not.",
     AchievementStat::kColors, 10},
    {"fashion_police", AchievementGroup::kArtist, "Fashion Police",
     "Make 5 named styles in one character.", "Dressed to impress.",
     AchievementStat::kStyles, 5},
    {"brush_hoarder", AchievementGroup::kArtist, "Brush Hoarder",
     "Use every line brush.", "Gotta draw 'em all.",
     AchievementStat::kBrushes, 6},
    {"bucket_list", AchievementGroup::kArtist, "Bucket List",
     "Fill 100 shapes.", "Pour decisions.", AchievementStat::kFills, 100},
    {"its_alive", AchievementGroup::kRigger, "It's Alive!",
     "Make your first character.", "Mwahaha.",
     AchievementStat::kCharacters, 1},
    {"bone_zone", AchievementGroup::kRigger, "Bone Zone",
     "Give one character 10 bones.", "Spooky, scary.",
     AchievementStat::kBones, 10},
    {"big_brain", AchievementGroup::kRigger, "Big Brain Bone",
     "Make your first smart bone.", "Think, bone, think!",
     AchievementStat::kSmartBones, 1},
    {"costume_party", AchievementGroup::kRigger, "Costume Party",
     "Put 3 drawings in one switch.", "Who are you today?",
     AchievementStat::kSwitchDrawings, 3},
    {"full_house", AchievementGroup::kRigger, "Full House",
     "Have 5 characters in one project.", "Crowded in here.",
     AchievementStat::kCharacters, 5},
    {"shape_shifter", AchievementGroup::kRigger, "Shape Shifter",
     "Give one character 50 shapes.", "So many pieces.",
     AchievementStat::kShapes, 50},
    {"lights_camera", AchievementGroup::kDirector, "Lights, Camera",
     "Set your first key.", "The first frame of fame.",
     AchievementStat::kKeys, 1},
    {"key_master", AchievementGroup::kDirector, "Key Master",
     "Set 12 keys on one layer.", "Who needs a locksmith.",
     AchievementStat::kKeysOnLayer, 12},
    {"ease_up", AchievementGroup::kDirector, "Ease Up",
     "Ease a key.", "Smooooth.", AchievementStat::kEasedKeys, 1},
    {"mouth_off", AchievementGroup::kDirector, "Mouth Off",
     "Time a line of words to the song.", "Say it, don't spray it.",
     AchievementStat::kCues, 1},
    {"seeing_ghosts", AchievementGroup::kDirector, "Seeing Ghosts",
     "Light up the light table.", "Boo, it's last frame.",
     AchievementStat::kLightTable, 1},
    {"wrap", AchievementGroup::kDirector, "That's a Wrap",
     "Finish a shot: a layer with two keys or more.", "Print it!",
     AchievementStat::kShotsDone, 1},
    {"box_office", AchievementGroup::kDirector, "Box Office",
     "Have 10 shots in one reel.", "Sequel when?",
     AchievementStat::kShots, 10},
    {"feature_length", AchievementGroup::kDirector, "Feature Length",
     "Make a film over 60 seconds.", "Grab popcorn.",
     AchievementStat::kReelSeconds, 61},
    {"fresh_press", AchievementGroup::kDirector, "Fresh Off the Press",
     "Burn your first video.", "Still warm.",
     AchievementStat::kExports, 1},
    {"collectors", AchievementGroup::kDirector, "Collector's Edition",
     "Burn 10 videos.", "Shelf space required.",
     AchievementStat::kExports, 10},
    {"not_a_phase", AchievementGroup::kGrinder, "Not a Phase",
     "Work 3 days in a row.", "It's who you are now.",
     AchievementStat::kStreak, 3},
    {"dedicated", AchievementGroup::kGrinder, "Dedicated",
     "Work 7 days in a row.", "A whole week of frames.",
     AchievementStat::kStreak, 7},
    {"no_life", AchievementGroup::kGrinder, "No Life",
     "Work 30 days in a row.", "Touch grass. Later.",
     AchievementStat::kStreak, 30},
    {"night_owl", AchievementGroup::kGrinder, "Creature of the Night",
     "Work past midnight.", "Sleep is for the weak.",
     AchievementStat::kNightOwl, 1},
    {"ctrl_z", AchievementGroup::kGrinder, "Ctrl+Z Warrior",
     "Undo 500 times.", "Mistakes were made.", AchievementStat::kUndos,
     500},
    {"clean_freak", AchievementGroup::kGrinder, "Clean Freak",
     "Dismiss 25 sticky notes.", "Ahh, tidy.",
     AchievementStat::kNotesDismissed, 25},
    {"tool_time", AchievementGroup::kGrinder, "Tool Time",
     "Use every Character tab tool.", "Jack of all trades.",
     AchievementStat::kToolsUsed, 14},
};
inline constexpr int kAchievementCount =
    static_cast<int>(std::size(kAchievements));

// Every stat's value, in AchievementStat order.
using AchievementStats = std::array<int, kAchievementStatCount>;

int StatOf(const AchievementStats& stats, AchievementStat stat);
bool IsEarned(const Achievement& achievement, const AchievementStats& stats);

// What the project shows as it is: for each stat read off a project,
// its value (the most in any one character or layer, or the count over
// the project); 0 for the stats the project cannot tell.
AchievementStats Measure(const Project& project);

// The stats that add up as you work (strokes, fills, keys): a step
// that raises one counts the rise; one that lowers it counts nothing.
bool IsTallied(AchievementStat stat);

}  // namespace snapper

#endif  // SNAPPER_EDIT_ACHIEVEMENTS_H_
