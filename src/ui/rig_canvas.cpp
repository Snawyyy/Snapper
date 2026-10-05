#include "ui/rig_canvas.h"

#include <QPainter>

#include <algorithm>
#include <cassert>

#include "anim/doll_pose.h"
#include "edit/history_manager.h"
#include "render/layer_painter.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kFill = 0.9;
constexpr double kJointRadius = 3.5;

// The doll at rest, as a layer the painter can draw.
Layer RestLayer(const QString& doll) {
  assert(!doll.isEmpty());
  Layer layer;
  layer.id = LayerId(1);
  layer.content = DollLayer{doll, {}, false};
  assert(layer.transform.keys.empty());
  return layer;
}

// Where everything is drawn at rest: one matrix per piece.
std::map<QString, QTransform> RestPieces(const Doll& doll) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  auto pieces = PieceTransforms(doll, PoseMap());
  assert(pieces.size() <= doll.rig.pieces.size());
  return pieces;
}

}  // namespace

RigCanvas::RigCanvas(const Managers& managers) : managers_(managers) {
  assert(managers_.IsComplete());
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  connect(managers_.history, &HistoryManager::Changed, this,
          [this] { update(); });
  assert(drag_ == Drag::kNone);
}

void RigCanvas::SetDoll(const QString& doll) {
  assert(doll.size() < 100000);
  doll_ = doll;
  piece_.clear();
  StopHanging();
  update();
  assert(piece_.isEmpty());
}

void RigCanvas::SetPiece(const QString& piece) {
  assert(piece.size() < 100000);
  piece_ = piece;
  update();
  assert(piece_ == piece);
}

QTransform RigCanvas::World() const {
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
  return QTransform::fromTranslate(-bounds.center().x(),
                                   -bounds.center().y()) *
         QTransform::fromScale(scale, scale) *
         QTransform::fromTranslate(width() / 2.0, height() / 2.0);
}

void RigCanvas::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
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
  PaintLayer(project, RestLayer(doll_), Frame(0), world, &cache_, &painter);
  painter.setRenderHint(QPainter::Antialiasing);
  PaintOverlay(&painter);
}

void RigCanvas::PaintOverlay(QPainter* painter) const {
  assert(painter != nullptr);
  const Doll& doll = *FindDoll(managers_.history->current(), doll_);
  const QTransform world = World();
  const auto pieces = RestPieces(doll);
  const auto joint = [&](const RigPiece& rig) {
    return (pieces.at(rig.name) * world).map(rig.pivot);
  };
  for (const RigPiece& rig : doll.rig.pieces) {
    const bool is_placed = pieces.contains(rig.name);
    if (!is_placed) {
      continue;
    }
    const RigPiece* parent =
        rig.parent.isEmpty() ? nullptr : FindRig(doll.rig, rig.parent);
    const bool has_bone = parent != nullptr && pieces.contains(parent->name);
    if (has_bone) {
      painter->setPen(QPen(theme::kHandle, 1.0, Qt::DashLine));
      painter->drawLine(joint(*parent), joint(rig));
    }
    const bool is_picked = rig.name == piece_;
    if (is_picked) {
      const ArtPiece* art = FindArt(doll, rig.name);
      painter->setPen(QPen(theme::kPick, 2.0));
      painter->setBrush(Qt::NoBrush);
      painter->drawPolygon((pieces.at(rig.name) * world)
                               .map(QPolygonF(QRectF(QPointF(),
                                                     QSizeF(art->size)))));
    }
    const double radius = is_picked ? kJointRadius * 1.6 : kJointRadius;
    painter->setPen(QPen(theme::kShadow, 1.0));
    painter->setBrush(theme::kHandle);
    painter->drawEllipse(joint(rig), radius, radius);
  }
  const bool shows_hang = is_hanging_ && pieces.contains(piece_);
  if (shows_hang) {
    painter->setPen(QPen(theme::kPick, 2.0, Qt::DashLine));
    painter->drawLine(joint(*FindRig(doll.rig, piece_)), cursor_);
  }
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kHandle);
  for (const IkChain& chain : doll.rig.chains) {
    const bool is_placed = pieces.contains(chain.lower);
    if (is_placed) {
      const QPointF tip = (pieces.at(chain.lower) * world).map(chain.tip);
      painter->drawRect(QRectF(tip - QPointF(4, 4), QSizeF(8, 8)));
    }
  }
}

}  // namespace snapper
