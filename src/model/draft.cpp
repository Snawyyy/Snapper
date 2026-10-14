#include "model/draft.h"

#include <algorithm>
#include <cassert>

namespace snapper {

const DraftCel* CelAt(const Draft& draft, Frame frame) {
  assert(draft.cels.size() <= static_cast<size_t>(kMaxDraftCels));
  assert(frame.index() > -1000000000);
  const DraftCel* showing = nullptr;
  for (const DraftCel& cel : draft.cels) {
    if (cel.at.index() > frame.index()) {
      break;
    }
    showing = &cel;
  }
  return showing;
}

const DraftCel* CelStartingAt(const Draft& draft, Frame frame) {
  assert(draft.cels.size() <= static_cast<size_t>(kMaxDraftCels));
  assert(frame.index() > -1000000000);
  const auto found =
      std::ranges::find_if(draft.cels, [frame](const DraftCel& cel) {
        return cel.at.index() == frame.index();
      });
  return found == draft.cels.end() ? nullptr : &*found;
}

const DraftCel* CelBefore(const Draft& draft, Frame frame) {
  assert(draft.cels.size() <= static_cast<size_t>(kMaxDraftCels));
  const DraftCel* showing = CelAt(draft, frame);
  if (showing == nullptr) {
    return nullptr;
  }
  const bool is_first = showing == &draft.cels.front();
  return is_first ? nullptr : showing - 1;
}

const DraftCel* CelAfter(const Draft& draft, Frame frame) {
  assert(draft.cels.size() <= static_cast<size_t>(kMaxDraftCels));
  assert(frame.index() > -1000000000);
  const auto found =
      std::ranges::find_if(draft.cels, [frame](const DraftCel& cel) {
        return cel.at.index() > frame.index();
      });
  return found == draft.cels.end() ? nullptr : &*found;
}

void SortCels(Draft* draft) {
  assert(draft != nullptr);
  std::ranges::stable_sort(draft->cels, {}, [](const DraftCel& cel) {
    return cel.at.index();
  });
  assert(std::ranges::is_sorted(draft->cels, {}, [](const DraftCel& cel) {
    return cel.at.index();
  }));
}

}  // namespace snapper
