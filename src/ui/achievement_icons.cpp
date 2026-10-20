#include "ui/achievement_icons.h"

#include <QPainter>

#include <array>
#include <cassert>

#include "edit/achievements.h"

namespace snapper {
namespace {

constexpr int kGrid = 12;

// Each shelf's sprite on a 12 by 12 grid: '.' is empty, the letters are
// colours from the palette below.
using Sprite = std::array<const char*, kGrid>;

constexpr Sprite kBrush = {
    "..........KK", ".........KWK", "........KWK.", ".......KWK..",
    "......KWK...", ".....KWK....", "....RRK.....", "...RRRR.....",
    "..RRRR......", ".RRRR.......", "RRR.........", "R..........."};
constexpr Sprite kBone = {
    ".WW.........", "WWWW........", "WWWWW.......", ".WWWWW......",
    "...WWWW.....", "....WWWW....", ".....WWWW...", "......WWWW..",
    ".......WWWWW", "........WWWW", "........WWWW", ".........WW."};
constexpr Sprite kClapper = {
    "KWKWKWKWKWK.", "WKWKWKWKWKW.", "............", "KKKKKKKKKKK.",
    "KRRRRRRRRRK.", "KRWWWWWWWRK.", "KRRRRRRRRRK.", "KRWWWWRRRRK.",
    "KRRRRRRRRRK.", "KRWWWWWWRRK.", "KRRRRRRRRRK.", "KKKKKKKKKKK."};
constexpr Sprite kHourglass = {
    "KKKKKKKKKKKK", ".K........K.", ".KYYYYYYYYK.", "..KYYYYYYK..",
    "...KYYYYK...", "....KYYK....", "....K..K....", "...K.YY.K...",
    "..K..YY..K..", ".K.YYYYYY.K.", ".KYYYYYYYYK.", "KKKKKKKKKKKK"};

const Sprite& SpriteOf(AchievementGroup group) {
  switch (group) {
    case AchievementGroup::kRigger:
      return kBone;
    case AchievementGroup::kDirector:
      return kClapper;
    case AchievementGroup::kGrinder:
      return kHourglass;
    case AchievementGroup::kArtist:
      break;
  }
  return kBrush;
}

QColor ColorOf(char code) {
  switch (code) {
    case 'K':
      return {0x2a, 0x22, 0x20};
    case 'W':
      return {0xf4, 0xf1, 0xea};
    case 'R':
      return {0xd8, 0x30, 0x4f};
    case 'Y':
      return {0xf2, 0xc2, 0x30};
    default:
      break;
  }
  return Qt::transparent;
}

// Bronze for an easy goal, silver for a middling one, gold for a hard
// one: where it sits among its stat's goals.
QColor TierOf(int index) {
  assert(index >= 0 && index < kAchievementCount);
  const Achievement& trophy = kAchievements[index];
  int easier = 0;
  for (const Achievement& other : kAchievements) {
    const bool is_easier = other.stat == trophy.stat && other.goal < trophy.goal;
    easier += is_easier ? 1 : 0;
  }
  const bool is_gold = easier >= 2 || trophy.goal >= 100;
  const bool is_silver = !is_gold && (easier >= 1 || trophy.goal >= 5);
  return is_gold     ? QColor(0xf2, 0xc2, 0x30)
         : is_silver ? QColor(0xc8, 0xcc, 0xd4)
                     : QColor(0xc4, 0x6a, 0x3a);
}

}  // namespace

QPixmap AchievementIcon(int index, bool is_earned, int side) {
  assert(index >= 0 && index < kAchievementCount);
  assert(side >= kGrid);
  QPixmap pixmap(side, side);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  // The tile, and its frame in the trophy's tier.
  const QColor tier = is_earned ? TierOf(index) : QColor(0x4a, 0x42, 0x40);
  painter.fillRect(pixmap.rect(), tier);
  const int edge = side / 10 + 1;
  painter.fillRect(pixmap.rect().adjusted(edge, edge, -edge, -edge),
                   is_earned ? QColor(0x2e, 0x2a, 0x29) : QColor(0x1c, 0x18,
                                                                 0x18));
  // The sprite, in whole pixels so it stays crisp.
  const int cell = (side - 4 * edge) / kGrid;
  const int left = (side - cell * kGrid) / 2;
  const Sprite& sprite = SpriteOf(kAchievements[index].group);
  for (int y = 0; y < kGrid; ++y) {
    for (int x = 0; x < kGrid; ++x) {
      const char code = sprite[static_cast<size_t>(y)][x];
      if (code == '.') {
        continue;
      }
      const QColor ink = is_earned ? ColorOf(code) : QColor(0x0c, 0x09, 0x0a);
      painter.fillRect(left + x * cell, left + y * cell, cell, cell, ink);
    }
  }
  return pixmap;
}

}  // namespace snapper
