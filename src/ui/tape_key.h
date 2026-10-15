#ifndef SNAPPER_UI_TAPE_KEY_H_
#define SNAPPER_UI_TAPE_KEY_H_

#include <QPushButton>
#include <QSize>

class QPaintEvent;
class QPainter;
class QRectF;

namespace snapper {

// The symbol stamped on a tape key.
enum class TapeGlyph { kFirst, kBack, kPlay, kNext, kLast };

// A play bar button drawn as an old tape recorder's piano key: a
// brushed silver top with its symbol pressed into it over a front edge.
// It sinks as it is held (the press HoverFade eases) and, when
// checkable, stays half down while checked, like a latched Play key.
// The text is kept for tooltips and tests but not drawn.
class TapeKey final : public QPushButton {
  Q_OBJECT

 public:
  explicit TapeKey(TapeGlyph glyph);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  // How far down the key sits, 0 up to 1 fully pressed.
  double Depth() const;
  void PaintGlyph(QPainter* painter, const QRectF& top) const;

  TapeGlyph glyph_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TAPE_KEY_H_
