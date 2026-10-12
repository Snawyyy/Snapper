#include "render/drawing_colors.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>

namespace snapper {
namespace {

// Distinct colors counted before new ones are ignored, so a photo
// cannot grow the map without end.
constexpr size_t kMaxCounted = 65536;

std::vector<ColorCount> Biggest(const std::unordered_map<QRgb, int>& counts) {
  std::vector<ColorCount> list;
  list.reserve(counts.size());
  for (const auto& [rgb, count] : counts) {
    list.push_back({rgb, count});
  }
  // Ties by color, so the order is the same every time.
  std::sort(list.begin(), list.end(),
            [](const ColorCount& a, const ColorCount& b) {
              return a.count != b.count ? a.count > b.count : a.rgb < b.rgb;
            });
  if (list.size() > static_cast<size_t>(kMaxPaletteColors)) {
    list.resize(static_cast<size_t>(kMaxPaletteColors));
  }
  return list;
}

}  // namespace

std::vector<ColorCount> SolidColors(const QImage& image) {
  if (image.isNull()) {
    return {};
  }
  const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
  std::unordered_map<QRgb, int> counts;
  for (int y = 0; y < argb.height(); ++y) {
    const auto* line = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
    for (int x = 0; x < argb.width(); ++x) {
      const QRgb pixel = line[x];
      if (qAlpha(pixel) != 255) {
        continue;
      }
      const auto found = counts.find(pixel);
      if (found != counts.end()) {
        ++found->second;
      } else if (counts.size() < kMaxCounted) {
        counts.emplace(pixel, 1);
      }
    }
  }
  return Biggest(counts);
}

std::vector<ColorCount> MergeColors(
    const std::vector<std::vector<ColorCount>>& lists) {
  std::unordered_map<QRgb, int> counts;
  for (const auto& list : lists) {
    assert(list.size() <= static_cast<size_t>(kMaxPaletteColors));
    for (const ColorCount& color : list) {
      counts[color.rgb] += color.count;
    }
  }
  return Biggest(counts);
}

}  // namespace snapper
