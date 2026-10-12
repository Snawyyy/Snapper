#ifndef SNAPPER_RENDER_REEL_TEXT_H_
#define SNAPPER_RENDER_REEL_TEXT_H_

#include <QPainter>
#include <QRectF>
#include <QString>

#include "model/project.h"

namespace snapper {

// The title-safe margin: text keeps this share of the frame in from
// each edge, and long lines wrap at it.
constexpr double kTitleSafe = 0.1;

// look's lettering holding text wrapped to the title-safe width of a
// canvas width pixels wide (a word longer than that stays whole).
TextLayer ReelLettering(const ReelTextLook& look, const QString& text,
                        double width);
// Where a text clip's lettering sits on the frame, in canvas pixels,
// at rest (no appear or leave motion): for picking and handles.
QRectF ReelTextRect(const Project& project, const Clip& clip);
// How many lines the clip's text takes once wrapped.
int ReelTextLines(const Project& project, const Clip& clip);
// Draws text clip, into frames after its start, onto painter, whose
// device is the frame at scale against the canvas. The look comes from
// LookOf; the appear and leave motions play at the clip's ends.
void PaintReelText(const Project& project, const Clip& clip, int into,
                   double scale, QPainter* painter);

}  // namespace snapper

#endif  // SNAPPER_RENDER_REEL_TEXT_H_
