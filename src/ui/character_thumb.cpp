#include "ui/character_thumb.h"

#include <QPainter>

#include <algorithm>
#include <cassert>

#include "anim/character_pose.h"
#include "render/character_paint.h"
#include "render/image_cache.h"

namespace snapper {

QPixmap CharacterPicture(const Character& character, int side) {
  assert(side > 0);
  assert(IsSound(character));
  QPixmap pixmap(side, side);
  pixmap.fill(Qt::transparent);
  const PosedCharacter posed = PoseCharacter(character, CharacterPose{});
  QRectF box;
  for (const PosedPart& part : posed.parts) {
    box = box.united(PartOutline(character, part).boundingRect());
  }
  if (box.isEmpty()) {
    return pixmap;
  }
  const double margin = side * 0.08;
  const double scale = std::min((side - 2.0 * margin) / box.width(),
                                (side - 2.0 * margin) / box.height());
  QTransform view;
  view.translate(side / 2.0, side / 2.0);
  view.scale(scale, scale);
  view.translate(-box.center().x(), -box.center().y());
  QPainter painter(&pixmap);
  ImageCache cache;
  PaintCharacter(character, posed, view, &cache, &painter);
  return pixmap;
}

}  // namespace snapper
