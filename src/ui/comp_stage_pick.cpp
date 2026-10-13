#include "ui/comp_stage_pick.h"

#include <QPolygonF>

#include <cassert>
#include <cmath>
#include <set>

#include "edit/history_manager.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/pick_keys.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// A box smaller than this is a click on empty space, not a drag.
constexpr double kBoxPixels = 3.0;

}  // namespace

CompStagePick::CompStagePick(const Managers& managers, ImageCache* cache)
    : managers_(managers), cache_(cache) {
  assert(managers_.selection != nullptr && managers_.history != nullptr);
  assert(cache_ != nullptr);
}

void CompStagePick::Press(QPointF point, Qt::KeyboardModifiers mods,
                          const CompStageSpot& spot) {
  assert(spot.scale > 0.0);
  assert(managers_.history != nullptr);
  Cancel();
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, spot.at.shot);
  const bool is_on_shot = spot.at.IsValid() && shot != nullptr;
  if (!is_on_shot) {
    return;
  }
  const auto hit = HitTest(project, *shot, spot.at.local,
                           point - spot.corner, spot.scale, cache_);
  const bool is_hit = hit.has_value();
  if (!is_hit) {
    is_boxing_ = true;
    start_ = point;
    corner_ = point;
    box_mode_ = BoxMode(mods);
    return;
  }
  const Pick pick{hit->layer, hit->piece};
  const bool is_picked = managers_.selection->picks().contains(pick);
  const std::optional<PickMode> mode = ClickMode(mods, is_picked);
  if (mode) {
    managers_.selection->PickThings({pick}, *mode, pick.layer);
    return;
  }
  later_ = pick;
}

void CompStagePick::Move(QPointF point) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(std::isfinite(start_.x()) && std::isfinite(start_.y()));
  if (is_boxing_) {
    corner_ = point;
  }
}

bool CompStagePick::Release(QPointF point, const CompStageSpot& spot) {
  assert(spot.scale > 0.0);
  assert(managers_.selection != nullptr);
  const std::optional<Pick> later = later_;
  later_.reset();
  if (later) {
    managers_.selection->PickThings({*later}, PickMode::kReplace,
                                    later->layer);
    return true;
  }
  const bool was_boxing = is_boxing_;
  corner_ = point;
  const QRectF box = Box();
  is_boxing_ = false;
  if (!was_boxing) {
    return false;
  }
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, spot.at.shot);
  const bool is_drawn = shot != nullptr && box.width() > kBoxPixels &&
                        box.height() > kBoxPixels;
  std::set<Pick> caught;
  if (is_drawn) {
    for (const StageHit& hit : HitBox(project, *shot, spot.at.local,
                                      box.translated(-spot.corner),
                                      spot.scale, cache_)) {
      caught.insert({hit.layer, hit.piece});
    }
  }
  const LayerId focus = caught.empty() ? managers_.selection->layer()
                                       : caught.begin()->layer;
  managers_.selection->PickThings(caught, box_mode_, focus);
  return true;
}

void CompStagePick::Cancel() {
  assert(std::isfinite(start_.x()) && std::isfinite(corner_.x()));
  is_boxing_ = false;
  later_.reset();
  assert(!is_boxing_ && !later_.has_value());
}

QRectF CompStagePick::Box() const {
  assert(std::isfinite(start_.x()) && std::isfinite(start_.y()));
  assert(std::isfinite(corner_.x()) && std::isfinite(corner_.y()));
  return is_boxing_ ? QRectF(start_, corner_).normalized() : QRectF();
}

void CompStagePick::Paint(QPainter* painter, const CompStageSpot& spot) {
  assert(painter != nullptr);
  assert(spot.scale > 0.0);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, spot.at.shot);
  const bool is_on_shot = spot.at.IsValid() && shot != nullptr;
  if (!is_on_shot) {
    return;
  }
  painter->save();
  painter->translate(spot.corner);
  painter->setPen(QPen(theme::kPick, 2.0));
  painter->setBrush(Qt::NoBrush);
  for (const Pick& pick : managers_.selection->picks()) {
    const bool is_whole = pick.piece.isEmpty();
    const auto piece =
        is_whole ? std::nullopt
                 : PieceOnScreen(project, *shot, pick.layer, pick.piece,
                                 spot.at.local, spot.scale, cache_->poses());
    const QPolygonF outline =
        is_whole ? LayerShape(project, *shot, pick.layer, spot.at.local,
                              spot.scale, cache_)
                 : (piece ? piece->outline : QPolygonF());
    painter->drawPolygon(outline);
  }
  painter->restore();
  const QRectF box = Box();
  const bool has_box = !box.isEmpty();
  if (has_box) {
    painter->setPen(QPen(theme::kPick, 1.0, Qt::DashLine));
    painter->setBrush(QColor(theme::kPick.red(), theme::kPick.green(),
                             theme::kPick.blue(), 40));
    painter->drawRect(box);
  }
}

}  // namespace snapper
