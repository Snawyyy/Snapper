#ifndef SNAPPER_RENDER_LAYER_PAINTER_H_
#define SNAPPER_RENDER_LAYER_PAINTER_H_

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QTransform>

#include "anim/links.h"
#include "base/frame.h"
#include "model/layer.h"
#include "model/project.h"
#include "render/image_cache.h"

namespace snapper {

// The lettering box of a text layer, centred on its origin, in layer
// space; drawing and clicking both use it.
QRectF TextBox(const TextLayer& text);
QFont TextFont(const TextLayer& text);
// The text's letters as a path, each line centred, in the same space as
// TextBox; PaintLettering draws it with the outline under the fill.
// Text layers and the reel's text clips both draw through these.
QPainterPath TextPath(const TextLayer& text);
void PaintLettering(const QPainterPath& path, const TextLayer& text,
                    QPainter* painter);

// Draws one picture layer (character, image or text) through view,
// which maps shot space to the output, moved by its shot's links (links
// may be nullptr for a layer outside any shot). Effect layers are not
// drawn here: they change what is already drawn.
void PaintLayer(const Project& project, const Layer& layer, Frame frame,
                const QTransform& view, LinkSolver* links, ImageCache* cache,
                QPainter* painter);

}  // namespace snapper

#endif  // SNAPPER_RENDER_LAYER_PAINTER_H_
