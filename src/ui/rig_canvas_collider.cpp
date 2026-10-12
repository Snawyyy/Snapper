// RigCanvas's colliders: the right-click menu, the outline's dots, and
// picking and deleting them.

#include <QContextMenuEvent>
#include <QMenu>
#include <QPainter>

#include <cassert>
#include <map>

#include "anim/doll_pose.h"
#include "anim/skin.h"
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
  // The drawing the piece shows here gets the collider.
  const QString name = RigDrawing(*doll, piece);
  const ArtPiece* art = FindArt(*doll, piece);
  const int index = art != nullptr ? FindDrawing(*art, name) : -1;
  const QImage& drawing = cache_.Get(DrawingPath(*doll, piece, index));
  emit Problem(
      ProblemOf(managers_.rig->MakeCollider(doll_, piece, drawing, name)));
  // Its dots show only while it is picked.
  ApplyPick({piece}, PickMode::kReplace, piece);
}

void RigCanvas::RemoveCollider(const QString& piece) {
  assert(managers_.history != nullptr);
  assert(!piece.isEmpty());
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const QString name = doll != nullptr ? RigDrawing(*doll, piece) : QString();
  emit Problem(
      ProblemOf(managers_.rig->RemoveCollider(doll_, piece, name)));
  PruneDots();
  update();
}

QString RigCanvas::RigDrawing(const Doll& doll, const QString& piece) const {
  assert(!piece.isEmpty());
  assert(skin_.size() < 100000);
  return RestDrawingName(doll, skin_, piece);
}

const Collider& RigCanvas::RigCollider(const Doll& doll,
                                       const RigPiece& rig) const {
  assert(!rig.name.isEmpty());
  assert(rig.drawing_colliders.size() < 100000);
  const bool has_own = !rig.drawing_colliders.empty();
  return has_own ? DrawingCollider(rig, RigDrawing(doll, rig.name))
                 : rig.collider;
}

void RigCanvas::MirrorToOtherSide(const QString& piece) {
  assert(managers_.history != nullptr);
  assert(!piece.isEmpty());
  emit Problem(ProblemOf(managers_.rig->MirrorNodes(doll_, piece)));
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
  const bool is_collider = rig != nullptr && RigCollider(*doll, *rig).IsOn();
  QMenu menu(this);
  QAction* make = menu.addAction(is_collider ? tr("Remake collider")
                                             : tr("Make collider"));
  QAction* remove = menu.addAction(tr("Remove collider"));
  menu.addSeparator();
  QAction* snap = menu.addAction(tr("Snap to shape"));
  QAction* snap_all = menu.addAction(tr("Snap all joints"));
  QAction* circle = menu.addAction(tr("Joint circle\tJ"));
  circle->setCheckable(true);
  circle->setChecked(rig != nullptr && rig->joint_circle.is_on);
  menu.addSeparator();
  QAction* mirror = menu.addAction(tr("Mirror rig to other side"));
  Explain(mirror, managers_.rig->WhyNoMirror(doll_, *hit));
  Explain(remove, is_collider ? QString()
                              : tr("%1 is not a collider yet.").arg(*hit));
  std::vector<QString> all;
  for (const RigPiece& piece : doll->rig.pieces) {
    all.push_back(piece.name);
  }
  const QAction* chosen = menu.exec(event->globalPos());
  const bool is_make = chosen == make;
  const bool is_remove = chosen == remove;
  const bool is_mirror = chosen == mirror;
  if (is_make) {
    MakeCollider(*hit);
  } else if (is_remove) {
    RemoveCollider(*hit);
  } else if (is_mirror) {
    MirrorToOtherSide(*hit);
  } else if (chosen == snap) {
    SnapJoints({*hit});
  } else if (chosen == snap_all) {
    SnapJoints(all);
  } else if (chosen == circle) {
    ToggleJointCircle(*hit);
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
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  if (doll == nullptr) {
    return;
  }
  // Each piece's names first: the edits below replace the doll.
  std::map<QString, QString> drawings;
  for (const auto& [piece, gone] : by_piece) {
    drawings[piece] = RigDrawing(*doll, piece);
  }
  {
    EditScope scope(managers_.history, tr("Delete collider dots"));
    for (const auto& [piece, gone] : by_piece) {
      emit Problem(ProblemOf(managers_.rig->DeleteColliderDots(
          doll_, piece, gone, drawings.at(piece))));
    }
  }
  picked_dots_.clear();
  update();
}

void RigCanvas::PaintCollider(QPainter* painter, const Doll& doll,
                              const std::map<QString, QTransform>& pieces,
                              const RigPiece& rig) const {
  assert(painter != nullptr);
  assert(pieces.contains(rig.name));
  const Collider& collider = RigCollider(doll, rig);
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
