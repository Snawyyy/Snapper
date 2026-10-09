#ifndef SNAPPER_UI_DRAG_PLAYHEAD_H_
#define SNAPPER_UI_DRAG_PLAYHEAD_H_

#include "base/frame.h"

namespace snapper {

class PlaybackManager;

// Remembers where the playhead was when a drag began, so the drag can
// carry the playhead along and Escape can put it back. The playhead is
// not project state, so none of this makes history.
class DragPlayhead {
 public:
  explicit DragPlayhead(PlaybackManager* playback);

  // Notes the playhead's place; call before the drag moves it.
  void Begin();
  // Where the playhead was at Begin.
  Frame origin() const { return origin_; }
  bool is_active() const { return is_active_; }
  // Puts the playhead back and ends the hold. False when none was held.
  bool Restore();
  // Ends the hold, leaving the playhead where the drag put it.
  void End();

 private:
  PlaybackManager* playback_;
  Frame origin_;
  bool is_active_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DRAG_PLAYHEAD_H_
