#ifndef SNAPPER_RENDER_EFFECTS_H_
#define SNAPPER_RENDER_EFFECTS_H_

#include <QColor>
#include <QImage>

#include "model/layer.h"

namespace snapper {

// Changes frame in place, like an adjustment layer over everything
// drawn so far. amount is 0 (no change) to 1 (full). scale is how big
// the frame is against the project canvas, so a preview at half size
// looks like the export. seed varies glitches frame to frame and is the
// same every time that frame renders.
void ApplyEffect(EffectKind kind, double amount, QColor color, double scale,
                 int seed, QImage* frame);

}  // namespace snapper

#endif  // SNAPPER_RENDER_EFFECTS_H_
