#ifndef SNAPPER_RENDER_CHARACTER_PAINT_H_
#define SNAPPER_RENDER_CHARACTER_PAINT_H_

#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QTransform>

#include <optional>

#include "anim/character_pose.h"
#include "model/character.h"
#include "render/image_cache.h"

namespace snapper {

// Draws posed (character space) through world: each shown part back to
// front, vector shapes filled then stroked, pictures from the
// character's folder. painter's opacity is multiplied in.
void PaintCharacter(const Character& character, const PosedCharacter& posed,
                    const QTransform& world, ImageCache* cache,
                    QPainter* painter);

// The outline a click on part can hit, in character space: its shapes'
// fills and strokes, or a picture's box.
QPainterPath PartOutline(const Character& character, const PosedPart& part);

// The front-most shown part under at (character space), or none.
std::optional<PartId> HitPart(const Character& character,
                              const PosedCharacter& posed, QPointF at);

}  // namespace snapper

#endif  // SNAPPER_RENDER_CHARACTER_PAINT_H_
