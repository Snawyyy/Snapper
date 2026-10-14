#ifndef SNAPPER_EDIT_STORYBOARD_EDITS_H_
#define SNAPPER_EDIT_STORYBOARD_EDITS_H_

#include <QString>

#include <cassert>
#include <set>
#include <utility>

#include "base/error.h"
#include "base/frame.h"
#include "base/text.h"
#include "edit/project_edits.h"
#include "model/shot.h"
#include "model/storyboard.h"

namespace snapper {

// The pure edits of storyboard layers, so StoryboardManager only decides
// when to make one and tests can check each without a window.

// The storyboard inside layer, or nullptr when layer is another kind.
StoryboardLayer* BoardOf(Layer* layer);
const StoryboardLayer* BoardOf(const Layer& layer);

// The project with the storyboard of layer in shot changed by
// change(StoryboardLayer*), which may refuse; fails when the layer is
// gone or is another kind.
template <typename Change>
Result<Project> WithBoard(Project project, ShotId shot, LayerId layer,
                          Change change) {
  assert(shot.value() >= 0);
  assert(layer.value() >= 0);
  return WithLayer(std::move(project), shot, layer,
                   [&change](Layer* edited) -> Result<void> {
                     StoryboardLayer* board = BoardOf(edited);
                     const bool has_board = board != nullptr;
                     if (!has_board) {
                       return std::unexpected(
                           Error{Tr("That layer isn't a storyboard.")});
                     }
                     return change(board);
                   });
}

// The storyboard layer an action in shot means: picked when it is one,
// else the topmost one in the stack; invalid when the shot has none.
LayerId ActiveStoryboard(const Shot& shot, LayerId picked);
// "Storyboard", or "Storyboard 2", "Storyboard 3"... when taken.
QString NextStoryboardName(const Shot& shot);

// Makes the cel starting at at show path: a new cel, or the path swapped
// on the one already there. Fails when the layer holds the most cels.
Result<void> PutCel(StoryboardLayer* board, Frame at, const QString& path);
// Removes the cels starting at frames.
Result<void> RemoveCels(StoryboardLayer* board,
                        const std::set<Frame>& frames);
// Moves the cels starting at frames by offset frames. Refuses a cel
// landing before frame 0, past the last frame or on a cel that stays.
Result<void> MoveCels(StoryboardLayer* board, const std::set<Frame>& frames,
                      int offset);

}  // namespace snapper

#endif  // SNAPPER_EDIT_STORYBOARD_EDITS_H_
