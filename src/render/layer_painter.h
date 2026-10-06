#ifndef SNAPPER_RENDER_LAYER_PAINTER_H_
#define SNAPPER_RENDER_LAYER_PAINTER_H_

#include <QFont>
#include <QPainter>
#include <QRectF>
#include <QTransform>

#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/layer.h"
#include "model/project.h"
#include "render/image_cache.h"

namespace snapper {

// The lettering box of a text layer, centred on its origin, in layer
// space; drawing and clicking both use it.
QRectF TextBox(const TextLayer& text);
QFont TextFont(const TextLayer& text);

// Draws one picture layer (doll, image or text) through view, which
// maps shot space to the output. Effect layers are not drawn here: they
// change what is already drawn.
void PaintLayer(const Project& project, const Layer& layer, Frame frame,
                const QTransform& view, ImageCache* cache,
                QPainter* painter);

}  // namespace snapper

#endif  // SNAPPER_RENDER_LAYER_PAINTER_H_
