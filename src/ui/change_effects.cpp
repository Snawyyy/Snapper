#include "ui/change_effects.h"

#include <algorithm>
#include <cassert>
#include <utility>

#include "ui/motion.h"

namespace snapper {
namespace {

// A paste of a whole song's keys still plays; past this the rest just
// appear.
constexpr size_t kMaxEffects = 256;

// What an edit plays: things growing in, and things puffing out.
struct Plan final {
  std::vector<ChangeEffect> grow;
  std::vector<ChangeEffect> puff;
  std::vector<ChangeEffect> slide;
};

ChangeEffect KeyEffect(ChangeEffectKind kind, LayerId layer, Frame frame) {
  assert(layer.value() >= 0);
  assert(frame.index() >= 0);
  ChangeEffect effect;
  effect.kind = kind;
  effect.lane = layer.value();
  effect.frame = frame;
  return effect;
}

ChangeEffect ClipEffect(ChangeEffectKind kind, const ClipChange& change) {
  assert(change.clip.IsValid());
  assert(change.track >= 0);
  ChangeEffect effect;
  effect.kind = kind;
  effect.lane = change.track;
  effect.item = change.clip.value();
  effect.frame = change.start;
  effect.length = change.length;
  return effect;
}

}  // namespace

double PopScale(double t) {
  assert(t >= 0.0);
  assert(t < 10.0);
  return 1.4 - 0.4 * t;
}

double RewindScale(double t) {
  assert(t >= 0.0);
  assert(t < 10.0);
  return 1.25 - 0.25 * t;
}

double PuffScale(double t) {
  assert(t >= 0.0);
  assert(t < 10.0);
  return std::max(0.0, 1.0 - t);
}

double PuffOpacity(double t) {
  assert(t >= 0.0);
  assert(t < 10.0);
  return std::clamp(1.0 - t, 0.0, 1.0);
}

double RewindGlow(double t) {
  assert(t >= 0.0);
  assert(t < 10.0);
  return std::clamp(1.0 - t, 0.0, 1.0);
}

ChangeEffects::ChangeEffects(MotionManager* motion, QObject* parent)
    : QObject(parent), motion_(motion) {
  assert(motion_ != nullptr);
  assert(parent != nullptr);
}

void ChangeEffects::ShowKeys(const KeyChanges& changes, bool is_travel) {
  assert(motion_ != nullptr);
  assert(!is_travel || changes.removed.size() < 100000);
  const bool is_off = motion_->mode() == MotionMode::kOff;
  if (is_off) {
    return;
  }
  // Undo and redo rewind what they bring back; a fresh edit pops it.
  const ChangeEffectKind arrival =
      is_travel ? ChangeEffectKind::kRewind : ChangeEffectKind::kPop;
  Plan plan;
  for (const KeyChange& key : changes.added) {
    plan.grow.push_back(KeyEffect(arrival, key.layer, key.frame));
  }
  for (const KeyMove& move : changes.moved) {
    ChangeEffect slide =
        KeyEffect(ChangeEffectKind::kSlide, move.layer, move.to);
    slide.from = move.from;
    plan.slide.push_back(slide);
  }
  for (const KeyChange& key : changes.removed) {
    plan.puff.push_back(
        KeyEffect(ChangeEffectKind::kPuff, key.layer, key.frame));
  }
  Start(std::move(plan.grow), true);
  Start(std::move(plan.puff), false);
  Start(std::move(plan.slide), false);
}

void ChangeEffects::ShowClips(const ClipChanges& changes, bool is_travel) {
  assert(motion_ != nullptr);
  assert(!is_travel || changes.removed.size() < 100000);
  const bool is_off = motion_->mode() == MotionMode::kOff;
  if (is_off) {
    return;
  }
  const ChangeEffectKind arrival =
      is_travel ? ChangeEffectKind::kRewind : ChangeEffectKind::kPop;
  Plan plan;
  for (const ClipChange& clip : changes.added) {
    plan.grow.push_back(ClipEffect(arrival, clip));
  }
  for (const ClipChange& clip : changes.moved) {
    ChangeEffect slide = ClipEffect(ChangeEffectKind::kSlide, clip);
    slide.from = clip.from_start;
    slide.from_lane = clip.from_track;
    plan.slide.push_back(slide);
  }
  for (const ClipChange& clip : changes.removed) {
    plan.puff.push_back(ClipEffect(ChangeEffectKind::kPuff, clip));
  }
  Start(std::move(plan.grow), true);
  Start(std::move(plan.puff), false);
  Start(std::move(plan.slide), false);
}

void ChangeEffects::Start(std::vector<ChangeEffect> effects,
                          bool is_overshoot) {
  assert(motion_ != nullptr);
  assert(effects_.size() <= kMaxEffects);
  const int batch = next_batch_++;
  const size_t room = kMaxEffects - effects_.size();
  const size_t count = std::min(room, effects.size());
  const bool is_empty = count == 0;
  if (is_empty) {
    return;
  }
  for (size_t i = 0; i < count; ++i) {
    effects[i].batch = batch;
    effects_.push_back(effects[i]);
  }
  const MotionKind kind = is_overshoot ? MotionKind::kPop : MotionKind::kMove;
  const auto animation = motion_->Animate(
      this, kind, 0.0, 1.0, [this, batch](const QVariant& value) {
        SetProgress(batch, value.toDouble());
      });
  const bool is_running = animation != nullptr;
  if (is_running) {
    connect(animation, &QAbstractAnimation::finished, this,
            [this, batch] { Finish(batch); });
  } else {
    Finish(batch);
  }
}

void ChangeEffects::SetProgress(int batch, double t) {
  assert(batch > 0);
  assert(effects_.size() <= kMaxEffects);
  for (ChangeEffect& effect : effects_) {
    const bool is_in_batch = effect.batch == batch;
    if (is_in_batch) {
      effect.t = std::max(0.0, t);
    }
  }
  emit Changed();
}

void ChangeEffects::Finish(int batch) {
  assert(batch > 0);
  assert(effects_.size() <= kMaxEffects);
  std::erase_if(effects_, [batch](const ChangeEffect& effect) {
    return effect.batch == batch;
  });
  emit Changed();
}

const ChangeEffect* ChangeEffects::Find(int lane, int item,
                                        Frame frame) const {
  assert(lane >= 0);
  assert(effects_.size() <= kMaxEffects);
  for (const ChangeEffect& effect : effects_) {
    const bool is_match = effect.kind != ChangeEffectKind::kPuff &&
                          effect.lane == lane && effect.item == item &&
                          effect.frame == frame;
    if (is_match) {
      return &effect;
    }
  }
  return nullptr;
}

double ChangeEffects::ScaleOf(int lane, int item, Frame frame) const {
  assert(lane >= 0);
  assert(item >= 0);
  const ChangeEffect* effect = Find(lane, item, frame);
  const bool is_playing = effect != nullptr;
  if (!is_playing) {
    return 1.0;
  }
  const bool is_slide = effect->kind == ChangeEffectKind::kSlide;
  if (is_slide) {
    return 1.0;
  }
  const bool is_pop = effect->kind == ChangeEffectKind::kPop;
  return is_pop ? PopScale(effect->t) : RewindScale(effect->t);
}

double ChangeEffects::GlowOf(int lane, int item, Frame frame) const {
  assert(lane >= 0);
  assert(item >= 0);
  const ChangeEffect* effect = Find(lane, item, frame);
  const bool is_rewinding =
      effect != nullptr && effect->kind == ChangeEffectKind::kRewind;
  return is_rewinding ? RewindGlow(effect->t) : 0.0;
}

std::optional<ChangeEffect> ChangeEffects::SlideOf(int lane, int item,
                                                   Frame frame) const {
  assert(lane >= 0);
  assert(frame.index() >= 0);
  const ChangeEffect* effect = Find(lane, item, frame);
  const bool is_sliding =
      effect != nullptr && effect->kind == ChangeEffectKind::kSlide;
  if (!is_sliding) {
    return std::nullopt;
  }
  return *effect;
}

double ChangeEffects::SlideOffsetOf(int lane, int item, Frame frame) const {
  assert(lane >= 0);
  assert(item >= 0);
  const auto slide = SlideOf(lane, item, frame);
  const bool is_sliding = slide.has_value();
  if (!is_sliding) {
    return 0.0;
  }
  return (slide->from.index() - frame.index()) * (1.0 - slide->t);
}

std::optional<double> ChangeEffects::RingOf(int lane, int item,
                                            Frame frame) const {
  assert(lane >= 0);
  assert(item >= 0);
  const ChangeEffect* effect = Find(lane, item, frame);
  const bool is_popping =
      effect != nullptr && effect->kind == ChangeEffectKind::kPop;
  if (!is_popping) {
    return std::nullopt;
  }
  return std::clamp(effect->t, 0.0, 1.0);
}

std::vector<ChangeEffect> ChangeEffects::Puffs() const {
  assert(effects_.size() <= kMaxEffects);
  assert(next_batch_ > 0);
  std::vector<ChangeEffect> puffs;
  for (const ChangeEffect& effect : effects_) {
    const bool is_puff = effect.kind == ChangeEffectKind::kPuff;
    if (is_puff) {
      puffs.push_back(effect);
    }
  }
  return puffs;
}

}  // namespace snapper
