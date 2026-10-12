#ifndef SNAPPER_UI_SKIN_TILES_H_
#define SNAPPER_UI_SKIN_TILES_H_

#include <QGridLayout>
#include <QPoint>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include <vector>

namespace snapper {

// A doll's skins as a grid of tiles, the list order reading left to
// right: lit tiles are the ones on. A click says which tile and the keys
// held; a right click asks for the tile's menu. Shared by the Rig tab's
// Skins box (making) and the inspector's (wearing).
class SkinTiles final : public QWidget {
  Q_OBJECT

 public:
  // hint is every tile's tooltip.
  explicit SkinTiles(const QString& hint, QWidget* parent = nullptr);

  // Shows names with the ones in lit lit; builds the tiles again only
  // when names changed.
  void Show(const std::vector<QString>& names,
            const std::vector<QString>& lit);
  int count() const { return static_cast<int>(tiles_.size()); }

 signals:
  void Clicked(const QString& name, Qt::KeyboardModifiers keys);
  void MenuAsked(const QString& name, QPoint global);

 private:
  QGridLayout grid_;
  std::vector<QPushButton*> tiles_;
  std::vector<QString> names_;
  QString hint_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SKIN_TILES_H_
