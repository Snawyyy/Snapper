// CharacterCanvas's shaping tools: Line width, Delete edge and Magnet.

#include <QMouseEvent>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "edit/character_manager.h"
#include "edit/vector_reshape.h"
#include "ui/grab.h"
#include "ui/character_canvas.h"

namespace snapper {
namespace {

// Widget pixels of drag that double a line's width.
constexpr double kWidthPixels = 120.0;
// The widest a point's line may be, times the shape's width.
constexpr double kMaxWidthShare = 10.0;

}  // namespace

void CharacterCanvas::PressLineWidth() {
  assert(session_ != nullptr);
  assert(characters_ != nullptr);
  const Character* character = Shown();
  const CharacterPart* part = FindPart(*character, session_->part());
  const PointId under = PointAt(Posed(), press_);
  if (under.IsValid() && !session_->points().contains(under)) {
    session_->SetPoints({under});
  }
  if (part == nullptr || part->kind != PartKind::kVector ||
      session_->points().empty()) {
    Report(tr("Press on a point of a drawing to change the line's width "
              "there."));
    return;
  }
  const VectorPoint* first =
      FindPoint(part->drawing, *session_->points().begin());
  start_value_ = first != nullptr ? first->width : 1.0;
  drag_ = Drag::kLineWidth;
  OpenScope(tr("Line width"));
}

void CharacterCanvas::MoveLineWidth(QPointF at, Qt::KeyboardModifiers mods) {
  assert(std::isfinite(at.y()));
  assert(characters_ != nullptr);
  // Up thickens, down thins, by doubling or halving per kWidthPixels.
  const double factor = std::pow(2.0, (press_.y() - at.y()) / kWidthPixels);
  double width = std::clamp(start_value_ * factor, 0.0, kMaxWidthShare);
  const bool is_stepped = mods.testFlag(Qt::ShiftModifier);
  if (is_stepped) {
    width = std::round(width * 4.0) / 4.0;
  }
  const std::vector<PointId> ids(session_->points().begin(),
                                 session_->points().end());
  const auto done = characters_->SetWidth(session_->character(),
                                          session_->part(), ids, width);
  if (!done) {
    Report(done.error().message);
  }
}

void CharacterCanvas::ClickDeleteEdge() {
  assert(session_ != nullptr);
  assert(characters_ != nullptr);
  const Character* character = Shown();
  const CharacterPart* part = FindPart(*character, session_->part());
  const bool is_drawing = part != nullptr && part->kind == PartKind::kVector;
  if (!is_drawing && !PickPartAt(press_)) {
    Report(tr("Click a line of a drawing to cut it out."));
    return;
  }
  part = FindPart(*character, session_->part());
  const PosedCharacter posed = Posed();
  const PosedPart* shown = PickedShown(posed);
  const auto edge =
      shown != nullptr && part != nullptr && part->kind == PartKind::kVector
          ? EdgeAt(part->drawing, shown->points, ToCharacter(press_),
                   kGrabReach / Scale())
          : std::nullopt;
  if (!edge.has_value()) {
    Report(tr("Click right on a line of the picked drawing to cut it."));
    return;
  }
  const auto done =
      characters_->DeleteEdge(session_->character(), part->id, *edge);
  session_->SetPoints({});
  session_->SetShape(-1);
  Report(done ? QString() : done.error().message);
}

void CharacterCanvas::PressMagnet() {
  assert(session_ != nullptr);
  assert(characters_ != nullptr);
  const Character* character = Shown();
  const CharacterPart* part = FindPart(*character, session_->part());
  const bool is_drawing = part != nullptr && part->kind == PartKind::kVector;
  if (!is_drawing && !PickPartAt(press_)) {
    Report(tr("Press on a drawing and drag to pull its points."));
    return;
  }
  part = FindPart(*character, session_->part());
  const PosedCharacter posed = Posed();
  const PosedPart* shown = PickedShown(posed);
  if (shown == nullptr || part == nullptr ||
      part->kind != PartKind::kVector) {
    Report(tr("Press on a drawing and drag to pull its points."));
    return;
  }
  magnet_ = MagnetWeights(part->drawing, shown->points, ToCharacter(press_),
                          session_->magnet_radius() / Scale());
  if (magnet_.empty()) {
    Report(tr("No point is within the magnet's reach; [ and ] change it."));
    return;
  }
  drag_ = Drag::kMagnet;
  OpenScope(tr("Magnet"));
}

void CharacterCanvas::MoveMagnet(QPointF at) {
  assert(std::isfinite(at.x()));
  assert(characters_ != nullptr);
  const QPointF step = IntoPart(session_->part(), last_, at);
  std::vector<std::pair<PointId, QPointF>> moves;
  moves.reserve(magnet_.size());
  for (const auto& [id, weight] : magnet_) {
    moves.emplace_back(id, step * weight);
  }
  const auto done =
      characters_->ShiftPoints(session_->character(), session_->part(), moves);
  if (!done) {
    Report(done.error().message);
  }
}

}  // namespace snapper
