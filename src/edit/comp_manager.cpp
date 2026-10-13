#include "edit/comp_manager.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <utility>

#include "anim/comp_knobs.h"
#include "anim/links.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "render/frame_renderer.h"

namespace snapper {
namespace {

// A frame at 24 fps, in milliseconds.
constexpr double kFrameBudget = 1000.0 / kFramesPerSecond;

constexpr std::array<const char*, kCompKindCount> kKindNames = {
    "Line smoother", "Color swap",   "Transmitted light",
    "Rim light",     "Gradient",     "Motion blur",
    "Sparkle",       "Speed lines",  "Finish"};

constexpr std::array<const char*, kBlendModeCount> kBlendNames = {
    "Normal",     "Darken",     "Multiply",   "Color burn", "Linear burn",
    "Lighten",    "Screen",     "Color dodge", "Add",       "Overlay",
    "Soft light", "Hard light", "Difference", "Exclusion",  "Subtract",
    "Hue",        "Saturation", "Color",      "Luminosity"};

constexpr std::array<const char*, kGradientShapeCount> kShapeNames = {
    "Linear", "Radial", "Angle", "Diamond", "Four-corner", "From edge"};

// A new effect of kind, with its own look.
CompEffect MadeOf(CompKind kind) {
  assert(static_cast<int>(kind) < kCompKindCount);
  CompEffect effect;
  effect.kind = kind;
  switch (kind) {
    case CompKind::kTransmittedLight:
      effect.blend = BlendMode::kAdd;
      effect.color = QColor(255, 214, 140);
      break;
    case CompKind::kRimLight:
      effect.blend = BlendMode::kAdd;
      effect.color = QColor(255, 240, 210);
      break;
    case CompKind::kSparkle:
      effect.blend = BlendMode::kAdd;
      effect.color = Qt::white;
      break;
    case CompKind::kSpeedLines:
      effect.color = QColor(20, 20, 20);
      break;
    case CompKind::kGradient:
      effect.stops = {GradientStop{0.0, QColor(255, 150, 170, 200)},
                      GradientStop{1.0, QColor(255, 150, 170, 0)}};
      break;
    case CompKind::kLineSmoother:
    case CompKind::kColorSwap:
    case CompKind::kMotionBlur:
    case CompKind::kFinish:
      break;
  }
  return effect;
}

}  // namespace

CompManager::CompManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(!are_effects_off_);
}

QString CompManager::KindName(CompKind kind) {
  assert(static_cast<int>(kind) >= 0 &&
         static_cast<int>(kind) < kCompKindCount);
  return Tr(kKindNames[static_cast<size_t>(kind)]);
}

QString CompManager::KindHelp(CompKind kind) {
  assert(static_cast<int>(kind) < kCompKindCount);
  switch (kind) {
    case CompKind::kLineSmoother:
      return Tr("Smooths stair steps on lines without blurring. Pick line "
                "colors to touch only those.");
    case CompKind::kColorSwap:
      return Tr("Swaps picked colors for others: night scenes, hit "
                "flashes, line recolor.");
    case CompKind::kTransmittedLight:
      return Tr("Glow spreading out from a shape's edge, or into it: "
                "lamps, magic, backlight.");
    case CompKind::kRimLight:
      return Tr("A thin band of light on the edges facing the light.");
    case CompKind::kGradient:
      return Tr("A ramp over the target: blush on skin, sky fades, a dark "
                "corner.");
    case CompKind::kMotionBlur:
      return Tr("Smears the target in one direction with cartoon streaks.");
    case CompKind::kSparkle:
      return Tr("Star streaks and glow from the brightest parts.");
    case CompKind::kSpeedLines:
      return Tr("Burst lines around a point, or speed lines across.");
    case CompKind::kFinish:
      return Tr("Film grain, scanlines or posterize over everything.");
  }
  return QString();
}

QString CompManager::BlendName(BlendMode mode) {
  assert(static_cast<int>(mode) >= 0 &&
         static_cast<int>(mode) < kBlendModeCount);
  return Tr(kBlendNames[static_cast<size_t>(mode)]);
}

QString CompManager::ShapeName(GradientShape shape) {
  assert(static_cast<int>(shape) >= 0 &&
         static_cast<int>(shape) < kGradientShapeCount);
  return Tr(kShapeNames[static_cast<size_t>(shape)]);
}

QString CompManager::KnobLabel(CompKind kind, CompKnob knob) {
  const auto spec = KnobSpecOf(kind, knob);
  return spec ? Tr(spec->label) : QString();
}

std::vector<QString> CompManager::StyleNames(CompKind kind) {
  assert(static_cast<int>(kind) < kCompKindCount);
  switch (kind) {
    case CompKind::kTransmittedLight:
      return {Tr("Outside"), Tr("Inside")};
    case CompKind::kSpeedLines:
      return {Tr("Burst"), Tr("Speed")};
    case CompKind::kFinish:
      return {Tr("Grain"), Tr("Scanlines"), Tr("Posterize")};
    default:
      return {};
  }
}

bool CompManager::IsKnobShown(const CompEffect& effect, CompKnob knob) {
  const auto spec = KnobSpecOf(effect.kind, knob);
  const bool is_point = knob == CompKnob::kAx || knob == CompKnob::kAy ||
                        knob == CompKnob::kBx || knob == CompKnob::kBy;
  if (!spec.has_value() || is_point) {
    return false;
  }
  const bool is_lit_angle = knob == CompKnob::kAngle &&
                            effect.uses_shot_light && CanUseShotLight(effect);
  if (is_lit_angle) {
    return false;
  }
  switch (effect.kind) {
    case CompKind::kGradient: {
      const bool is_edge = effect.shape == GradientShape::kFromEdge;
      const bool is_corners = effect.shape == GradientShape::kFourCorner;
      return knob == CompKnob::kSize   ? is_edge
             : knob == CompKnob::kCount ? !is_corners
                                        : true;
    }
    case CompKind::kSpeedLines: {
      const bool is_burst = effect.style == 0;
      return knob == CompKnob::kAngle      ? !is_burst
             : knob == CompKnob::kSoftness ? is_burst
                                           : true;
    }
    case CompKind::kFinish: {
      const bool is_poster = effect.style == 2;
      const bool is_grain = effect.style == 0;
      return knob == CompKnob::kCount  ? is_poster
             : knob == CompKnob::kSize ? !is_poster
             : knob == CompKnob::kRate ? is_grain
                                       : true;
    }
    default:
      return true;
  }
}

bool CompManager::UsesColor(const CompEffect& effect) {
  return effect.kind == CompKind::kTransmittedLight ||
         effect.kind == CompKind::kRimLight ||
         effect.kind == CompKind::kSparkle ||
         effect.kind == CompKind::kSpeedLines;
}

bool CompManager::CanUseShotLight(const CompEffect& effect) {
  const bool is_rim = effect.kind == CompKind::kRimLight;
  const bool is_aimed_ramp = effect.kind == CompKind::kGradient &&
                             (effect.shape == GradientShape::kLinear ||
                              effect.shape == GradientShape::kRadial);
  return is_rim || is_aimed_ramp;
}

std::vector<CompEffect> CompManager::Of(ShotId shot) const {
  assert(history_ != nullptr);
  const Shot* found = FindShot(history_->current(), shot);
  return found != nullptr ? found->comp : std::vector<CompEffect>();
}

const CompEffect* CompManager::Find(ShotId shot, CompEffectId id) const {
  assert(history_ != nullptr);
  const Shot* found = FindShot(history_->current(), shot);
  return found != nullptr ? FindCompEffect(found->comp, id) : nullptr;
}

QString CompManager::WhyNoAdd(ShotId shot) const {
  assert(history_ != nullptr);
  const Shot* found = FindShot(history_->current(), shot);
  const bool is_missing = found == nullptr;
  if (is_missing) {
    return Tr("Pick a shot first.");
  }
  const bool is_full =
      found->comp.size() >= static_cast<size_t>(kMaxCompEffectsPerShot);
  return is_full ? Tr("A shot holds at most %1 effects.")
                       .arg(kMaxCompEffectsPerShot)
                 : QString();
}

Result<CompEffectId> CompManager::Insert(ShotId shot, CompEffect effect,
                                         int index, const QString& label) {
  assert(history_ != nullptr);
  assert(!label.isEmpty());
  const QString why = WhyNoAdd(shot);
  const bool can_add = why.isEmpty();
  if (!can_add) {
    return std::unexpected(Error{why});
  }
  CompEffectId made;
  const auto next = WithShot(
      history_->current(), shot, [&](Shot* edited) -> Result<void> {
        effect.id = NextCompEffectId(edited->comp);
        made = effect.id;
        const int at = std::clamp(index, 0,
                                  static_cast<int>(edited->comp.size()));
        edited->comp.insert(edited->comp.begin() + at, std::move(effect));
        return {};
      });
  const auto done = history_->Apply(label, next);
  if (!done) {
    return std::unexpected(done.error());
  }
  Pick(shot, made);
  assert(made.IsValid());
  return made;
}

Result<CompEffectId> CompManager::Add(ShotId shot, CompKind kind) {
  assert(static_cast<int>(kind) < kCompKindCount);
  const int top = static_cast<int>(Of(shot).size());
  return Insert(shot, MadeOf(kind), top,
                Tr("Add %1").arg(KindName(kind)));
}

Result<CompEffectId> CompManager::AddSceneGradient(ShotId shot) {
  assert(history_ != nullptr);
  CompEffect effect = MadeOf(CompKind::kGradient);
  effect.blend = BlendMode::kOverlay;
  effect.stops = {GradientStop{0.0, QColor(255, 170, 90)},
                  GradientStop{1.0, QColor(60, 80, 180)}};
  const auto& canvas = history_->current().canvas;
  const double half = canvas.height / 2.0;
  SetKey(&effect.knobs[static_cast<size_t>(CompKnob::kAy)],
         Key<double>{Frame(0), -half, Ease::kStep});
  SetKey(&effect.knobs[static_cast<size_t>(CompKnob::kBy)],
         Key<double>{Frame(0), half, Ease::kStep});
  const int top = static_cast<int>(Of(shot).size());
  return Insert(shot, std::move(effect), top, Tr("Add Scene gradient"));
}

Result<void> CompManager::Remove(ShotId shot, CompEffectId id) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  const CompEffect* effect = Find(shot, id);
  const bool is_present = effect != nullptr;
  if (!is_present) {
    return std::unexpected(Error{Tr("That effect no longer exists.")});
  }
  const QString label = Tr("Remove %1").arg(KindName(effect->kind));
  const bool was_picked = PickedIn(shot) == id;
  const auto next = WithShot(history_->current(), shot,
                             [id](Shot* edited) -> Result<void> {
                               std::erase_if(edited->comp,
                                             [id](const CompEffect& one) {
                                               return one.id == id;
                                             });
                               return {};
                             });
  const int index = CompEffectIndex(Of(shot), id);
  const auto done = history_->Apply(label, next);
  // The pick moves to the effect below, or the new bottom one.
  const std::vector<CompEffect> left = Of(shot);
  const bool can_repick = done.has_value() && was_picked && !left.empty();
  if (can_repick) {
    const int below = std::clamp(index - 1, 0, static_cast<int>(left.size()) - 1);
    Pick(shot, left[static_cast<size_t>(below)].id);
  }
  return done;
}

Result<CompEffectId> CompManager::Duplicate(ShotId shot, CompEffectId id) {
  assert(history_ != nullptr);
  const CompEffect* effect = Find(shot, id);
  const bool is_present = effect != nullptr;
  if (!is_present) {
    return std::unexpected(Error{Tr("That effect no longer exists.")});
  }
  const int index = CompEffectIndex(Of(shot), id);
  assert(index >= 0);
  return Insert(shot, *effect, index + 1,
                Tr("Duplicate %1").arg(KindName(effect->kind)));
}

Result<void> CompManager::Move(ShotId shot, CompEffectId id, int index) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  const auto next = WithShot(
      history_->current(), shot, [&](Shot* edited) -> Result<void> {
        const int from = CompEffectIndex(edited->comp, id);
        const bool is_present = from >= 0;
        if (!is_present) {
          return std::unexpected(Error{Tr("That effect no longer exists.")});
        }
        const int to =
            std::clamp(index, 0, static_cast<int>(edited->comp.size()) - 1);
        CompEffect moved = edited->comp[static_cast<size_t>(from)];
        edited->comp.erase(edited->comp.begin() + from);
        edited->comp.insert(edited->comp.begin() + to, std::move(moved));
        return {};
      });
  return history_->Apply(Tr("Reorder effects"), next);
}

Result<void> CompManager::SetOn(ShotId shot, CompEffectId id, bool is_on) {
  return Edit(shot, id, is_on ? Tr("Show effect") : Tr("Hide effect"),
              [is_on](CompEffect* effect) -> Result<void> {
                effect->is_on = is_on;
                return {};
              });
}

Result<void> CompManager::SetBlend(ShotId shot, CompEffectId id,
                                   BlendMode mode) {
  assert(static_cast<int>(mode) >= 0 &&
         static_cast<int>(mode) < kBlendModeCount);
  return Edit(shot, id, Tr("Blend %1").arg(BlendName(mode)),
              [mode](CompEffect* effect) -> Result<void> {
                effect->blend = mode;
                return {};
              });
}

Result<void> CompManager::StepBlend(ShotId shot, CompEffectId id, int delta) {
  assert(delta != 0);
  const CompEffect* effect = Find(shot, id);
  const bool is_present = effect != nullptr;
  if (!is_present) {
    return std::unexpected(Error{Tr("Pick an effect first.")});
  }
  const int next = ((static_cast<int>(effect->blend) + delta) %
                        kBlendModeCount +
                    kBlendModeCount) %
                   kBlendModeCount;
  return SetBlend(shot, id, static_cast<BlendMode>(next));
}

QTransform CompManager::PointSpace(ShotId shot, CompEffectId id, Frame frame,
                                   double scale) const {
  assert(history_ != nullptr);
  assert(scale > 0.0);
  const Project& project = history_->current();
  const Shot* found = FindShot(project, shot);
  const CompEffect* effect = Find(shot, id);
  const bool is_present = found != nullptr && effect != nullptr;
  if (!is_present) {
    return QTransform();
  }
  LinkSolver links(project, *found);
  const CompPictures none;
  const CompContext context{
      &project, found, frame, scale,
      FrameRenderer::ViewTransform(project, *found, frame, scale), &links,
      &none, CompShow()};
  return CompSpace(context, *effect);
}

CompEffectId CompManager::PickedIn(ShotId shot) const {
  const auto found = picked_.find(shot);
  const bool is_picked =
      found != picked_.end() && Find(shot, found->second) != nullptr;
  return is_picked ? found->second : CompEffectId();
}

void CompManager::Pick(ShotId shot, CompEffectId id) {
  assert(shot.value() >= 0);
  assert(id.value() >= 0);
  const bool is_same = PickedIn(shot) == id;
  if (is_same) {
    return;
  }
  picked_[shot] = id;
  emit Changed();
}

void CompManager::SetEffectsOff(bool is_off) {
  const bool is_same = are_effects_off_ == is_off;
  if (is_same) {
    return;
  }
  are_effects_off_ = is_off;
  emit Changed();
}

void CompManager::SetMaskShown(bool is_shown) {
  const bool is_same = is_mask_shown_ == is_shown;
  if (is_same) {
    return;
  }
  is_mask_shown_ = is_shown;
  emit Changed();
}

CompShow CompManager::ShowFor(ShotId shot) const {
  CompShow show;
  show.is_off = are_effects_off_;
  show.mask_of = is_mask_shown_ ? PickedIn(shot) : CompEffectId();
  return show;
}

void CompManager::NoteCosts(const std::map<CompEffectId, double>& costs) {
  const auto slow = [](const std::map<CompEffectId, double>& all) {
    std::set<CompEffectId> ids;
    for (const auto& [id, ms] : all) {
      if (ms > kFrameBudget) {
        ids.insert(id);
      }
    }
    return ids;
  };
  const bool is_changed = slow(costs) != slow(costs_);
  costs_ = costs;
  if (is_changed) {
    emit Changed();
  }
}

bool CompManager::IsSlow(CompEffectId id) const {
  const auto found = costs_.find(id);
  return found != costs_.end() && found->second > kFrameBudget;
}

}  // namespace snapper
