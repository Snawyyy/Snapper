#ifndef SNAPPER_UI_SNAPPER_STYLE_H_
#define SNAPPER_UI_SNAPPER_STYLE_H_

#include <QColor>
#include <QPainter>
#include <QProxyStyle>
#include <QRect>

namespace snapper {

// The dynamic property a widget carries while it fades under the
// pointer: a double from 0 (away) to 1 (fully hovered). Without it the
// style reads the plain mouse-over state, so hover still shows.
inline constexpr char kHoverProperty[] = "snapper_hover";

// The same for a press: 0 at rest, 1 held down, a little below 0 while
// it springs back. The button is squashed by SquashScale of it.
inline constexpr char kPressProperty[] = "snapper_press";
// A held button shrinks by this fraction.
inline constexpr double kSquash = 0.04;

// The scale a button is drawn at for this press level.
double SquashScale(double press);

// How one button face looks: the colour it is filled with, and whether
// its bevel is sunk. Pure, so the press and hover states are tested
// without painting.
struct ButtonLook final {
  QColor fill;
  bool is_sunk = false;
};

// hover is 0 to 1. A pressed or latched button sinks; hovering
// brightens a button that can act; a button that cannot act never
// reacts.
ButtonLook LookOfButton(bool is_enabled, bool is_down, bool is_checked,
                        double hover);

// The one style of the app: Windows underneath, with raised buttons
// that brighten under the pointer and sink when pressed, chunky tabs,
// and sunken dropdowns. Every widget gets it from the application, so no widget
// carries a stylesheet.
// How wide a splitter handle or dock separator is, so it is easy to
// catch; the line drawn in it stays thin until the pointer is over it.
inline constexpr int kGripExtent = 10;

// Paints the ridges of a grip into box, along its long side: dark and
// raised at rest, yellow on a dark band when is_active (the pointer is
// over it or it is being dragged).
void DrawGripLine(QPainter* painter, const QRect& box, bool is_active);

class SnapperStyle final : public QProxyStyle {
 public:
  SnapperStyle();

  void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                     QPainter* painter,
                     const QWidget* widget) const override;
  void drawControl(ControlElement element, const QStyleOption* option,
                   QPainter* painter, const QWidget* widget) const override;
  // Dropdowns are sunken boxes with a raised arrow button.
  void drawComplexControl(ComplexControl control,
                          const QStyleOptionComplex* option,
                          QPainter* painter,
                          const QWidget* widget) const override;
  // A dropdown takes the full width it is given and opens a sunken list.
  void polish(QWidget* widget) override;
  using QProxyStyle::polish;
  // Splitter handles and dock separators grab big: kGripExtent wide.
  int pixelMetric(PixelMetric metric, const QStyleOption* option,
                  const QWidget* widget) const override;
  QSize sizeFromContents(ContentsType type, const QStyleOption* option,
                         const QSize& size,
                         const QWidget* widget) const override;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SNAPPER_STYLE_H_
