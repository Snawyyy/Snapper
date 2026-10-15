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

inline constexpr QColor kPlastic{0x25, 0x23, 0x21};
inline constexpr QColor kPlasticLight{0x36, 0x33, 0x30};
inline constexpr QColor kPlasticDark{0x12, 0x11, 0x10};
inline constexpr QColor kGroove{0x00, 0x00, 0x00, 90};
inline constexpr QColor kRidge{0xff, 0xff, 0xff, 12};
inline constexpr QColor kLabel{0xec, 0xe6, 0xd2};
inline constexpr QColor kLabelShut{0x8e, 0x88, 0x7a};
inline constexpr QColor kLabelRule{0xc4, 0x6a, 0x3a};
inline constexpr QColor kInk{0x3a, 0x34, 0x2c};
inline constexpr QColor kTape{0x5a, 0x34, 0x1e};
inline constexpr QColor kTapeLight{0x8c, 0x56, 0x30};
inline constexpr QColor kTapeDark{0x2e, 0x1a, 0x10};
// The display glows the same phosphor green as the radio dial, so the
// gear has one light color.
inline constexpr QColor kGlass{0x06, 0x10, 0x0c};
inline constexpr QColor kGlow{0x9c, 0xe8, 0xb4};
inline constexpr QColor kGlowDim{0x3a, 0x7a, 0x5a};
// Bands on a cassette's label, one per kind of clip.
inline constexpr QColor kBandShot{0xc4, 0x6a, 0x3a};
inline constexpr QColor kBandVideo{0x3e, 0x8e, 0x68};
inline constexpr QColor kBandText{0x8c, 0x56, 0x30};
inline constexpr QColor kBandMissing{0xd8, 0x30, 0x4f};
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

// A clip as a video cassette's spine: a black shell with a cream label
// filling it, a band of the clip kind's color across the label's top
// and name written under it (none for an empty name). A shut one (off)
// has a faded label.
void PaintCassette(QPainter* painter, const QRectF& box, const QColor& band,
                   const QString& name, bool is_shut);

// A colored strip of tape: color lit along its top, darker at its foot,
// for a bar that carries its color (a Comp effect's kind).
void PaintColorTape(QPainter* painter, const QRectF& run,
                    const QColor& color);

// The dark glass of a DVD player's display, sunk into the plastic.
void PaintDisplay(QPainter* painter, const QRectF& glass);

// text in the display's glow at area's left: lit, or dim when nothing
// is there to show.
void PaintGlowText(QPainter* painter, const QRectF& area,
                   const QString& text, bool is_lit);

// A small screw head at center: a grey dome with a dark slot.
void PaintScrew(QPainter* painter, QPointF center, double radius);

// The face of a deck's front panel over area: brushed plastic, lit
// along its top edge and shaded along its foot, a screw at each end.
void PaintPanel(QPainter* painter, const QRectF& area);

}  // namespace deck
}  // namespace snapper

#endif  // SNAPPER_UI_DECK_PAINT_H_
