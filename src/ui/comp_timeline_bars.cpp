// CompTimeline: the effect clips, where they snap, and dragging them.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>
#include <variant>

#include "anim/comp_reel.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "ui/comp_timeline.h"

namespace snapper {
namespace {

// How near a snap catches, in pixels.
constexpr double kSnap = 6.0;

}  // namespace

const Clip* CompTimeline::ClipOf(ShotId shot) const {
  assert(managers_.history != nullptr);
  assert(shot.value() >= 0);
  return FirstClipOf(managers_.history->current(), shot);
}

std::vector<CompTimeline::Bar> CompTimeline::Bars() const {
  assert(managers_.comp != nullptr);
  assert(managers_.history != nullptr);
  const Project& project = managers_.history->current();
  std::vector<std::pair<const Clip*, const Shot*>> shots;
  for (const auto& shot : project.shots) {
    const Clip* clip = ClipOf(shot->id);
    const bool is_on_reel = clip != nullptr && !shot->comp.empty();
    if (is_on_reel) {
      shots.emplace_back(clip, shot.get());
    }
  }
  std::ranges::sort(shots, [](const auto& a, const auto& b) {
    return a.first->start.index() < b.first->start.index();
  });
  std::vector<Bar> bars;
  double y = kRuler + kRow;
  for (const auto& [clip, shot] : shots) {
    for (const CompEffect& effect : shot->comp) {
      Bar bar;
      bar.ref = CompClipRef{shot->id, effect.id};
      bar.clip = clip;
      bar.offset = clip->start.index() - clip->in.index();
      bar.is_on_doll = effect.target.layer.IsValid();
      const bool is_whole = effect.length == kWholeShot;
      bar.first = is_whole ? clip->in.index() : effect.start;
      bar.end = is_whole ? clip->in.index() + clip->length.index()
                         : effect.start + effect.length;
      const double left = XOf(bar.first + bar.offset);
      const double right = XOf(bar.end + bar.offset);
      bar.box = QRectF(left, y + 1, std::max(4.0, right - left), kRow - 2);
      bars.push_back(bar);
      y += kRow;
    }
  }
  return bars;
}

std::optional<CompTimeline::Bar> CompTimeline::BarAt(QPointF at) const {
  assert(std::isfinite(at.x()));
  assert(std::isfinite(at.y()));
  for (const Bar& bar : Bars()) {
    const bool is_on = bar.box.adjusted(-3, 0, 3, 0).contains(at);
    if (is_on) {
      return bar;
    }
  }
  return std::nullopt;
}

std::vector<int> CompTimeline::SnapTargets() const {
  assert(managers_.playback != nullptr);
  assert(managers_.history != nullptr);
  const Project& project = managers_.history->current();
  std::vector<int> targets{
      managers_.playback->FrameOn(Timeline::kReel).index()};
  for (const ReelTrack& track : project.reel.tracks) {
    const bool is_picture = !track.is_sound && !track.is_subtitle;
    for (const Clip& clip : track.clips) {
      if (is_picture) {
        targets.push_back(clip.start.index());
        targets.push_back(clip.end().index());
      }
    }
  }
  for (const Frame& marker : project.reel.markers) {
    targets.push_back(marker.index());
  }
  for (const Bar& bar : Bars()) {
    const bool is_moving = from_.contains(bar.ref);
    if (!is_moving) {
      targets.push_back(bar.first + bar.offset);
      targets.push_back(bar.end + bar.offset);
    }
  }
  return targets;
}

std::optional<int> CompTimeline::SnapOf(int frame) const {
  assert(PixelsPerFrame() > 0.0);
  assert(kSnap > 0.0);
  std::optional<int> best;
  double nearest = kSnap;
  for (const int target : SnapTargets()) {
    const double gap = std::abs(XOf(target) - XOf(frame));
    const bool is_nearer = gap <= nearest;
    if (is_nearer) {
      nearest = gap;
      best = target;
    }
  }
  return best;
}

void CompTimeline::StartDrag(const Bar& bar, QPointF at) {
  assert(managers_.comp != nullptr);
  assert(std::isfinite(at.x()));
  constexpr double kEdge = 6.0;
  const bool is_start = at.x() <= bar.box.left() + kEdge;
  const bool is_end = at.x() >= bar.box.right() - kEdge;
  grab_ = is_start ? Grab::kStart : is_end ? Grab::kEnd : Grab::kMove;
  grabbed_ = bar.ref;
  press_frame_ = FreeFrameAt(at.x());
  press_at_ = at;
  has_moved_ = false;
  from_.clear();
  const std::set<CompClipRef> picks = managers_.comp->ClipPicks();
  for (const Bar& one : Bars()) {
    const bool is_carried = picks.contains(one.ref) || one.ref == bar.ref;
    if (is_carried) {
      from_[one.ref] = Span{one.first, one.end, one.offset};
    }
  }
}

void CompTimeline::DragTo(QPointF at, bool is_free) {
  assert(std::isfinite(at.x()));
  assert(managers_.history != nullptr);
  const bool is_far =
      has_moved_ || std::abs(at.x() - press_at_.x()) >= 3.0;
  if (!is_far) {
    return;
  }
  has_moved_ = true;
  narrows_on_release_ = false;
  const bool needs_scope = drag_ == nullptr;
  if (needs_scope) {
    drag_ = std::make_unique<EditScope>(
        managers_.history,
        grab_ == Grab::kMove ? tr("Move effects") : tr("Trim effect"));
  }
  const int delta = FreeFrameAt(at.x()) - press_frame_;
  const bool is_move = grab_ == Grab::kMove;
  if (is_move) {
    MovePicked(delta, is_free);
  } else {
    TrimGrabbed(delta, is_free);
  }
  update();
}

void CompTimeline::MovePicked(int delta, bool is_free) {
  assert(from_.contains(grabbed_));
  assert(managers_.comp != nullptr);
  const Span& grabbed = from_.at(grabbed_);
  snap_.reset();
  if (!is_free) {
    const int first = grabbed.first + grabbed.offset + delta;
    const int end = grabbed.end + grabbed.offset + delta;
    const std::optional<int> by_first = SnapOf(first);
    const std::optional<int> by_end = SnapOf(end);
    const bool has_first = by_first.has_value();
    const bool has_end = by_end.has_value();
    snap_ = has_first ? by_first : by_end;
    delta += has_first ? *by_first - first : has_end ? *by_end - end : 0;
  }
  delta = KeptOnReel(from_, delta);
  for (const auto& [ref, span] : from_) {
    static_cast<void>(managers_.comp->SetTiming(
        ref.shot, ref.id, span.first + delta, span.end - span.first, true));
  }
}

void CompTimeline::TrimGrabbed(int delta, bool is_free) {
  assert(from_.contains(grabbed_));
  assert(managers_.comp != nullptr);
  const Span& span = from_.at(grabbed_);
  const bool is_start = grab_ == Grab::kStart;
  int edge = (is_start ? span.first : span.end) + delta;
  snap_.reset();
  if (!is_free) {
    snap_ = SnapOf(edge + span.offset);
    const bool has_snap = snap_.has_value();
    edge = has_snap ? *snap_ - span.offset : edge;
  }
  // An end stays on the reel, frame 0 to its end.
  edge = std::clamp(edge, -span.offset, Length() - span.offset);
  const int first = is_start ? std::min(span.end - 1, edge) : span.first;
  const int end = is_start ? span.end : std::max(span.first + 1, edge);
  static_cast<void>(managers_.comp->SetTiming(grabbed_.shot, grabbed_.id,
                                              first, end - first, false));
}

int CompTimeline::KeptOnReel(const std::map<CompClipRef, Span>& spans,
                             int delta) const {
  assert(Length() > 0);
  assert(spans.size() < 100000);
  int least = -kMaxFrame;
  int most = kMaxFrame;
  for (const auto& [ref, span] : spans) {
    least = std::max(least, -(span.first + span.offset));
    most = std::min(most, Length() - (span.end + span.offset));
  }
  return std::clamp(delta, least, std::max(least, most));
}

void CompTimeline::EndDrag() {
  assert(managers_.history != nullptr);
  assert(grab_ != Grab::kBox);
  // Leaving the scope lands the drag as one undo step.
  drag_.reset();
  grab_ = Grab::kNone;
  snap_.reset();
  from_.clear();
  update();
}

}  // namespace snapper
