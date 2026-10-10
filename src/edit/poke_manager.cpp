#include "edit/poke_manager.h"

#include <cassert>
#include <cmath>
#include <variant>

#include "anim/doll_lean.h"
#include "anim/poke_layer.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/project_edits.h"

namespace snapper {
namespace {

// A virtual frame where no dot moved more than this (drawing pixels)
// is calm; a few calm frames in a row after letting go is settled.
constexpr double kCalm = 0.02;
constexpr int kCalmTicks = 4;

}  // namespace

PokeManager::PokeManager(HistoryManager* history, PlaybackManager* playback)
    : history_(history), playback_(playback) {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  clock_.setTimerType(Qt::PreciseTimer);
  clock_.setInterval(1000 / kFramesPerSecond);
  connect(&clock_, &QTimer::timeout, this, &PokeManager::Tick);
  connect(history_, &HistoryManager::Changed, this,
          &PokeManager::OnProjectChanged);
  connect(playback_, &PlaybackManager::FrameChanged, this,
          &PokeManager::OnFrameChanged);
}

QString PokeManager::WhyNot(ShotId shot, LayerId layer) const {
  const Project& project = history_->current();
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  const Doll* doll =
      layer.IsValid() ? DollOfLayer(project, shot, layer) : nullptr;
  const bool is_doll = doll != nullptr;
  if (!is_doll) {
    return Tr("Hover a doll to poke it.");
  }
  bool has_springs = false;
  for (const RigPiece& piece : doll->rig.pieces) {
    has_springs = has_springs || !piece.drag_nodes.empty();
  }
  assert(doll->rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  return has_springs ? QString()
                     : Tr("This doll has no drag, cloth or jelly dots to "
                          "poke.");
}

QString PokeManager::Begin(ShotId shot, LayerId layer, Frame at,
                           QPointF point) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(at.index() >= 0);
  const QString why = WhyNot(shot, layer);
  const bool is_refused = !why.isEmpty();
  if (is_refused) {
    return why;
  }
  End();
  shot_ = shot;
  layer_ = layer;
  at_ = at;
  anchor_ = point;
  shove_ = QPointF();
  is_held_ = true;
  is_active_ = true;
  clock_.start();
  Tick();
  return QString();
}

void PokeManager::Move(QPointF point) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(is_active_ || !is_held_);
  const bool is_pushing = is_active_ && is_held_;
  if (is_pushing) {
    shove_ = PokeShove(point - anchor_);
  }
}

void PokeManager::Release() {
  assert(shoves_.size() <= static_cast<size_t>(kMaxPokeFrames));
  assert(is_active_ || !is_held_);
  is_held_ = false;
  shove_ = QPointF();
}

void PokeManager::End() {
  assert(shoves_.size() <= static_cast<size_t>(kMaxPokeFrames));
  assert(calm_ticks_ >= 0);
  const bool was_active = is_active_;
  clock_.stop();
  is_active_ = false;
  is_held_ = false;
  shove_ = QPointF();
  shoves_.clear();
  trail_.Clear();
  poses_.clear();
  calm_ticks_ = 0;
  if (was_active) {
    emit Changed();
  }
}

bool PokeManager::Solve() {
  assert(is_active_);
  const Project& project = history_->current();
  const Shot* shot = FindShot(project, shot_);
  const Layer* layer = shot != nullptr ? FindLayer(*shot, layer_) : nullptr;
  const Doll* doll = DollOfLayer(project, shot_, layer_);
  const bool is_gone = layer == nullptr || doll == nullptr;
  if (is_gone) {
    return false;
  }
  const Layer poke = PokeLayer(*layer, at_, shoves_);
  poses_ = ShownPoses(*doll, poke, Frame(at_.index() + ticks()), {}, &trail_);
  assert(poses_.size() <= static_cast<size_t>(kMaxPieceTracks));
  return true;
}

void PokeManager::Tick() {
  assert(calm_ticks_ >= 0);
  assert(shoves_.size() <= static_cast<size_t>(kMaxPokeFrames));
  if (!is_active_) {
    return;
  }
  const bool is_capped = ticks() >= kMaxPokeFrames;
  if (is_capped) {
    End();
    return;
  }
  const PoseMap before = poses_;
  shoves_.push_back(shove_);
  const bool is_solved = Solve();
  if (!is_solved) {
    End();
    return;
  }
  const bool is_calm = PoseDistance(poses_, before) < kCalm;
  calm_ticks_ = is_calm && !is_held_ ? calm_ticks_ + 1 : 0;
  const bool is_settled = calm_ticks_ >= kCalmTicks;
  if (is_settled) {
    End();
    return;
  }
  emit Changed();
}

void PokeManager::OnProjectChanged() {
  assert(history_ != nullptr);
  assert(shoves_.size() <= static_cast<size_t>(kMaxPokeFrames));
  if (!is_active_) {
    return;
  }
  // The doll or its settings changed: run the same poke again on it.
  trail_.Clear();
  const bool is_solved = Solve();
  if (is_solved) {
    emit Changed();
  } else {
    End();
  }
}

void PokeManager::OnFrameChanged() {
  assert(playback_ != nullptr);
  assert(shoves_.size() <= static_cast<size_t>(kMaxPokeFrames));
  End();
}

}  // namespace snapper
