#include "edit/comp_manager.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <utility>

#include "anim/links.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "render/frame_renderer.h"

namespace snapper {
namespace {

constexpr std::array<const char*, kCompKindCount> kKindNames = {
    "Line smoother", "Color swap",   "Transmitted light",
    "Rim light",     "Gradient",     "Motion blur",
    "Sparkle",       "Speed lines",  "Finish",
    "F's plugin"};

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
    case CompKind::kFsp:
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
  assert(static_cast<int>(kind) >= 0);
  assert(static_cast<int>(kind) < kCompKindCount);
  switch (kind) {
    case CompKind::kLineSmoother:
      return Tr("Smooths the stair steps of cel lines and fills without "
                "blurring them. Pick line colors to touch only those. "
                "Rebuilt from OLM Smoother v2's manual (Apache 2.0 tools "
                "by OLM Digital), not its code.");
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
    case CompKind::kFsp:
      return Tr("An effect from F's Plugins by bryful, the After Effects "
                "plugins' own code.");
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
        const int size = static_cast<int>(edited->comp.size());
        const bool is_top = index < 0;
        const int at = is_top ? size : std::min(index, size);
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

Result<CompEffectId> CompManager::Add(ShotId shot, CompKind kind,
                                      const CompTarget& target, int index) {
  assert(static_cast<int>(kind) < kCompKindCount);
  assert(target.layer.value() >= 0);
  CompEffect effect = MadeOf(kind);
  effect.target = target;
  return Insert(shot, std::move(effect), index,
                Tr("Add %1").arg(KindName(kind)));
}

Result<CompEffectId> CompManager::AddSceneGradient(ShotId shot,
                                                   const CompTarget& target,
                                                   int index) {
  assert(history_ != nullptr);
  assert(target.layer.value() >= 0);
  CompEffect effect = MadeOf(CompKind::kGradient);
  effect.target = target;
  effect.blend = BlendMode::kOverlay;
  effect.stops = {GradientStop{0.0, QColor(255, 170, 90)},
                  GradientStop{1.0, QColor(60, 80, 180)}};
  const auto& canvas = history_->current().canvas;
  const double half = canvas.height / 2.0;
  SetKey(&effect.knobs[static_cast<size_t>(CompKnob::kAy)],
         Key<double>{Frame(0), -half, Ease::kStep});
  SetKey(&effect.knobs[static_cast<size_t>(CompKnob::kBy)],
         Key<double>{Frame(0), half, Ease::kStep});
  return Insert(shot, std::move(effect), index, Tr("Add Scene gradient"));
}

Result<void> CompManager::Remove(ShotId shot, CompEffectId id) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  const CompEffect* effect = Find(shot, id);
  const bool is_present = effect != nullptr;
  if (!is_present) {
    return std::unexpected(Error{Tr("That effect no longer exists.")});
  }
  const QString label = Tr("Remove %1").arg(EffectName(*effect));
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
                Tr("Duplicate %1").arg(EffectName(*effect)));
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

}  // namespace snapper
