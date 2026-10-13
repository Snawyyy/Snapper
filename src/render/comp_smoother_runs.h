#ifndef SNAPPER_RENDER_COMP_SMOOTHER_RUNS_H_
#define SNAPPER_RENDER_COMP_SMOOTHER_RUNS_H_

#include <QColor>
#include <QRgb>

#include <array>
#include <vector>

namespace snapper {

// The edge search of the Line smoother (render/comp_smoother.h): which
// share of each neighbour's color every pixel takes.

// A picture as the smoother reads it: straight (not premultiplied)
// 8-bit pixels row by row, after the color key, every clear pixel 0.
struct CelGrid final {
  std::vector<QRgb> pixels;
  int width = 0;
  int height = 0;
};

// The share of the pixel above, below, left and right, in that order.
enum class SmootherSide { kUp, kDown, kLeft, kRight };
using SmootherShares = std::vector<std::array<float, 4>>;

struct SmootherEdgeRule final {
  // Colors no further apart than this on every channel are one.
  int range = 0;
  // 0 to 1: how far short steps blend past the plain MLAA share.
  double extra = 0.0;
  // When not empty, only edges with one of these on a side count.
  std::vector<QColor> line_colors;
};

// The MLAA shares of every pixel of cel, one per pixel. A run of an
// edge (pixels of two colors side by side along a row or a column)
// whose ends turn into the next step of a staircase is read as a line
// from the middle of each step to the middle of the run; a pixel takes
// the color across the edge by the area the line cuts from it. An end
// that turns back (a corner, the cap of a line) or into a long side
// adds nothing, so corners stay sharp.
SmootherShares FindSmootherShares(const CelGrid& cel,
                                  const SmootherEdgeRule& rule);

}  // namespace snapper

#endif  // SNAPPER_RENDER_COMP_SMOOTHER_RUNS_H_
