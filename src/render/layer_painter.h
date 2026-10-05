#ifndef SNAPPER_RENDER_LAYER_PAINTER_H_
#define SNAPPER_RENDER_LAYER_PAINTER_H_

#include <QPainter>
#include <QTransform>

#include "base/frame.h"
#include "model/layer.h"
#include "model/project.h"
#include "render/image_cache.h"

namespace snapper {

// The layer's own place in shot space at frame, flip included.
QTransform LayerTransform(const Layer& layer, Frame frame);

// Draws one picture layer (doll, image or text) through view, which
// maps shot space to the output. Effect layers are not drawn here: they
// change what is already drawn.
void PaintLayer(const Project& project, const Layer& layer, Frame frame,
                const QTransform& view, ImageCache* cache,
                QPainter* painter);

}  // namespace snapper

#endif  // SNAPPER_RENDER_LAYER_PAINTER_H_
