#include "ui/rig_canvas.h"

#include <QPainter>

#include <algorithm>
#include <cassert>

#include "anim/doll_pose.h"
#include "anim/skin.h"
#include "edit/history_manager.h"
#include "edit/palette_manager.h"
#include "render/layer_painter.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kFill = 0.9;
constexpr double kJointRadius = kHandleDraw / 2;
// The thin outline of the piece under the cursor.
constexpr QColor kHoverLine{255, 255, 255, 200};

// Where everything is drawn at rest: one matrix per piece.
std::map<QString, QTransform> RestPieces(const Doll& doll) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  auto pieces = PieceTransforms(doll, PoseMap());
  assert(pieces.size() <= doll.rig.pieces.size());
  return pieces;
}

}  // namespace

RigCanvas::RigCanvas(const Managers& managers)
    : managers_(managers),
      hover_(managers.motion, this),
      feedback_(managers.motion, this) {
  assert(managers_.IsComplete());
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  connect(managers_.history, &HistoryManager::Changed, this,
          [this] { update(); });
  connect(&hover_, &HandleHover::Changed, this, [this] { update(); });
  connect(&feedback_, &DragFeedback::Changed, this, [this] { update(); });
  if (managers_.palette != nullptr) {
    connect(managers_.palette, &PaletteManager::Picked, this,
            [this](const QColor& color) { ColorJointCircle(color); });
  }
  assert(drag_ == Drag::kNone);
}

void RigCanvas::SetDoll(const QString& doll) {
  assert(doll.size() < 100000);
  doll_ = doll;
  piece_.clear();
  picked_.clear();
  picked_dots_.clear();
  StopHanging();
  update();
  assert(piece_.isEmpty());
}

void RigCanvas::SetSkin(const QString& skin) {
  assert(skin.size() < 100000);
  skin_ = skin;
  update();
  assert(skin_ == skin);
}

void RigCanvas::SetPiece(const QString& piece) {
  assert(piece.size() < 100000);
  SetPick(piece.isEmpty() ? QStringList() : QStringList{piece}, piece);
  assert(piece_ == piece);
}

void RigCanvas::SetPick(const QStringList& picked, const QString& focus) {
  assert(picked.size() <= kMaxDollPieces);
  assert(focus.size() < 100000);
  picked_ = std::set<QString>(picked.begin(), picked.end());
  PruneDots();
  piece_ = focus;
  update();
}

QTransform RigCanvas::World() const {
  assert(managers_.history != nullptr);
  const Doll* doll = doll_.isEmpty()
                         ? nullptr
                         : FindDoll(managers_.history->current(), doll_);
  QRectF bounds(-50, -50, 100, 100);
  const bool has_doll = doll != nullptr;
  if (has_doll) {
    for (const ArtPiece& art : doll->art.pieces) {
      bounds = bounds.united(QRectF(art.position, QSizeF(art.size)));
    }
  }
  const double scale = kFill * std::min(width() / bounds.width(),
                                        height() / bounds.height());
  assert(scale > 0.0 || width() == 0 || height() == 0);
  // Fitted round the widget's middle, then zoomed there and slid.
  return QTransform::fromTranslate(-bounds.center().x(),
                                   -bounds.center().y()) *
         QTransform::fromScale(scale * zoom_, scale * zoom_) *
         QTransform::fromTranslate(width() / 2.0 + pan_.x(),
                                   height() / 2.0 + pan_.y());
}

void RigCanvas::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), theme::kFaceDark);
  const Project& project = managers_.history->current();
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(project, doll_);
  const bool has_doll = doll != nullptr;
  if (!has_doll) {
    painter.setPen(theme::kTextOff);
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("Import a doll from the Cast panel to rig it."));
    return;
  }
  const QTransform world = World();
  const QSizeF canvas(doll->art.canvas);
  painter.setTransform(world);
  painter.fillRect(QRectF(QPointF(-canvas.width() / 2, -canvas.height() / 2),
                          canvas),
                   Qt::white);
  painter.resetTransform();
  Layer rest = RestLayer(doll_);
  const bool is_skinned = !skin_.isEmpty();
  if (is_skinned) {
    std::get<DollLayer>(rest.content).skins =
        WornFromStart(SkinSet{{skin_}});
  }
  PaintLayer(project, rest, Frame(0), world, nullptr, &cache_, &painter);
  painter.setRenderHint(QPainter::Antialiasing);
  PaintOverlay(&painter);
  if (is_boxing_) {
    painter.setPen(QPen(theme::kPick, 1.0, Qt::DashLine));
    painter.setBrush(QColor(theme::kPick.red(), theme::kPick.green(),
                            theme::kPick.blue(), 40));
    painter.drawRect(QRectF(box_from_, box_to_).normalized());
  }
  feedback_.Paint(&painter, QRectF(rect()));
}

void RigCanvas::PaintOverlay(QPainter* painter) const {
  assert(painter != nullptr);
  assert(!doll_.isEmpty());
  const Doll& doll = *FindDoll(managers_.history->current(), doll_);
  const auto pieces = RestPieces(doll);
  for (const RigPiece& rig : doll.rig.pieces) {
    const bool is_placed = pieces.contains(rig.name);
    if (is_placed) {
      PaintJoint(painter, doll, pieces, rig);
    }
    const bool shows_collider = is_placed && picked_.contains(rig.name);
    if (shows_collider) {
      PaintCollider(painter, pieces, rig);
    }
  }
  PaintShapeSnap(painter, pieces);
  const bool shows_hang = is_hanging_ && pieces.contains(piece_);
  if (shows_hang) {
    const QTransform world = World();
    const RigPiece& hung = *FindRig(doll.rig, piece_);
    painter->setPen(QPen(theme::kPick, 2.0, Qt::DashLine));
    painter->drawLine((pieces.at(hung.name) * world).map(hung.pivot),
                      cursor_);
  }
  for (const IkChain& chain : doll.rig.chains) {
    const bool is_placed = pieces.contains(chain.lower);
    if (is_placed) {
      PaintTip(painter, pieces, chain);
    }
  }
}

void RigCanvas::PaintJoint(QPainter* painter, const Doll& doll,
                           const std::map<QString, QTransform>& pieces,
                           const RigPiece& rig) const {
  assert(painter != nullptr);
  assert(pieces.contains(rig.name));
  const QTransform world = World();
  const auto joint = [&](const RigPiece& piece) {
    return (pieces.at(piece.name) * world).map(piece.pivot);
  };
  const RigPiece* parent =
      rig.parent.isEmpty() ? nullptr : FindRig(doll.rig, rig.parent);
  const bool has_bone = parent != nullptr && pieces.contains(parent->name);
  if (has_bone) {
    painter->setPen(QPen(theme::kHandle, 1.0, Qt::DashLine));
    painter->drawLine(joint(*parent), joint(rig));
  }
  const bool is_picked = picked_.contains(rig.name);
  // Picked is red and solid; the piece a click would pick is thin.
  const bool is_hovered = rig.name == hover_piece_ && !is_picked;
  const bool is_outlined = is_picked || is_hovered;
  if (is_outlined) {
    const ArtPiece* art = FindArt(doll, rig.name);
    painter->setPen(is_picked ? QPen(theme::kPick, 2.0)
                              : QPen(kHoverLine, 1.0, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawPolygon((pieces.at(rig.name) * world)
                             .map(QPolygonF(QRectF(QPointF(),
                                                   QSizeF(art->size)))));
  }
  const double radius = (is_picked ? kJointRadius * 1.6 : kJointRadius) *
                        hover_.ScaleOf(GrabKind::kPivot, rig.name);
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kHandle);
  painter->drawEllipse(joint(rig), radius, radius);
}

void RigCanvas::PaintTip(QPainter* painter,
                         const std::map<QString, QTransform>& pieces,
                         const IkChain& chain) const {
  assert(painter != nullptr);
  assert(pieces.contains(chain.lower));
  const QPointF tip = (pieces.at(chain.lower) * World()).map(chain.tip);
  const double side =
      kHandleDraw * hover_.ScaleOf(GrabKind::kIkTip, chain.name);
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kHandle);
  painter->drawRect(QRectF(tip - QPointF(side / 2, side / 2),
                           QSizeF(side, side)));
}

}  // namespace snapper
