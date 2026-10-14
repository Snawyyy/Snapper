#include "model/storyboard.h"

#include <algorithm>
#include <cassert>

namespace snapper {

const StoryboardCel* CelAt(const StoryboardLayer& board, Frame frame) {
  assert(board.cels.size() <= static_cast<size_t>(kMaxStoryboardCels));
  assert(frame.index() > -1000000000);
  const StoryboardCel* showing = nullptr;
  for (const StoryboardCel& cel : board.cels) {
    const bool is_later = cel.at > frame;
    if (is_later) {
      break;
    }
    showing = &cel;
  }
  return showing;
}

const StoryboardCel* CelStartingAt(const StoryboardLayer& board,
                                   Frame frame) {
  assert(board.cels.size() <= static_cast<size_t>(kMaxStoryboardCels));
  assert(frame.index() > -1000000000);
  const auto found = std::ranges::find_if(
      board.cels,
      [frame](const StoryboardCel& cel) { return cel.at == frame; });
  return found == board.cels.end() ? nullptr : &*found;
}

const StoryboardCel* CelBefore(const StoryboardLayer& board, Frame frame) {
  assert(board.cels.size() <= static_cast<size_t>(kMaxStoryboardCels));
  assert(frame.index() > -1000000000);
  const StoryboardCel* before = nullptr;
  for (const StoryboardCel& cel : board.cels) {
    const bool is_reached = cel.at >= frame;
    if (is_reached) {
      break;
    }
    before = &cel;
  }
  return before;
}

const StoryboardCel* CelAfter(const StoryboardLayer& board, Frame frame) {
  assert(board.cels.size() <= static_cast<size_t>(kMaxStoryboardCels));
  assert(frame.index() > -1000000000);
  const auto found = std::ranges::find_if(
      board.cels,
      [frame](const StoryboardCel& cel) { return cel.at > frame; });
  return found == board.cels.end() ? nullptr : &*found;
}

void SortCels(StoryboardLayer* board) {
  assert(board != nullptr);
  assert(board->cels.size() <= static_cast<size_t>(kMaxStoryboardCels));
  std::ranges::stable_sort(board->cels, {}, [](const StoryboardCel& cel) {
    return cel.at.index();
  });
  assert(std::ranges::is_sorted(board->cels, {},
                                [](const StoryboardCel& cel) {
                                  return cel.at.index();
                                }));
}

}  // namespace snapper
