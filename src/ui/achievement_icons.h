#ifndef SNAPPER_UI_ACHIEVEMENT_ICONS_H_
#define SNAPPER_UI_ACHIEVEMENT_ICONS_H_

#include <QPixmap>

namespace snapper {

// A trophy's pixel-art badge, side pixels square, in the Terraria way:
// its shelf's sprite (a brush, a bone, a clapperboard, an hourglass) on
// a tile framed bronze, silver or gold by how hard it is. A trophy not
// yet earned is a dark silhouette.
QPixmap AchievementIcon(int index, bool is_earned, int side);

}  // namespace snapper

#endif  // SNAPPER_UI_ACHIEVEMENT_ICONS_H_
