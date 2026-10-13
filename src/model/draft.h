#ifndef SNAPPER_MODEL_DRAFT_H_
#define SNAPPER_MODEL_DRAFT_H_

#include <QString>

#include <vector>

#include "base/frame.h"

namespace snapper {

// The most cels one shot's draft holds.
inline constexpr int kMaxDraftCels = 2000;
// How strongly a new draft shows over the shot.
inline constexpr double kDefaultDraftOpacity = 0.6;

// One picture of a draft: a PNG the size of the canvas, shown from at
// until the next cel (or the shot's end). Each edit saves a new file,
// so a path never changes its pixels and undo needs no copies.
struct DraftCel final {
  Frame at;
  QString path;

  bool operator==(const DraftCel&) const = default;
};

// A shot's draft: one raster sketch over the shot, made of cels, for
// planning the shot against its timing. It is drawn in canvas space,
// without the camera, over everything; export leaves it out.
struct Draft final {
  // Earliest first, at most one per frame.
  std::vector<DraftCel> cels;
  double opacity = kDefaultDraftOpacity;
  // The draft's eye.
  bool is_shown = true;

  bool operator==(const Draft&) const = default;
};

// The cel showing at frame (the last one at or before it), or nullptr
// before the first.
const DraftCel* CelAt(const Draft& draft, Frame frame);
// The cel that starts exactly at frame, or nullptr.
const DraftCel* CelStartingAt(const Draft& draft, Frame frame);
// The cel before the one showing at frame, and the first one after
// frame: the onion skins.
const DraftCel* CelBefore(const Draft& draft, Frame frame);
const DraftCel* CelAfter(const Draft& draft, Frame frame);
// Puts the cels in frame order.
void SortCels(Draft* draft);

}  // namespace snapper

#endif  // SNAPPER_MODEL_DRAFT_H_
