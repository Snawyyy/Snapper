#ifndef SNAPPER_UI_KIND_TILE_H_
#define SNAPPER_UI_KIND_TILE_H_

#include "ui/dot_kinds.h"
#include "ui/loop_tile.h"

namespace snapper {

// One material tile: a tiny cartoon of the material (Still, Drag, Cloth
// or Jelly) over its name.
class KindTile final : public LoopTile {
  Q_OBJECT

 public:
  KindTile(Material material, MotionManager* motion, QWidget* parent);

  Material material() const { return material_; }

 protected:
  void PaintArt(QPainter* painter, const QRectF& art, double phase,
                const QColor& ink) const override;

 private:
  Material material_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_KIND_TILE_H_
