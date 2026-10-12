#ifndef SNAPPER_RENDER_DRAWING_COLORS_H_
#define SNAPPER_RENDER_DRAWING_COLORS_H_

#include <QImage>
#include <QRgb>

#include <vector>

namespace snapper {

// The most colors a palette list shows for one drawing or doll.
constexpr int kMaxPaletteColors = 64;

// One color of a drawing and how many pixels have it.
struct ColorCount final {
  QRgb rgb = 0;
  int count = 0;

  bool operator==(const ColorCount&) const = default;
};

// The fully solid colors of image (alpha 255, so soft edges add no
// near-duplicates), most pixels first, at most kMaxPaletteColors.
std::vector<ColorCount> SolidColors(const QImage& image);

// Several drawings' colors as one list: counts of the same color add
// up, most pixels first, at most kMaxPaletteColors.
std::vector<ColorCount> MergeColors(
    const std::vector<std::vector<ColorCount>>& lists);

}  // namespace snapper

#endif  // SNAPPER_RENDER_DRAWING_COLORS_H_
