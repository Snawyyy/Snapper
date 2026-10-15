#ifndef SNAPPER_UI_CRT_EFFECT_H_
#define SNAPPER_UI_CRT_EFFECT_H_

#include <QGraphicsEffect>
#include <QPixmap>

class QPainter;

namespace snapper {

// Shows a widget as if on a small CRT tube: every screen pixel lit
// through one colored phosphor of a repeating tile,
//   r g b
//   b r g
// with no scan lines and no shadow mask. Numbers and readouts wear it.
// The tile is tied to the screen, so it holds still as widgets move.
class CrtEffect final : public QGraphicsEffect {
  Q_OBJECT

 public:
  CrtEffect();

 protected:
  void draw(QPainter* painter) override;

 private:
  // One 3 by 2 tile of phosphors.
  QPixmap tile_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CRT_EFFECT_H_
