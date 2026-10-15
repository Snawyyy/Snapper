#ifndef SNAPPER_RENDER_LAYER_PAINTER_H_
#define SNAPPER_RENDER_LAYER_PAINTER_H_

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QTransform>

#include <map>
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
// The text's letters as a path, each line centred, in the same space as
// TextBox; PaintLettering draws it with the outline under the fill.
// Text layers and the reel's text clips both draw through these.
QPainterPath TextPath(const TextLayer& text);
void PaintLettering(const QPainterPath& path, const TextLayer& text,
                    QPainter* painter);

// A layer showing doll at rest, unkeyed, for canvases that draw a doll
// outside any shot (the rig canvas, draw mode).
Layer RestLayer(const QString& doll);
// The same, each piece showing the drawing drawings gives it (by piece
// name; -1 its default), still at rest.
Layer RestLayer(const QString& doll, const std::map<QString, int>& drawings);

// What a doll drawn for some of its pieces does with the others. Pieces
// below the lowest named one are never drawn: nothing named is under
// them to cover.
enum class PieceSplit {
  // The others keep their place, unseen.
  kOnly,
  // The others above erase what they cover: the named pieces as they
  // show in the doll.
  kShown,
  // The others above are drawn and the named ones erase what they
  // cover: what stands in front of the named pieces.
  kCover,
};

// Draws one picture layer (doll, image or text) through view, which
// maps shot space to the output, moved by its shot's links (links may
// be nullptr for a layer outside any shot). Effect layers are not drawn
// here: they change what is already drawn. With only_pieces, a doll
// draws just the pieces so named, the others treated as split says.
void PaintLayer(const Project& project, const Layer& layer, Frame frame,
                const QTransform& view, LinkSolver* links, ImageCache* cache,
                QPainter* painter,
                const std::set<QString>* only_pieces = nullptr,
                PieceSplit split = PieceSplit::kOnly);

}  // namespace snapper

#endif  // SNAPPER_RENDER_LAYER_PAINTER_H_
