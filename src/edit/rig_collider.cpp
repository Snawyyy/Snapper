// RigManager's colliders: an outline of one drawing of a piece that
// pushes the warp dots of the doll's other pieces.

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

// The collider an outline of dots makes: all of them kept.
Collider RingCollider(const std::vector<QPointF>& ring) {
  assert(!ring.empty());
  assert(ring.size() <= static_cast<size_t>(kMaxColliderDots));
  Collider collider;
  collider.ring_size = static_cast<int>(ring.size());
  for (size_t i = 0; i < ring.size(); ++i) {
    collider.dots.push_back({ring[i], static_cast<int>(i)});
  }
  return collider;
}

// The collider of edited's drawing to change: the drawing's own, made
// from the piece's when it has none yet, or the piece's for an empty
// drawing.
Collider& EditedCollider(RigPiece* edited, const QString& drawing) {
  assert(edited != nullptr);
  assert(drawing.size() < 100000);
  const bool is_piece = drawing.isEmpty();
  if (is_piece) {
    return edited->collider;
  }
  const auto own = edited->drawing_colliders.find(drawing);
  const bool has_own = own != edited->drawing_colliders.end();
  return has_own ? own->second
                 : edited->drawing_colliders.emplace(drawing, edited->collider)
                       .first->second;
}

}  // namespace

Result<void> RigManager::MakeCollider(const QString& doll,
                                      const QString& piece,
                                      const QImage& drawing,
                                      const QString& drawing_name) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const std::vector<QPointF> ring = TraceOutline(
      drawing, kOutlineAlpha, kOutlineSpacing, kMaxColliderDots);
  const bool is_visible = !ring.empty();
  if (!is_visible) {
    return std::unexpected(
        Error{Tr("There is nothing visible in that drawing to outline.")});
  }
  const Collider collider = RingCollider(ring);
  return history_->Apply(
      Tr("Make collider of %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [&collider, &drawing_name](RigPiece* edited) -> Result<void> {
                  // A new outline starts with every piece pushed.
                  EditedCollider(edited, drawing_name) = collider;
                  return {};
                }));
}

Result<void> RigManager::DeleteColliderDots(const QString& doll,
                                            const QString& piece,
                                            const std::vector<int>& gone,
                                            const QString& drawing_name) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  return history_->Apply(
      Tr("Delete collider dots of %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [&gone, &drawing_name](RigPiece* edited) -> Result<void> {
                  Collider& collider = EditedCollider(edited, drawing_name);
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
                                        const QString& piece,
                                        const QString& drawing_name) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  return history_->Apply(
      Tr("Remove collider of %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [&drawing_name](RigPiece* edited) -> Result<void> {
                  const bool is_on =
                      DrawingCollider(*edited, drawing_name).IsOn();
                  if (!is_on) {
                    return std::unexpected(
                        Error{Tr("That piece is not a collider.")});
                  }
                  // A drawing keeps an empty one only to hide the
                  // piece's collider.
                  const bool is_own = !drawing_name.isEmpty();
                  if (is_own && !edited->collider.IsOn()) {
                    edited->drawing_colliders.erase(drawing_name);
                  } else {
                    EditedCollider(edited, drawing_name) = Collider();
                  }
                  return {};
                }));
}

Result<void> RigManager::SetColliderPushes(const QString& doll,
                                           const QString& piece,
                                           const QString& other,
                                           bool pushes,
                                           const QString& drawing_name) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty() && !other.isEmpty());
  return history_->Apply(
      pushes ? Tr("%1 pushes %2").arg(piece, other)
             : Tr("%1 spares %2").arg(piece, other),
      WithPiece(history_->current(), doll, piece,
                [&other, &drawing_name, pushes](RigPiece* edited)
                    -> Result<void> {
                  const bool is_on =
                      DrawingCollider(*edited, drawing_name).IsOn();
                  if (!is_on) {
                    return std::unexpected(
                        Error{Tr("That piece is not a collider.")});
                  }
                  std::vector<QString>& spared =
                      EditedCollider(edited, drawing_name).spared;
                  std::erase(spared, other);
                  if (!pushes) {
                    spared.push_back(other);
                  }
                  return {};
                }));
}

}  // namespace snapper
