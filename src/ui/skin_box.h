#ifndef SNAPPER_UI_SKIN_BOX_H_
#define SNAPPER_UI_SKIN_BOX_H_

#include <QGroupBox>
#include <QLabel>
#include <QShortcut>
#include <QVBoxLayout>

#include "ui/managers.h"
#include "ui/skin_tiles.h"

namespace snapper {

// The skins of the picked doll, in the inspector while the whole doll is
// picked and has skins: a click on a tile puts that skin on at the
// playhead (alone), a click on a lit one takes it off, Alt click stacks
// it (Shift means "add to the pick" everywhere, so never stack). Every
// picked doll with a skin of the same name changes too. Shift+[ and
// Shift+] step through the skins. Right click: Stack on what is worn,
// Update from current look. Skins are made in the Rig tab.
class SkinBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit SkinBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  void Wear(const QString& skin, Qt::KeyboardModifiers keys);
  // Puts skin on at the playhead, over what is worn when is_stacked.
  void Put(const QString& skin, bool is_stacked);
  void Step(int by);
  void ShowMenu(const QString& skin, QPoint global);

  Managers managers_;
  QVBoxLayout layout_;
  SkinTiles tiles_;
  // Owned by the box.
  QLabel* empty_ = nullptr;
  QShortcut previous_;
  QShortcut next_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SKIN_BOX_H_
