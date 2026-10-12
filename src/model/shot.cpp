#include "model/shot.h"

#include <algorithm>
#include <cassert>

namespace snapper {

const Layer* FindLayer(const Shot& shot, LayerId id) {
  assert(id.IsValid());
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  const auto found =
      std::find_if(shot.layers.begin(), shot.layers.end(),
                   [id](const Layer& layer) { return layer.id == id; });
  return found == shot.layers.end() ? nullptr : &*found;
}

Layer* FindLayer(Shot* shot, LayerId id) {
  assert(shot != nullptr);
  assert(id.IsValid());
  const Layer* layer = FindLayer(static_cast<const Shot&>(*shot), id);
  return const_cast<Layer*>(layer);
}

const Link* FindLink(const Shot& shot, const LinkEnd& follower) {
  assert(shot.links.size() <= static_cast<size_t>(kMaxLinksPerShot));
  assert(follower.layer.value() >= 0);
  const auto found = std::ranges::find_if(
      shot.links, [&follower](const Link& link) {
        return link.follower == follower;
      });
  return found == shot.links.end() ? nullptr : &*found;
}

void DropDeadLinks(Shot* shot) {
  assert(shot != nullptr);
  assert(shot->links.size() <= static_cast<size_t>(kMaxLinksPerShot));
  std::erase_if(shot->links, [shot](const Link& link) {
    const bool is_alive = link.follower.layer.IsValid() &&
                          link.leader.layer.IsValid() &&
                          FindLayer(*shot, link.follower.layer) != nullptr &&
                          FindLayer(*shot, link.leader.layer) != nullptr;
    return !is_alive;
  });
  assert(shot->holds.size() <= static_cast<size_t>(kMaxHoldsPerShot));
  std::erase_if(shot->holds, [shot](const Hold& hold) {
    return !hold.object.IsValid() || FindLayer(*shot, hold.object) == nullptr;
  });
  for (Hold& hold : shot->holds) {
    const bool is_holder_gone =
        !hold.IsDrop() && FindLayer(*shot, hold.holder.layer) == nullptr;
    if (is_holder_gone) {
      hold.holder = LinkEnd{};
    }
  }
}

std::vector<const Hold*> HoldsOf(const Shot& shot, LayerId object) {
  assert(object.value() >= 0);
  assert(shot.holds.size() <= static_cast<size_t>(kMaxHoldsPerShot));
  std::vector<const Hold*> holds;
  for (const Hold& hold : shot.holds) {
    const bool is_its = hold.object == object;
    if (is_its) {
      holds.push_back(&hold);
    }
  }
  std::ranges::stable_sort(holds, [](const Hold* a, const Hold* b) {
    return a->at < b->at;
  });
  return holds;
}

const Hold* HoldAt(const Shot& shot, LayerId object, Frame frame) {
  assert(object.value() >= 0);
  assert(frame.index() >= 0);
  const Hold* found = nullptr;
  for (const Hold* hold : HoldsOf(shot, object)) {
    const bool is_started = hold->at <= frame;
    if (is_started) {
      found = hold;
    }
  }
  return found;
}

}  // namespace snapper
