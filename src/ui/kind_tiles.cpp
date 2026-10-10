#include "ui/kind_tiles.h"

#include <cassert>
#include <memory>

#include "ui/form_helpers.h"

namespace snapper {
namespace {

const char* const kNames[kMaterialCount] = {"kind_still", "kind_drag",
                                            "kind_cloth", "kind_jelly"};

QString HelpOf(Material material) {
  assert(static_cast<int>(material) < kMaterialCount);
  assert(static_cast<int>(material) >= 0);
  switch (material) {
    case Material::kStill:
      return QObject::tr("Still: the dot does not move by itself.");
    case Material::kDrag:
      return QObject::tr("Drag: the dot trails behind when the doll moves "
                         "and bounces back, like hair.");
    case Material::kCloth:
      return QObject::tr("Cloth: dots hang together like a sheet and "
                         "swing when the doll moves.");
    case Material::kJelly:
      return QObject::tr("Jelly: dots wobble as one soft blob and keep "
                         "their area.");
  }
  return QString();
}

}  // namespace

KindTiles::KindTiles(MotionManager* motion, QWidget* parent)
    : QWidget(parent), layout_(this) {
  assert(motion != nullptr);
  assert(parent != nullptr);
  layout_.setContentsMargins(0, 0, 0, 0);
  for (int i = 0; i < kMaterialCount; ++i) {
    const auto material = static_cast<Material>(i);
    // Qt child of this row, which owns it.
    auto* tile =
        std::make_unique<KindTile>(material, motion, this).release();
    tile->setObjectName(kNames[i]);
    layout_.addWidget(tile);
    tiles_[static_cast<size_t>(i)] = tile;
    connect(tile, &QAbstractButton::clicked, this,
            [this, material] { emit Chosen(material); });
  }
  layout_.addStretch(1);
}

KindTile* KindTiles::TileOf(Material material) {
  assert(static_cast<int>(material) < kMaterialCount);
  assert(static_cast<int>(material) >= 0);
  return tiles_[static_cast<size_t>(material)];
}

void KindTiles::Show(const KindMix& mix, const QString& why_not) {
  assert(mix.total() >= 0);
  assert(tiles_[0] != nullptr);
  for (KindTile* tile : tiles_) {
    const Material material = tile->material();
    tile->SetShare(mix.ShareOf(material));
    Explain(tile, why_not, HelpOf(material));
  }
}

}  // namespace snapper
