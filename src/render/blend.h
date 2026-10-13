#ifndef SNAPPER_RENDER_BLEND_H_
#define SNAPPER_RENDER_BLEND_H_

#include <QImage>

#include <cstdint>
#include <vector>

#include "model/comp.h"

namespace snapper {

// The one blend function: lays top over frame (both premultiplied ARGB32,
// the same size) in mode at opacity (0 to 1), so a mode looks the same
// wherever it is used. mask, when not empty, holds one coverage byte
// per pixel (row by row) that scales top. Follows the W3C compositing
// formulas: over a see-through area of frame every mode acts as Normal,
// so exports with alpha stay clean.
void BlendOnto(const QImage& top, BlendMode mode, double opacity,
               const std::vector<std::uint8_t>& mask, QImage* frame);
// For a picture that stands in for frame (frame redrawn, as the Line
// smoother gives it): in Normal, frame becomes picture by opacity times
// mask, alpha too, so it can thin an edge or clear a keyed color, not
// only add; other modes blend through BlendOnto only the pixels picture
// changed.
void StandInOnto(const QImage& picture, BlendMode mode, double opacity,
                 const std::vector<std::uint8_t>& mask, QImage* frame);

}  // namespace snapper

#endif  // SNAPPER_RENDER_BLEND_H_
