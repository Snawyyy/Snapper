#ifndef SNAPPER_UI_KIND_TILES_H_
#define SNAPPER_UI_KIND_TILES_H_

#include <QHBoxLayout>
#include <QString>
#include <QWidget>

#include <array>

#include "ui/dot_kinds.h"
#include "ui/kind_tile.h"

namespace snapper {

class MotionManager;

// The four material tiles in a row: Still, Drag, Cloth, Jelly. It
// shows which are used by the picked dots and reports a click.
class KindTiles final : public QWidget {
  Q_OBJECT

 public:
  KindTiles(MotionManager* motion, QWidget* parent);

  // Outlines the used tiles; why_not greys all of them with that
  // reason (empty: they can be clicked).
  void Show(const KindMix& mix, const QString& why_not);
  KindTile* TileOf(Material material);

 signals:
  // The tile of material was clicked.
  void Chosen(Material material);

 private:
  QHBoxLayout layout_;
  std::array<KindTile*, kMaterialCount> tiles_{};
};

}  // namespace snapper

#endif  // SNAPPER_UI_KIND_TILES_H_
