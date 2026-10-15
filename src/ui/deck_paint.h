#ifndef SNAPPER_UI_DECK_PAINT_H_
#define SNAPPER_UI_DECK_PAINT_H_

#include <QColor>
#include <QPainter>
#include <QRectF>
#include <QString>

namespace snapper {

// The timelines dress as old home audio and video gear: rows of black
// deck plastic, names on cream cassette labels, holds and clips as
// brown magnetic tape, sound and readouts on a glowing DVD player
// display. The ruler is the radio dial (timeline_ruler.cpp). Every
// timeline paints from here so the gear looks the same on each tab.
namespace deck {

inline constexpr QColor kPlastic{0x1c, 0x1c, 0x1e};
inline constexpr QColor kPlasticLight{0x2a, 0x2a, 0x2d};
inline constexpr QColor kPlasticDark{0x0e, 0x0e, 0x10};
inline constexpr QColor kGroove{0x00, 0x00, 0x00, 90};
inline constexpr QColor kRidge{0xff, 0xff, 0xff, 12};
inline constexpr QColor kLabel{0xec, 0xe6, 0xd2};
inline constexpr QColor kLabelShut{0x8e, 0x88, 0x7a};
inline constexpr QColor kLabelRule{0xc4, 0x6a, 0x3a};
inline constexpr QColor kInk{0x3a, 0x34, 0x2c};
inline constexpr QColor kTape{0x5a, 0x34, 0x1e};
inline constexpr QColor kTapeLight{0x8c, 0x56, 0x30};
inline constexpr QColor kTapeDark{0x2e, 0x1a, 0x10};
inline constexpr QColor kGlass{0x04, 0x0a, 0x0c};
inline constexpr QColor kGlow{0x46, 0xe0, 0xd0};
inline constexpr QColor kGlowDim{0x1e, 0x6a, 0x64};
inline constexpr QColor kAmber{0xf2, 0x8c, 0x28};

// Black plastic over area with a groove every row_height from area's
// top; the part past end (where nothing plays) a shade darker.
void PaintPlastic(QPainter* painter, const QRectF& area, double end,
                  double row_height);

// A cream label strip with a ruled line and text in ink; a shut one
// (muted, off) is faded, the current one wears a red tab at its left.
void PaintLabel(QPainter* painter, const QRectF& strip, const QString& text,
                bool is_current, bool is_shut);

// A run of brown tape: darker edges and a sheen along its top.
void PaintTape(QPainter* painter, const QRectF& run);

// The dark glass of a DVD player's display, sunk into the plastic.
void PaintDisplay(QPainter* painter, const QRectF& glass);

// text in the display's glow at area's left: lit, or dim when nothing
// is there to show.
void PaintGlowText(QPainter* painter, const QRectF& area,
                   const QString& text, bool is_lit);

}  // namespace deck
}  // namespace snapper

#endif  // SNAPPER_UI_DECK_PAINT_H_
