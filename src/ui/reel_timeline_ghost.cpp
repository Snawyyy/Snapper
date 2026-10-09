// ReelTimeline's ghost: the clips a drag moves or trims.

#include <algorithm>
#include <cassert>
#include <vector>

#include "edit/history_manager.h"
#include "ui/reel_timeline.h"

namespace snapper {

void ReelTimeline::GrabGhost() {
  assert(drag_ == Drag::kMove || drag_ == Drag::kTrimStart ||
         drag_ == Drag::kTrimEnd);
  assert(managers_.history != nullptr);
  // A move takes every picked clip; a trim only the one dragged.
  const std::vector<ClipId> dragged =
      drag_ == Drag::kMove ? Picked() : std::vector<ClipId>{dragged_};
  const Reel& reel = managers_.history->current().reel;
  feedback_.SetGhost(MakeGhost(
      GhostKind::kClip, size(), devicePixelRatioF(),
      [&](QPainter* painter) {
        // The same cut as the tracks: nothing over the headers or ruler.
        painter->setClipRect(
            QRectF(kHeaderWidth, kRulerHeight, width(), height()));
        for (const ClipId id : dragged) {
          const Clip* clip = ClipOf(reel, id);
          const bool has_clip = clip != nullptr;
          if (has_clip) {
            PaintClip(painter, *clip);
          }
        }
      }));
}

}  // namespace snapper
