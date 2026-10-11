#include "ui/kind_tile.h"

#include <cassert>

#include "ui/material_art.h"

namespace snapper {

KindTile::KindTile(Material material, MotionManager* motion, QWidget* parent)
    : LoopTile(MaterialName(material), motion, parent), material_(material) {
  assert(static_cast<int>(material) >= 0);
  assert(static_cast<int>(material) < kMaterialCount);
}

void KindTile::PaintArt(QPainter* painter, const QRectF& art, double phase,
                        const QColor& ink) const {
  assert(painter != nullptr);
  assert(phase >= 0.0 && phase <= 1.0);
  PaintMaterial(painter, art, material_, phase, ink);
}

}  // namespace snapper
