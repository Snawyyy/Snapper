#include "edit/link_manager.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/links.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"

namespace snapper {
namespace {

// True when end names a layer of shot, and a piece of its doll when it
// names one.
bool Exists(const Project& project, const Shot& shot, const LinkEnd& end) {
  assert(end.layer.value() >= 0);
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  const Layer* layer =
      end.layer.IsValid() ? FindLayer(shot, end.layer) : nullptr;
  if (layer == nullptr) {
    return false;
  }
  const bool is_whole = end.piece.isEmpty();
  const Doll* doll = DollOfLayer(project, shot.id, end.layer);
  return is_whole || (doll != nullptr && FindRig(doll->rig, end.piece));
}

}  // namespace

LinkManager::LinkManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
}

QString LinkManager::WhyNoLink(ShotId shot,
                               const std::vector<LinkEnd>& followers,
                               const LinkEnd& leader) const {
  assert(history_ != nullptr);
  assert(followers.size() < 100000);
  const Project& project = history_->current();
  const Shot* found = FindShot(project, shot);
  if (found == nullptr) {
    return Tr("That shot no longer exists.");
  }
  const bool has_leader = Exists(project, *found, leader);
  if (!has_leader) {
    return Tr("That is no longer on the stage.");
  }
  const bool has_followers =
      !followers.empty() &&
      std::ranges::all_of(followers, [&](const LinkEnd& follower) {
        return Exists(project, *found, follower);
      });
  if (!has_followers) {
    return Tr("Pick what should follow first.");
  }
  const bool is_own = std::ranges::any_of(
      followers,
      [&leader](const LinkEnd& f) { return f.layer == leader.layer; });
  if (is_own) {
    return Tr("Something picked is part of %1; link to something else.")
        .arg(NameOf(shot, leader));
  }
  const bool is_loop =
      std::ranges::any_of(followers, [&](const LinkEnd& follower) {
        return WouldLoop(project, *found, follower, leader);
      });
  if (is_loop) {
    return Tr("%1 already follows what is picked; that would loop.")
        .arg(NameOf(shot, leader));
  }
  const auto added = std::ranges::count_if(
      followers,
      [found](const LinkEnd& f) { return FindLink(*found, f) == nullptr; });
  const bool is_full = found->links.size() + static_cast<size_t>(added) >
                       static_cast<size_t>(kMaxLinksPerShot);
  if (is_full) {
    return Tr("A shot holds at most %1 links.").arg(kMaxLinksPerShot);
  }
  return QString();
}

Result<void> LinkManager::LinkTo(ShotId shot,
                                 const std::vector<LinkEnd>& followers,
                                 const LinkEnd& leader, Frame from) {
  assert(history_ != nullptr);
  assert(from.index() >= 0);
  const QString why_not = WhyNoLink(shot, followers, leader);
  const bool can_link = why_not.isEmpty();
  if (!can_link) {
    return std::unexpected(Error{why_not});
  }
  return history_->Apply(
      Tr("Link movement"),
      WithShot(history_->current(), shot, [&](Shot* edited) {
        for (const LinkEnd& follower : followers) {
          std::erase_if(edited->links, [&follower](const Link& link) {
            return link.follower == follower;
          });
          edited->links.push_back(Link{follower, leader, from, 1.0});
        }
        return Result<void>();
      }));
}

QString LinkManager::WhyNoUnlink(
    ShotId shot, const std::vector<LinkEnd>& followers) const {
  assert(history_ != nullptr);
  assert(followers.size() < 100000);
  const Shot* found = FindShot(history_->current(), shot);
  const bool has_link =
      found != nullptr &&
      std::ranges::any_of(followers, [found](const LinkEnd& f) {
        return FindLink(*found, f) != nullptr;
      });
  return has_link ? QString() : Tr("Nothing picked follows anything.");
}

Result<void> LinkManager::Unlink(ShotId shot,
                                 const std::vector<LinkEnd>& followers) {
  assert(history_ != nullptr);
  assert(followers.size() < 100000);
  const QString why_not = WhyNoUnlink(shot, followers);
  const bool can_unlink = why_not.isEmpty();
  if (!can_unlink) {
    return std::unexpected(Error{why_not});
  }
  return history_->Apply(
      Tr("Unlink movement"),
      WithShot(history_->current(), shot, [&followers](Shot* edited) {
        std::erase_if(edited->links, [&followers](const Link& link) {
          return std::ranges::find(followers, link.follower) !=
                 followers.end();
        });
        return Result<void>();
      }));
}

Result<void> LinkManager::ShiftStrength(
    ShotId shot, const std::vector<LinkEnd>& followers, double delta) {
  assert(history_ != nullptr);
  assert(followers.size() < 100000);
  const bool is_number = std::isfinite(delta);
  const QString why_not = WhyNoUnlink(shot, followers);
  if (!is_number || !why_not.isEmpty()) {
    return std::unexpected(
        Error{is_number ? why_not : Tr("That isn't a number.")});
  }
  return history_->Apply(
      Tr("Link strength"),
      WithShot(history_->current(), shot, [&](Shot* edited) {
        for (Link& link : edited->links) {
          const bool is_picked =
              std::ranges::find(followers, link.follower) != followers.end();
          if (is_picked) {
            link.strength =
                std::clamp(link.strength + delta, 0.0, kMaxLinkStrength);
          }
        }
        return Result<void>();
      }));
}

QString LinkManager::NameOf(ShotId shot, const LinkEnd& end) const {
  assert(history_ != nullptr);
  assert(end.layer.value() >= 0);
  const Shot* found = FindShot(history_->current(), shot);
  const Layer* layer = found != nullptr && end.layer.IsValid()
                           ? FindLayer(*found, end.layer)
                           : nullptr;
  if (layer == nullptr) {
    return Tr("something gone");
  }
  const bool is_whole = end.piece.isEmpty();
  return is_whole ? layer->name
                  : Tr("%1's %2").arg(layer->name, end.piece);
}

}  // namespace snapper
