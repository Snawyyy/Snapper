#ifndef SNAPPER_MODEL_STORYBOARD_H_
#define SNAPPER_MODEL_STORYBOARD_H_

#include <QString>

#include <vector>

#include "base/frame.h"

namespace snapper {

// The most cels one storyboard layer holds.
inline constexpr int kMaxStoryboardCels = 2000;
// How strongly a new storyboard layer shows over what lies under it.
inline constexpr double kDefaultStoryboardOpacity = 0.6;
// The most limbs a storyboard layer can sink behind.
inline constexpr int kMaxStoryboardSink = 256;

// One picture of a storyboard layer: a PNG the size of the canvas, shown
// from at (a frame of the shot) until the next cel, or the shot's end.
// Each edit saves a new file, so a path never changes its pixels and
// undo needs no copies.
struct StoryboardCel final {
  Frame at;
  QString path;

  bool operator==(const StoryboardCel&) const = default;
};

// What a storyboard layer holds (a kind of Layer content, model/layer.h):
// a raster sketch of the shot's plan, made of cels. It is drawn in canvas
// space, with no camera and no layer transform, at its place in the
// layer stack, so draw order and the layer's eye work as for any layer.
// The shot's export leaves it out unless is_exported is on.
struct StoryboardLayer final {
  // Earliest first, at most one per frame.
  std::vector<StoryboardCel> cels;
  // 0 to 1.
  double opacity = kDefaultStoryboardOpacity;
  // In the exported video (an animatic); the stage and the Video tab's
  // preview show it either way.
  bool is_exported = false;
  // How many limbs of the doll right under it in the stack it sinks
  // behind: 0 lies over the whole doll, 1 goes behind its front-most
  // drawn limb, and so on. The limbs are counted in their draw order at
  // each frame, so a limb that moves forward passes in front. Without a
  // doll right under it (other storyboards between are passed over),
  // it does nothing.
  int sink = 0;

  bool operator==(const StoryboardLayer&) const = default;
};

// The cel showing at frame (the last one at or before it), or nullptr
// before the first.
const StoryboardCel* CelAt(const StoryboardLayer& board, Frame frame);
// The cel that starts exactly at frame, or nullptr.
const StoryboardCel* CelStartingAt(const StoryboardLayer& board, Frame frame);
// The last cel starting before frame: the onion skin of a cel drawn at
// frame, to trace from.
const StoryboardCel* CelBefore(const StoryboardLayer& board, Frame frame);
// Puts the cels in frame order.
void SortCels(StoryboardLayer* board);

}  // namespace snapper

#endif  // SNAPPER_MODEL_STORYBOARD_H_
