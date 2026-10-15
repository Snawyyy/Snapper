#ifndef SNAPPER_UI_TAPE_KEY_H_
#define SNAPPER_UI_TAPE_KEY_H_

#include <QPushButton>
#include <QSize>
#include <QString>

#include <optional>

class QPaintEvent;
class QPainter;
class QRectF;

namespace snapper {

// The symbol stamped on a tape key.
enum class TapeGlyph { kFirst, kBack, kPlay, kNext, kLast };

// A play bar button drawn as an old cassette recorder's piano key: a
// black plastic block, a round dimple in its top holding the symbol,
// its front face showing below.
// It sinks as it is held (the press HoverFade eases) and, when
// checkable, stays half down while checked, like a latched Play key.
// A glyph key keeps its text for tooltips and tests but does not draw
// it; a text key (a boom box's Rec or Eject) prints its text instead.
class TapeKey : public QPushButton {
  Q_OBJECT

 public:
  explicit TapeKey(TapeGlyph glyph);
  explicit TapeKey(const QString& text);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
  // The key's top face where it sits now, for marks drawn over it.
  QRectF TopFace() const;
  // Room kept clear at the top face's left, before the print.
  void SetPrintInset(double inset);

 private:
  // How far down the key sits, 0 up to 1 fully pressed.
  double Depth() const;
  void PaintGlyph(QPainter* painter, const QRectF& cup) const;
  void PaintPrint(QPainter* painter, const QRectF& top) const;

  std::optional<TapeGlyph> glyph_;
  double print_inset_ = 0.0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TAPE_KEY_H_
