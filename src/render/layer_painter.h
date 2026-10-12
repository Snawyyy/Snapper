#ifndef SNAPPER_RENDER_LAYER_PAINTER_H_
#define SNAPPER_RENDER_LAYER_PAINTER_H_

#include <QFont>
#include <QPainter>
#include <QRectF>
#include <QTransform>

#include <set>

#include "anim/doll_pose.h"
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

// A layer showing doll at rest, unkeyed, for canvases that draw a doll
// outside any shot (the rig canvas, draw mode).
Layer RestLayer(const QString& doll);

// Draws one picture layer (doll, image or text) through view, which
// maps shot space to the output, moved by its shot's links (links may
// be nullptr for a layer outside any shot). Effect layers are not drawn
// here: they change what is already drawn. With only_pieces, a doll
// draws just the pieces so named (the others keep their place, unseen).
void PaintLayer(const Project& project, const Layer& layer, Frame frame,
                const QTransform& view, LinkSolver* links, ImageCache* cache,
                QPainter* painter,
                const std::set<QString>* only_pieces = nullptr);

}  // namespace snapper

#endif  // SNAPPER_RENDER_LAYER_PAINTER_H_
