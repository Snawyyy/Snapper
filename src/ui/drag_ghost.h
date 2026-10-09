#ifndef SNAPPER_UI_DRAG_GHOST_H_
#define SNAPPER_UI_DRAG_GHOST_H_

#include <QImage>
#include <QPainter>
#include <QSize>

#include <cassert>
#include <utility>

namespace snapper {

// What a drag leaves behind where it began: a picture of the thing
// being dragged.
enum class GhostKind {
  // No picture: only the dashed ring marks the start.
  kNone,
  // Pieces or layers on the stage.
  kStagePick,
  // Joints or IK tips on the rig canvas.
  kRigHandle,
  // Keys on a shot's timeline.
  kKey,
  // Clips on the reel.
  kClip,
  // Shots on the shot strip.
  kShot,
};

// A faint copy of the dragged thing, as wide and tall as the canvas it
// is drawn on, so it needs no placing. Empty image: nothing to show.
struct Ghost final {
  GhostKind kind = GhostKind::kNone;
  QImage image;
};

// Draws a ghost by running the canvas's own paint code for the item
// (paint takes a QPainter*) onto a clear image of size, in canvas
// pixels. ratio is the screen's pixel ratio, so it stays sharp.
template <typename Paint>
Ghost MakeGhost(GhostKind kind, QSize size, double ratio, Paint&& paint) {
  assert(kind != GhostKind::kNone);
  assert(ratio > 0.0);
  const bool is_empty = size.isEmpty();
  if (is_empty) {
    return Ghost{kind, QImage()};
  }
  QImage image((QSizeF(size) * ratio).toSize(),
               QImage::Format_ARGB32_Premultiplied);
  image.setDevicePixelRatio(ratio);
  image.fill(Qt::transparent);
  {
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    paint(&painter);
  }
  return Ghost{kind, std::move(image)};
}

}  // namespace snapper

#endif  // SNAPPER_UI_DRAG_GHOST_H_
