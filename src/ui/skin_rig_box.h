#ifndef SNAPPER_UI_SKIN_RIG_BOX_H_
#define SNAPPER_UI_SKIN_RIG_BOX_H_

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/managers.h"
#include "ui/skin_tiles.h"
#include "ui/tape_key.h"

namespace snapper {

// The Rig tab's Skins box for the doll being rigged: "+" makes a skin
// (a copy of the one being changed, or an empty one), From Krita names
// makes one per "skin/drawing" prefix. Clicking a tile picks that skin
// to change: the rig canvas shows the doll wearing it and the Drawing
// picker sets the picked pieces' drawing in it (or hides them) instead
// of the doll's default; clicking it again stops. Right click a tile:
// Rename, Duplicate, Move up, Move down, Delete.
// A plated section of its own (plated_box.h).
class SkinRigBox final : public QWidget {
  Q_OBJECT

 public:
  explicit SkinRigBox(const Managers& managers);

  // Shows doll's skins; picking another doll stops changing a skin.
  void SetDoll(const QString& doll);
  // The skin being changed, or empty.
  const QString& skin() const { return skin_; }
  void Refresh();

 signals:
  // The skin being changed is now skin (empty for none).
  void SkinPicked(const QString& skin);
  void Problem(const QString& why);

 private:
  void Pick(const QString& skin);
  void ShowMenu(const QString& skin, QPoint global);

  Managers managers_;
  QString doll_;
  QString skin_;
  QVBoxLayout layout_;
  SkinTiles tiles_;
  QHBoxLayout buttons_;
  TapeKey add_;
  TapeKey from_krita_;
  QLabel hint_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SKIN_RIG_BOX_H_
