#ifndef SNAPPER_MODEL_PROJECT_H_
#define SNAPPER_MODEL_PROJECT_H_

#include <QString>

#include <map>
#include <memory>
#include <vector>

#include "model/doll.h"
#include "model/reel.h"
#include "model/shot.h"

namespace snapper {

constexpr int kMinCanvasSide = 16;
// 8K: the largest frame common video encoders accept.
constexpr int kMaxCanvasSide = 7680;
constexpr int kMaxShots = 512;
constexpr int kMaxProjectDolls = 64;

// The video frame every shot renders into, in pixels.
struct CanvasSize final {
  int width = 1920;
  int height = 1080;

  bool operator==(const CanvasSize&) const = default;
};

bool IsValidCanvas(CanvasSize canvas);

// The whole document. A value: HistoryManager keeps snapshots of it for
// undo, so shots and dolls are shared read-only pointers, and an edit
// copies only the one it changes.
struct Project final {
  QString name = QStringLiteral("Untitled");
  CanvasSize canvas;
  // The song every shot plays over; empty for none.
  QString song;
  // Master track order.
  std::vector<std::shared_ptr<const Shot>> shots;
  // Every doll the shots use, by name.
  std::map<QString, std::shared_ptr<const Doll>> dolls;
  // The final video, cut from shots and video files.
  Reel reel;
  int next_shot_id = 1;
  int next_layer_id = 1;
  int next_clip_id = 1;

  bool operator==(const Project&) const = default;
};

// Index of the shot with id, or -1.
int ShotIndex(const Project& project, ShotId id);
const Shot* FindShot(const Project& project, ShotId id);
const Doll* FindDoll(const Project& project, const QString& name);

}  // namespace snapper

#endif  // SNAPPER_MODEL_PROJECT_H_
