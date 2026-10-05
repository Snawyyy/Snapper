#ifndef SNAPPER_MODEL_PROJECT_H_
#define SNAPPER_MODEL_PROJECT_H_

#include <QString>

namespace snapper {

constexpr int kMinCanvasSide = 16;
// 8K: past that, frames stop fitting in GPU textures on common cards.
constexpr int kMaxCanvasSide = 7680;

// The video frame every shot renders into, in pixels.
struct CanvasSize final {
  int width = 1920;
  int height = 1080;

  bool operator==(const CanvasSize&) const = default;
};

bool IsValidCanvas(CanvasSize canvas);

// The whole document. A value: HistoryManager keeps snapshots of it for
// undo, so large parts are added as shared, read-only pointers.
struct Project final {
  QString name = QStringLiteral("Untitled");
  CanvasSize canvas;

  bool operator==(const Project&) const = default;
};

}  // namespace snapper

#endif  // SNAPPER_MODEL_PROJECT_H_
