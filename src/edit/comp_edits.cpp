#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

#include "anim/comp_knobs.h"
#include "base/text.h"
#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "edit/palette_manager.h"
#include "edit/project_edits.h"

namespace snapper {
namespace {

// Keys a change into channel: with no key or one it is the value for
// the whole shot; with more it is keyed at frame.
void Put(Channel<double>* channel, Frame frame, double value, Ease ease) {
  assert(channel != nullptr);
  assert(frame.index() >= 0);
  const bool is_flat = channel->keys.size() <= 1;
  if (is_flat) {
    channel->keys = {Key<double>{Frame(0), value, ease}};
    return;
  }
  SetKey(channel, Key<double>{frame, value, ease});
}

Ease EaseOf(bool is_eased) {
  return is_eased ? Ease::kEaseInOut : Ease::kStep;
}

bool SameColor(const QColor& a, const QColor& b) {
  return a.rgba() == b.rgba();
}

Result<void> Gone() {
  return std::unexpected(Error{Tr("That effect no longer exists.")});
}

// Stops spread evenly from 0 to 1, keeping their order.
void Spread(std::vector<GradientStop>* stops) {
  assert(stops != nullptr);
  const size_t count = stops->size();
  for (size_t i = 0; i < count; ++i) {
    (*stops)[i].at = count > 1 ? static_cast<double>(i) /
                                     static_cast<double>(count - 1)
                               : 0.0;
  }
}

}  // namespace

Result<void> CompManager::Edit(
    ShotId shot, CompEffectId id, const QString& label,
    const std::function<Result<void>(CompEffect*)>& change,
    const QColor& recent) {
  assert(history_ != nullptr);
  assert(!label.isEmpty());
  auto next = WithShot(history_->current(), shot,
                       [&](Shot* edited) -> Result<void> {
                         CompEffect* effect =
                             FindCompEffect(&edited->comp, id);
                         const bool is_present = effect != nullptr;
                         if (!is_present) {
                           return Gone();
                         }
                         return change(effect);
                       });
  const bool has_recent = next.has_value() && recent.isValid();
  if (has_recent) {
    next = PaletteManager::WithRecent(std::move(*next), recent);
  }
  return history_->Apply(label, std::move(next));
}

Result<void> CompManager::SetKnob(ShotId shot, CompEffectId id, CompKnob knob,
                                  Frame frame, double value) {
  assert(frame.index() >= 0);
  assert(static_cast<int>(knob) < kCompKnobCount);
  const CompEffect* effect = Find(shot, id);
  const bool is_present = effect != nullptr;
  if (!is_present) {
    return Gone();
  }
  const auto spec = KnobSpecOf(*effect, knob);
  const bool is_used = spec.has_value();
  if (!is_used) {
    return std::unexpected(Error{Tr("That effect has no such control.")});
  }
  const double kept = std::clamp(value, spec->low, spec->high);
  // A plugin's toggle, choice or color holds one value for the shot.
  const bool is_flat_only = !IsKeyable(*effect, knob);
  const QString label = QStringLiteral("%1 %2").arg(
      EffectName(*effect), KnobLabel(*effect, knob).toLower());
  return Edit(shot, id, label,
              [&](CompEffect* edited) -> Result<void> {
                auto* channel = &edited->knobs[static_cast<size_t>(knob)];
                if (is_flat_only) {
                  channel->keys = {Key<double>{Frame(0), kept, Ease::kStep}};
                  return {};
                }
                Put(channel, frame, kept, EaseOf(IsEased(*edited)));
                return {};
              });
}

Result<void> CompManager::SetPoint(ShotId shot, CompEffectId id, bool is_b,
                                   Frame frame, QPointF point) {
  assert(frame.index() >= 0);
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  const CompEffect* effect = Find(shot, id);
  const bool is_present = effect != nullptr;
  if (!is_present) {
    return Gone();
  }
  const CompKnob x_knob = is_b ? CompKnob::kBx : CompKnob::kAx;
  const CompKnob y_knob = is_b ? CompKnob::kBy : CompKnob::kAy;
  const auto spec = KnobSpecOf(*effect, x_knob);
  const bool is_used = spec.has_value();
  if (!is_used) {
    return std::unexpected(Error{Tr("That effect has no such point.")});
  }
  const QString label = Tr("Move %1 point").arg(EffectName(*effect));
  return Edit(shot, id, label, [&](CompEffect* edited) -> Result<void> {
    const Ease ease = EaseOf(IsEased(*edited));
    Put(&edited->knobs[static_cast<size_t>(x_knob)], frame,
        std::clamp(point.x(), spec->low, spec->high), ease);
    Put(&edited->knobs[static_cast<size_t>(y_knob)], frame,
        std::clamp(point.y(), spec->low, spec->high), ease);
    return {};
  });
}

Result<void> CompManager::ToggleKey(ShotId shot, CompEffectId id,
                                    CompKnob knob, Frame frame) {
  assert(frame.index() >= 0);
  assert(static_cast<int>(knob) < kCompKnobCount);
  const CompEffect* effect = Find(shot, id);
  const bool is_present = effect != nullptr;
  if (!is_present) {
    return Gone();
  }
  const bool can_key = IsKeyable(*effect, knob);
  if (!can_key) {
    return std::unexpected(
        Error{Tr("That control holds one value for the whole shot.")});
  }
  const bool is_keyed = HasKeyAt(shot, id, knob, frame);
  const QString label =
      (is_keyed ? Tr("Remove key %1") : Tr("Key %1"))
          .arg(KnobLabel(*effect, knob).toLower());
  const double value = KnobAt(*effect, knob, frame);
  return Edit(shot, id, label, [&](CompEffect* edited) -> Result<void> {
    auto& channel = edited->knobs[static_cast<size_t>(knob)];
    if (is_keyed) {
      RemoveKey(&channel, frame);
      return {};
    }
    const Ease ease = EaseOf(IsEased(*edited));
    // A first key also holds the start, so the knob moves from there.
    const bool is_first = channel.keys.empty() && frame.index() > 0;
    if (is_first) {
      SetKey(&channel, Key<double>{Frame(0), value, ease});
    }
    SetKey(&channel, Key<double>{frame, value, ease});
    return {};
  });
}

bool CompManager::HasKeyAt(ShotId shot, CompEffectId id, CompKnob knob,
                           Frame frame) const {
  const CompEffect* effect = Find(shot, id);
  const bool is_present = effect != nullptr;
  if (!is_present) {
    return false;
  }
  const auto& channel = effect->knobs[static_cast<size_t>(knob)];
  // A single key is the shot's value, not a key the user sees.
  return channel.keys.size() > 1 && KeyIndexAt(channel, frame) >= 0;
}

Result<void> CompManager::SetEased(ShotId shot, CompEffectId id,
                                   bool is_eased) {
  return Edit(shot, id, is_eased ? Tr("Ease keys") : Tr("Hold keys"),
              [is_eased](CompEffect* effect) -> Result<void> {
                for (auto& channel : effect->knobs) {
                  for (auto& key : channel.keys) {
                    key.ease = EaseOf(is_eased);
                  }
                }
                return {};
              });
}

bool CompManager::IsEased(const CompEffect& effect) {
  return std::ranges::any_of(effect.knobs, [](const Channel<double>& c) {
    return std::ranges::any_of(
        c.keys, [](const Key<double>& key) { return key.ease != Ease::kStep; });
  });
}

std::set<int> CompManager::KeyFrames(const CompEffect& effect) {
  std::set<int> frames;
  for (const auto& channel : effect.knobs) {
    const bool is_animated = channel.keys.size() > 1;
    if (!is_animated) {
      continue;
    }
    for (const auto& key : channel.keys) {
      frames.insert(key.frame.index());
    }
  }
  return frames;
}

Result<void> CompManager::SetColor(ShotId shot, CompEffectId id,
                                   QColor color) {
  assert(color.isValid());
  return Edit(
      shot, id, Tr("Effect color"),
      [color](CompEffect* effect) -> Result<void> {
        effect->color = color;
        return {};
      },
      color);
}

Result<void> CompManager::SetStyle(ShotId shot, CompEffectId id, int style) {
  const CompEffect* effect = Find(shot, id);
  if (effect == nullptr) {
    return Gone();
  }
  const int count = static_cast<int>(StyleNames(effect->kind).size());
  const bool is_known = style >= 0 && style < count;
  if (!is_known) {
    return std::unexpected(Error{Tr("That effect has no such look.")});
  }
  return Edit(shot, id,
              StyleNames(effect->kind)[static_cast<size_t>(style)],
              [style](CompEffect* edited) -> Result<void> {
                edited->style = style;
                return {};
              });
}

Result<void> CompManager::SetShape(ShotId shot, CompEffectId id,
                                   GradientShape shape) {
  assert(static_cast<int>(shape) < kGradientShapeCount);
  return Edit(shot, id, Tr("Gradient %1").arg(ShapeName(shape)),
              [shape](CompEffect* effect) -> Result<void> {
                effect->shape = shape;
                // Four corners need four colors.
                const bool needs_corners =
                    shape == GradientShape::kFourCorner;
                const QColor corner_colors[] = {
                    QColor(255, 120, 120), QColor(255, 220, 120),
                    QColor(120, 160, 255), QColor(160, 255, 160)};
                while (needs_corners && effect->stops.size() < 4) {
                  effect->stops.push_back(GradientStop{
                      1.0, corner_colors[effect->stops.size()]});
                }
                Spread(&effect->stops);
                return {};
              });
}

Result<void> CompManager::SetUsesShotLight(ShotId shot, CompEffectId id,
                                           bool is_used) {
  return Edit(shot, id, is_used ? Tr("Use shot light") : Tr("Own light"),
              [is_used](CompEffect* effect) -> Result<void> {
                effect->uses_shot_light = is_used;
                return {};
              });
}

Result<void> CompManager::SetStopColor(ShotId shot, CompEffectId id, int stop,
                                       QColor color) {
  assert(color.isValid());
  return Edit(
      shot, id, Tr("Gradient color"),
      [stop, color](CompEffect* effect) -> Result<void> {
        const bool is_known =
            stop >= 0 && stop < static_cast<int>(effect->stops.size());
        if (!is_known) {
          return std::unexpected(Error{Tr("That color stop is gone.")});
        }
        // The stop keeps its own opacity.
        QColor kept = color;
        kept.setAlpha(effect->stops[static_cast<size_t>(stop)].color.alpha());
        const bool is_clear = kept.alpha() == 0;
        if (is_clear) {
          kept.setAlpha(255);
        }
        effect->stops[static_cast<size_t>(stop)].color = kept;
        return {};
      },
      color);
}

QString CompManager::WhyNoAddStop(const CompEffect& effect) const {
  const bool is_ramp = effect.kind == CompKind::kGradient &&
                       effect.shape != GradientShape::kFourCorner;
  if (!is_ramp) {
    return Tr("Only a gradient ramp has color stops.");
  }
  const bool is_full =
      effect.stops.size() >= static_cast<size_t>(kMaxGradientStops);
  return is_full ? Tr("A gradient holds at most %1 colors.")
                       .arg(kMaxGradientStops)
                 : QString();
}

QString CompManager::WhyNoRemoveStop(const CompEffect& effect) const {
  const bool is_few = effect.stops.size() <= 2;
  const bool is_corners = effect.shape == GradientShape::kFourCorner;
  return is_corners ? Tr("Four-corner keeps one color per corner.")
         : is_few   ? Tr("A gradient needs at least two colors.")
                    : QString();
}

Result<void> CompManager::AddStop(ShotId shot, CompEffectId id) {
  const CompEffect* effect = Find(shot, id);
  if (effect == nullptr) {
    return Gone();
  }
  const QString why = WhyNoAddStop(*effect);
  if (!why.isEmpty()) {
    return std::unexpected(Error{why});
  }
  return Edit(shot, id, Tr("Add gradient color"),
              [](CompEffect* edited) -> Result<void> {
                auto& stops = edited->stops;
                const QColor middle =
                    stops.empty() ? QColor(Qt::gray)
                                  : stops[stops.size() / 2].color;
                const auto at = static_cast<std::ptrdiff_t>(stops.size() / 2);
                stops.insert(stops.begin() + at, GradientStop{0.5, middle});
                Spread(&stops);
                return {};
              });
}

Result<void> CompManager::RemoveStop(ShotId shot, CompEffectId id, int stop) {
  const CompEffect* effect = Find(shot, id);
  if (effect == nullptr) {
    return Gone();
  }
  const QString why = WhyNoRemoveStop(*effect);
  if (!why.isEmpty()) {
    return std::unexpected(Error{why});
  }
  return Edit(shot, id, Tr("Remove gradient color"),
              [stop](CompEffect* edited) -> Result<void> {
                const bool is_known =
                    stop >= 0 && stop < static_cast<int>(edited->stops.size());
                if (!is_known) {
                  return std::unexpected(Error{Tr("That color stop is gone.")});
                }
                edited->stops.erase(edited->stops.begin() + stop);
                Spread(&edited->stops);
                return {};
              });
}

QString CompManager::WhyNoAddSwap(const CompEffect& effect) const {
  const bool is_swap = effect.kind == CompKind::kColorSwap;
  if (!is_swap) {
    return Tr("Only a color swap has color pairs.");
  }
  const bool is_full =
      effect.swaps.size() >= static_cast<size_t>(kMaxCompColors);
  return is_full ? Tr("A color swap holds at most %1 pairs.")
                       .arg(kMaxCompColors)
                 : QString();
}

Result<void> CompManager::AddSwap(ShotId shot, CompEffectId id, QColor from) {
  const CompEffect* effect = Find(shot, id);
  if (effect == nullptr) {
    return Gone();
  }
  const QString why = WhyNoAddSwap(*effect);
  if (!why.isEmpty()) {
    return std::unexpected(Error{why});
  }
  const QColor start = from.isValid() ? from : QColor(Qt::black);
  return Edit(shot, id, Tr("Add color pair"),
              [start](CompEffect* edited) -> Result<void> {
                edited->swaps.push_back(ColorSwap{start, Qt::white});
                return {};
              });
}

Result<void> CompManager::SetSwapColor(ShotId shot, CompEffectId id, int swap,
                                       bool is_to, QColor color) {
  assert(color.isValid());
  return Edit(
      shot, id, is_to ? Tr("Swap to color") : Tr("Swap from color"),
      [swap, is_to, color](CompEffect* effect) -> Result<void> {
        const bool is_known =
            swap >= 0 && swap < static_cast<int>(effect->swaps.size());
        if (!is_known) {
          return std::unexpected(Error{Tr("That color pair is gone.")});
        }
        ColorSwap& pair = effect->swaps[static_cast<size_t>(swap)];
        (is_to ? pair.to : pair.from) = color;
        return {};
      },
      color);
}

Result<void> CompManager::RemoveSwap(ShotId shot, CompEffectId id, int swap) {
  return Edit(shot, id, Tr("Remove color pair"),
              [swap](CompEffect* effect) -> Result<void> {
                const bool is_known =
                    swap >= 0 && swap < static_cast<int>(effect->swaps.size());
                if (!is_known) {
                  return std::unexpected(Error{Tr("That color pair is gone.")});
                }
                effect->swaps.erase(effect->swaps.begin() + swap);
                return {};
              });
}

Result<void> CompManager::PickTargetColor(ShotId shot, CompEffectId id,
                                          QColor color,
                                          Qt::KeyboardModifiers modifiers) {
  assert(color.isValid());
  const bool is_add = modifiers.testFlag(Qt::ShiftModifier);
  const bool is_flip = modifiers.testFlag(Qt::ControlModifier);
  return Edit(
      shot, id, Tr("Pick target color"),
      [&](CompEffect* effect) -> Result<void> {
        auto& colors = effect->target.colors;
        const auto found = std::ranges::find_if(
            colors, [&](const QColor& one) { return SameColor(one, color); });
        const bool is_there = found != colors.end();
        if (is_flip && is_there) {
          colors.erase(found);
          return {};
        }
        const bool is_alone = !is_add && !is_flip;
        if (is_alone) {
          colors.clear();
        }
        const bool is_full =
            colors.size() >= static_cast<size_t>(kMaxCompColors);
        if (is_full) {
          return std::unexpected(Error{
              Tr("An effect picks at most %1 colors.").arg(kMaxCompColors)});
        }
        const bool is_new = is_alone || !is_there;
        if (is_new) {
          colors.push_back(color);
        }
        return {};
      },
      color);
}

Result<void> CompManager::ClearTargetColors(ShotId shot, CompEffectId id) {
  return Edit(shot, id, Tr("Clear target colors"),
              [](CompEffect* effect) -> Result<void> {
                effect->target.colors.clear();
                return {};
              });
}

double CompManager::ShotLight(ShotId shot, Frame frame) const {
  assert(history_ != nullptr);
  const Shot* found = FindShot(history_->current(), shot);
  return found != nullptr ? ShotLightAt(*found, frame) : kDefaultShotLight;
}

Result<void> CompManager::SetShotLight(ShotId shot, Frame frame,
                                       double angle) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const double turned = std::fmod(std::fmod(angle, 360.0) + 360.0, 360.0);
  const auto next = WithShot(history_->current(), shot,
                             [&](Shot* edited) -> Result<void> {
                               Put(&edited->light, frame, turned,
                                   Ease::kEaseInOut);
                               return {};
                             });
  return history_->Apply(Tr("Shot light"), next);
}

bool CompManager::HasShotLightKeyAt(ShotId shot, Frame frame) const {
  assert(history_ != nullptr);
  const Shot* found = FindShot(history_->current(), shot);
  return found != nullptr && found->light.keys.size() > 1 &&
         KeyIndexAt(found->light, frame) >= 0;
}

Result<void> CompManager::ToggleShotLightKey(ShotId shot, Frame frame) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_keyed = HasShotLightKeyAt(shot, frame);
  const double value = ShotLight(shot, frame);
  const auto next = WithShot(
      history_->current(), shot, [&](Shot* edited) -> Result<void> {
        auto& channel = edited->light;
        if (is_keyed) {
          RemoveKey(&channel, frame);
          return {};
        }
        const bool is_first = channel.keys.empty() && frame.index() > 0;
        if (is_first) {
          SetKey(&channel, Key<double>{Frame(0), value, Ease::kEaseInOut});
        }
        SetKey(&channel, Key<double>{frame, value, Ease::kEaseInOut});
        return {};
      });
  return history_->Apply(
      is_keyed ? Tr("Remove key shot light") : Tr("Key shot light"), next);
}

}  // namespace snapper
