#ifndef SNAPPER_RENDER_COMP_SMOOTHER_H_
#define SNAPPER_RENDER_COMP_SMOOTHER_H_

#include <QColor>
#include <QImage>

#include <vector>

#include "fsp/fsp_effects.h"

namespace snapper {

// The Comp tab's Line smoother: a rebuild of OLM Smoother v2 (OLM
// Digital, OLM OpenTools) from its manual, not its code. Morphological
// antialiasing (MLAA): it finds the stair steps of flat cel art, reads
// each run of a step as part of a straight line through the middles of
// the steps, and gives every pixel the share of the color across the
// edge that the line covers. Flat areas and straight edges are left
// alone, so nothing blurs.

enum class SmootherGamma {
  kOff,
  kAllColors,
  // Only blends with a pixel of one of gamma_colors use the gamma.
  kGammaColors,
};

struct SmootherParams final {
  // How much of the blend is applied, 0 none to 1 all.
  double smoothness = 1.0;
  // 0 to 1: short steps (curves, steep slopes) blend further.
  double extra = 0.0;
  // Colors no further apart than this on every channel (0 to 255) count
  // as one; 255 leaves the picture as it is.
  int range = 0;
  // Pixels of key's color turn clear before smoothing.
  bool uses_key = false;
  QColor key = Qt::white;
  // v2: blends in linear light (sRGB in and out); v1 in sRGB values.
  bool is_linear = true;
  SmootherGamma gamma = SmootherGamma::kOff;
  // Applied after linearising, before and after the blend; 1 is none.
  double gamma_value = 2.4;
  std::vector<QColor> gamma_colors;
  // When not empty, only steps with one of these colors on a side
  // change (the effect's picked target colors).
  std::vector<QColor> line_colors;

  bool operator==(const SmootherParams&) const = default;
};

// The params of the smoother's values, slot by slot in the order of
// OlmSmootherInfo (fsp/olm_smoother_info.h); line_colors is left empty.
SmootherParams SmootherParamsOf(const FspValues& values);

// frame (premultiplied ARGB32) smoothed, the same size and format. The
// work is in frame's own pixels: it reads stair steps one pixel high
// whatever scale the frame was drawn at.
QImage SmoothCel(const QImage& frame, const SmootherParams& params);

}  // namespace snapper

#endif  // SNAPPER_RENDER_COMP_SMOOTHER_H_
