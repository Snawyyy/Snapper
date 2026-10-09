// RigCanvas's colliders: the right-click menu, the outline's dots, and
// picking and deleting them.

#include <QContextMenuEvent>
#include <QMenu>
#include <QPainter>

#include <cassert>
#include <map>

#include "anim/doll_pose.h"
#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "render/stage_hit.h"
#include "ui/collider_keys.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"
#include "ui/rig_canvas.h"
#include "ui/theme.h"

namespace snapper {

void RigCanvas::MakeCollider(const QString& piece) {
  assert(managers_.history != nullptr);
  assert(!piece.isEmpty());
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const bool has_doll = doll != nullptr;
  if (!has_doll) {
    return;
  }
  const QImage& drawing = cache_.Get(DrawingPath(*doll, piece, -1));
  emit Problem(
      ProblemOf(managers_.rig->MakeCollider(doll_, piece, drawing)));
  // Its dots show only while it is picked.
  ApplyPick({piece}, PickMode::kReplace, piece);
}

void RigCanvas::RemoveCollider(const QString& piece) {
  assert(managers_.history != nullptr);
  assert(!piece.isEmpty());
  emit Problem(ProblemOf(managers_.rig->RemoveCollider(doll_, piece)));
  PruneDots();
  update();
}

void RigCanvas::contextMenuEvent(QContextMenuEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const auto hit =
      doll != nullptr ? HitDollPiece(*doll, PoseMap(), World(), event->pos(),
                                     &cache_)
                      : std::nullopt;
  const bool is_piece = hit.has_value();
  if (!is_piece) {
    return;
  }
  const RigPiece* rig = FindRig(doll->rig, *hit);
  const bool is_collider = rig != nullptr && rig->collider.IsOn();
  QMenu menu(this);
  QAction* make = menu.addAction(is_collider ? tr("Remake collider")
                                             : tr("Make collider"));
  QAction* remove = menu.addAction(tr("Remove collider"));
  Explain(remove, is_collider ? QString()
                              : tr("%1 is not a collider yet.").arg(*hit));
  const QAction* chosen = menu.exec(event->globalPos());
  const bool is_make = chosen == make;
  const bool is_remove = chosen == remove;
  if (is_make) {
    MakeCollider(*hit);
  } else if (is_remove) {
    RemoveCollider(*hit);
  }
}

void RigCanvas::PickDot(const QString& key, PickMode mode) {
  assert(!key.isEmpty());
  assert(ParseDotKey(key).has_value());
  picked_dots_ = Combine(picked_dots_, std::set<QString>{key}, mode);
  update();
}

void RigCanvas::PruneDots() {
  assert(picked_dots_.size() <= static_cast<size_t>(kMaxColliderDots) *
                                    kMaxDollPieces);
  std::erase_if(picked_dots_, [this](const QString& key) {
    const auto dot = ParseDotKey(key);
    return !dot || !picked_.contains(dot->first);
  });
  assert(picked_dots_.size() <= static_cast<size_t>(kMaxColliderDots) *
                                    kMaxDollPieces);
}

void RigCanvas::DeletePickedDots() {
  assert(managers_.history != nullptr);
  assert(picked_dots_.size() <= static_cast<size_t>(kMaxColliderDots) *
                                    kMaxDollPieces);
  std::map<QString, std::vector<int>> by_piece;
  for (const QString& key : picked_dots_) {
    const auto dot = ParseDotKey(key);
    if (dot) {
      by_piece[dot->first].push_back(dot->second);
    }
  }
  const bool has_dots = !by_piece.empty();
  if (!has_dots) {
    return;
  }
  {
    EditScope scope(managers_.history, tr("Delete collider dots"));
    for (const auto& [piece, gone] : by_piece) {
      emit Problem(
          ProblemOf(managers_.rig->DeleteColliderDots(doll_, piece, gone)));
    }
  }
  picked_dots_.clear();
  update();
}

void RigCanvas::PaintCollider(QPainter* painter,
                              const std::map<QString, QTransform>& pieces,
                              const RigPiece& rig) const {
  assert(painter != nullptr);
  assert(pieces.contains(rig.name));
  const Collider& collider = rig.collider;
  const bool is_on = collider.IsOn();
  if (!is_on) {
    return;
  }
  const QTransform to_widget = pieces.at(rig.name) * World();
  const size_t count = collider.dots.size();
  painter->setPen(QPen(theme::kCollider, 2.0));
  for (size_t i = 0; i < count; ++i) {
    const ColliderDot& from = collider.dots[i];
    const ColliderDot& to = collider.dots[(i + 1) % count];
    const bool is_edge =
        count > 1 && (from.slot + 1) % collider.ring_size == to.slot;
    if (is_edge) {
      painter->drawLine(to_widget.map(from.at), to_widget.map(to.at));
    }
  }
  for (const ColliderDot& dot : collider.dots) {
    const QString key = DotKey(rig.name, dot.slot);
    const bool is_picked = picked_dots_.contains(key);
    const double radius = (is_picked ? kHandleDraw * 0.8 : kHandleDraw / 2) *
                          hover_.ScaleOf(GrabKind::kColliderDot, key);
    painter->setPen(QPen(theme::kShadow, 1.0));
    painter->setBrush(is_picked ? theme::kPick : theme::kCollider);
    painter->drawEllipse(to_widget.map(dot.at), radius, radius);
  }
}

}  // namespace snapper
