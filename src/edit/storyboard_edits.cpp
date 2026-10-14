#include "edit/storyboard_edits.h"

#include <algorithm>
#include <cassert>
#include <variant>

#include "base/text.h"

namespace snapper {

StoryboardLayer* BoardOf(Layer* layer) {
  assert(layer != nullptr);
  assert(layer->id.IsValid());
  return std::get_if<StoryboardLayer>(&layer->content);
}

const StoryboardLayer* BoardOf(const Layer& layer) {
  assert(layer.id.IsValid());
  assert(layer.name.size() < 100000);
  return std::get_if<StoryboardLayer>(&layer.content);
}

LayerId ActiveStoryboard(const Shot& shot, LayerId picked) {
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  assert(picked.value() >= 0);
  const Layer* chosen = picked.IsValid() ? FindLayer(shot, picked) : nullptr;
  const bool is_picked_board =
      chosen != nullptr && BoardOf(*chosen) != nullptr;
  if (is_picked_board) {
    return picked;
  }
  for (auto it = shot.layers.rbegin(); it != shot.layers.rend(); ++it) {
    const bool is_board = BoardOf(*it) != nullptr;
    if (is_board) {
      return it->id;
    }
  }
  return LayerId();
}

namespace {

bool IsNameTaken(const Shot& shot, const QString& name) {
  assert(!name.isEmpty());
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  return std::ranges::any_of(shot.layers, [&name](const Layer& layer) {
    return layer.name == name;
  });
}

}  // namespace

QString NextStoryboardName(const Shot& shot) {
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  const QString plain = Tr("Storyboard");
  assert(!plain.isEmpty());
  const bool is_free = !IsNameTaken(shot, plain);
  if (is_free) {
    return plain;
  }
  for (int number = 2; number <= kMaxLayersPerShot + 1; ++number) {
    const QString numbered = Tr("Storyboard %1").arg(number);
    const bool is_numbered_free = !IsNameTaken(shot, numbered);
    if (is_numbered_free) {
      return numbered;
    }
  }
  return plain;
}

Result<void> PutCel(StoryboardLayer* board, Frame at, const QString& path) {
  assert(board != nullptr);
  assert(!path.isEmpty());
  const auto found = std::ranges::find_if(
      board->cels, [at](const StoryboardCel& cel) { return cel.at == at; });
  const bool is_swap = found != board->cels.end();
  if (is_swap) {
    found->path = path;
    return {};
  }
  const bool is_full =
      board->cels.size() >= static_cast<size_t>(kMaxStoryboardCels);
  if (is_full) {
    return std::unexpected(
        Error{Tr("A storyboard holds at most %1 frames.")
                  .arg(kMaxStoryboardCels)});
  }
  board->cels.push_back(StoryboardCel{at, path});
  SortCels(board);
  return {};
}

Result<void> RemoveCels(StoryboardLayer* board,
                        const std::set<Frame>& frames) {
  assert(board != nullptr);
  assert(frames.size() <= static_cast<size_t>(kMaxStoryboardCels));
  const auto removed = std::erase_if(
      board->cels,
      [&frames](const StoryboardCel& cel) { return frames.contains(cel.at); });
  const bool is_nothing = removed == 0;
  if (is_nothing) {
    return std::unexpected(Error{Tr("Those storyboard frames are gone.")});
  }
  return {};
}

Result<void> MoveCels(StoryboardLayer* board, const std::set<Frame>& frames,
                      int offset) {
  assert(board != nullptr);
  assert(frames.size() <= static_cast<size_t>(kMaxStoryboardCels));
  // Worked on a copy, so a refused move leaves every cel where it was.
  StoryboardLayer moved = *board;
  for (StoryboardCel& cel : moved.cels) {
    const bool is_moving = frames.contains(cel.at);
    if (!is_moving) {
      continue;
    }
    const int landed = cel.at.index() + offset;
    const bool is_off = landed < 0 || landed > kMaxFrame;
    if (is_off) {
      return std::unexpected(
          Error{Tr("A storyboard frame can't move before frame 0.")});
    }
    cel.at = Frame(landed);
  }
  SortCels(&moved);
  const auto clash = std::ranges::adjacent_find(
      moved.cels, [](const StoryboardCel& a, const StoryboardCel& b) {
        return a.at == b.at;
      });
  const bool has_clash = clash != moved.cels.end();
  if (has_clash) {
    return std::unexpected(
        Error{Tr("Another storyboard frame starts there already.")});
  }
  *board = std::move(moved);
  return {};
}

}  // namespace snapper
