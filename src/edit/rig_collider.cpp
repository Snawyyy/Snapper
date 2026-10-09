// RigManager's colliders: a piece's outline that pushes cloth.

#include <QImage>
#include <QRectF>

#include <algorithm>
#include <cassert>

#include "anim/outline_trace.h"
#include "edit/history_manager.h"
#include "edit/rig_edits.h"
#include "edit/rig_manager.h"

namespace snapper {
namespace {

// The collider an outline of dots makes: all of them kept, pushing as
// deep as half the outline's narrower side.
Collider ColliderOf(const std::vector<QPointF>& ring) {
  assert(!ring.empty());
  assert(ring.size() <= static_cast<size_t>(kMaxColliderDots));
  Collider collider;
  collider.ring_size = static_cast<int>(ring.size());
  double left = ring.front().x();
  double right = left;
  double top = ring.front().y();
  double bottom = top;
  for (size_t i = 0; i < ring.size(); ++i) {
    collider.dots.push_back({ring[i], static_cast<int>(i)});
    left = std::min(left, ring[i].x());
    right = std::max(right, ring[i].x());
    top = std::min(top, ring[i].y());
    bottom = std::max(bottom, ring[i].y());
  }
  collider.depth = std::min(right - left, bottom - top) / 2.0;
  return collider;
}

}  // namespace

Result<void> RigManager::MakeCollider(const QString& doll,
                                      const QString& piece,
                                      const QImage& drawing) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const std::vector<QPointF> ring = TraceOutline(
      drawing, kOutlineAlpha, kOutlineSpacing, kMaxColliderDots);
  const bool is_visible = !ring.empty();
  if (!is_visible) {
    return std::unexpected(
        Error{Tr("There is nothing visible in that drawing to outline.")});
  }
  const Collider collider = ColliderOf(ring);
  return history_->Apply(
      Tr("Make collider of %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [&collider](RigPiece* edited) -> Result<void> {
                  // A new outline starts with every piece pushed.
                  edited->collider = collider;
                  return {};
                }));
}

Result<void> RigManager::DeleteColliderDots(const QString& doll,
                                            const QString& piece,
                                            const std::vector<int>& gone) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  return history_->Apply(
      Tr("Delete collider dots of %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [&gone](RigPiece* edited) -> Result<void> {
                  Collider& collider = edited->collider;
                  const size_t before = collider.dots.size();
                  std::erase_if(collider.dots, [&gone](const ColliderDot& d) {
                    return std::find(gone.begin(), gone.end(), d.slot) !=
                           gone.end();
                  });
                  const bool is_changed = collider.dots.size() != before;
                  if (!is_changed) {
                    return std::unexpected(
                        Error{Tr("Pick one or more collider dots first.")});
                  }
                  return {};
                }));
}

Result<void> RigManager::RemoveCollider(const QString& doll,
                                        const QString& piece) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  return history_->Apply(
      Tr("Remove collider of %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [](RigPiece* edited) -> Result<void> {
                  const bool is_on = edited->collider.IsOn();
                  if (!is_on) {
                    return std::unexpected(
                        Error{Tr("That piece is not a collider.")});
                  }
                  edited->collider = Collider();
                  return {};
                }));
}

Result<void> RigManager::SetColliderPushes(const QString& doll,
                                           const QString& piece,
                                           const QString& other,
                                           bool pushes) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty() && !other.isEmpty());
  return history_->Apply(
      pushes ? Tr("%1 pushes %2").arg(piece, other)
             : Tr("%1 spares %2").arg(piece, other),
      WithPiece(history_->current(), doll, piece,
                [&other, pushes](RigPiece* edited) -> Result<void> {
                  const bool is_on = edited->collider.IsOn();
                  if (!is_on) {
                    return std::unexpected(
                        Error{Tr("That piece is not a collider.")});
                  }
                  std::vector<QString>& spared = edited->collider.spared;
                  std::erase(spared, other);
                  if (!pushes) {
                    spared.push_back(other);
                  }
                  return {};
                }));
}

}  // namespace snapper
