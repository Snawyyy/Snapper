#include "edit/dial_manager.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/sampler.h"
#include "anim/smart_dial.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/pose_edits.h"
#include "edit/rig_edits.h"

namespace snapper {
namespace {

// A stop of a template: its name and place.
struct TemplateStop final {
  const char* name;
  double at;
};

Error NoDial(const QString& doll, const QString& dial) {
  return Error{Tr("%1 has no dial called %2.").arg(doll, dial)};
}

// The project with doll's dial called dial changed by change.
template <typename Change>
Result<Project> WithDial(const Project& project, const QString& doll,
                         const QString& dial, Change change) {
  assert(!doll.isEmpty());
  assert(!dial.isEmpty());
  return WithRig(project, doll, [&](Rig* rig) -> Result<void> {
    SmartDial* found = FindDial(&rig->dials, dial);
    if (found == nullptr) {
      return std::unexpected(NoDial(doll, dial));
    }
    return change(found);
  });
}

// The first of name, "name 2", "name 3"... that dials do not use.
QString FreshDialName(const std::vector<SmartDial>& dials,
                      const QString& name) {
  assert(!name.isEmpty());
  assert(dials.size() <= static_cast<size_t>(kMaxSmartDials));
  QString fresh = name;
  for (int number = 2; FindDial(dials, fresh) != nullptr; ++number) {
    fresh = QStringLiteral("%1 %2").arg(name).arg(number);
  }
  return fresh;
}

std::vector<TemplateStop> StopsOf(DialTemplate kind) {
  switch (kind) {
    case DialTemplate::kHeadTurn:
      return {{"Left", 0.0}, {"Front", 0.5}, {"Right", 1.0}};
    case DialTemplate::kBlink:
      return {{"Open", 0.0}, {"Half", 0.5}, {"Closed", 1.0}};
    case DialTemplate::kMouth:
      return {{"Rest", 0.0}, {"A", 0.25}, {"E", 0.5}, {"O", 0.75},
              {"M", 1.0}};
    case DialTemplate::kBlank:
      return {};
  }
  return {};
}

// The drawing a piece shows with pose: its own pick, else the rig's,
// else Krita's.
int ShownDrawing(const Doll& doll, const QString& piece, int picked) {
  const RigPiece* rig = FindRig(doll.rig, piece);
  const ArtPiece* art = FindArt(doll, piece);
  const int fallback = rig != nullptr && rig->default_drawing >= 0
                           ? rig->default_drawing
                       : art != nullptr ? art->default_drawing
                                        : -1;
  return picked >= 0 ? picked : fallback;
}

// A pose with what the dial takes out of the hand: turn, move, size,
// skew, opacity, drawing and order back at rest; warp, lean and swivel
// kept.
PiecePose Emptied(PiecePose pose) {
  const PiecePose rest;
  pose.rotation = rest.rotation;
  pose.offset = rest.offset;
  pose.scale_x = rest.scale_x;
  pose.scale_y = rest.scale_y;
  pose.skew = rest.skew;
  pose.opacity = rest.opacity;
  pose.drawing = rest.drawing;
  pose.order = rest.order;
  return pose;
}

// Index of the stop of dial within snapping distance of value, or -1.
int StopNear(const SmartDial& dial, double value) {
  int best = -1;
  double nearest = kDialSnap;
  for (size_t i = 0; i < dial.stops.size(); ++i) {
    const double apart = std::abs(dial.stops[i].at - value);
    if (apart <= nearest) {
      nearest = apart;
      best = static_cast<int>(i);
    }
  }
  return best;
}

// Puts dial's stops back in order, keeping rest on the same stop.
void Sorted(SmartDial* dial) {
  assert(dial != nullptr);
  const bool has_rest = dial->rest >= 0 &&
                        dial->rest < static_cast<int>(dial->stops.size());
  const double rest_at = has_rest
                             ? dial->stops[static_cast<size_t>(dial->rest)].at
                             : -1.0;
  std::ranges::stable_sort(dial->stops, {}, &DialStop::at);
  dial->rest = 0;
  for (size_t i = 0; i < dial->stops.size(); ++i) {
    if (dial->stops[i].at == rest_at) {
      dial->rest = static_cast<int>(i);
    }
  }
}

bool IsStop(const SmartDial& dial, int stop) {
  return stop >= 0 && stop < static_cast<int>(dial.stops.size());
}

}  // namespace

QString TemplateName(DialTemplate kind) {
  switch (kind) {
    case DialTemplate::kHeadTurn:
      return Tr("Head turn");
    case DialTemplate::kBlink:
      return Tr("Blink");
    case DialTemplate::kMouth:
      return Tr("Mouth");
    case DialTemplate::kBlank:
      return Tr("Dial");
  }
  return Tr("Dial");
}

DialManager::DialManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
}

Result<QString> DialManager::AddDial(const QString& doll, DialTemplate kind,
                                     const std::vector<QString>& pieces) {
  assert(history_ != nullptr);
  assert(static_cast<int>(kind) < kDialTemplateCount);
  const Doll* found = FindDoll(history_->current(), doll);
  if (found == nullptr) {
    return std::unexpected(Error{Tr("%1 isn't in the project.").arg(doll)});
  }
  const bool is_full =
      found->rig.dials.size() >= static_cast<size_t>(kMaxSmartDials);
  if (is_full) {
    return std::unexpected(
        Error{Tr("%1 has as many dials as it can hold.").arg(doll)});
  }
  SmartDial dial;
  dial.name = FreshDialName(found->rig.dials, TemplateName(kind));
  for (const QString& piece : pieces) {
    const bool is_new = FindRig(found->rig, piece) != nullptr &&
                        std::ranges::find(dial.pieces, piece) ==
                            dial.pieces.end();
    if (is_new) {
      dial.pieces.push_back(piece);
    }
  }
  if (dial.pieces.empty()) {
    return std::unexpected(
        Error{Tr("Pick the pieces the dial should move first.")});
  }
  for (const TemplateStop& stop : StopsOf(kind)) {
    dial.stops.push_back({Tr(stop.name), stop.at, {}, {}});
  }
  const bool is_turn = kind == DialTemplate::kHeadTurn;
  dial.rest = is_turn ? 1 : 0;
  dial.swap = is_turn ? DialSwap::kHalfway : DialSwap::kAtStop;
  const QString name = dial.name;
  const auto added = history_->Apply(
      Tr("Add %1 dial").arg(name),
      WithRig(history_->current(), doll, [&dial](Rig* rig) -> Result<void> {
        rig->dials.push_back(dial);
        return {};
      }));
  if (!added) {
    return std::unexpected(added.error());
  }
  return name;
}

Result<void> DialManager::RemoveDial(const QString& doll,
                                     const QString& dial) {
  assert(history_ != nullptr);
  assert(!dial.isEmpty());
  auto next = WithRig(history_->current(), doll,
                      [&](Rig* rig) -> Result<void> {
                        const auto gone = std::erase_if(
                            rig->dials, [&dial](const SmartDial& d) {
                              return d.name == dial;
                            });
                        return gone > 0 ? Result<void>()
                                        : std::unexpected(NoDial(doll, dial));
                      });
  if (next) {
    next = WithDollLayers(std::move(*next), doll, [&dial](DollLayer* layer) {
      layer->dials.erase(dial);
    });
  }
  return history_->Apply(Tr("Delete %1 dial").arg(dial), std::move(next));
}

Result<void> DialManager::RenameDial(const QString& doll, const QString& dial,
                                     const QString& to) {
  assert(history_ != nullptr);
  const QString name = to.trimmed();
  if (name.isEmpty()) {
    return std::unexpected(Error{Tr("A dial needs a name.")});
  }
  if (name == dial) {
    return {};
  }
  auto next = WithRig(history_->current(), doll,
                      [&](Rig* rig) -> Result<void> {
                        const bool is_taken =
                            FindDial(rig->dials, name) != nullptr;
                        if (is_taken) {
                          return std::unexpected(Error{
                              Tr("%1 already has a dial called %2.")
                                  .arg(doll, name)});
                        }
                        SmartDial* found = FindDial(&rig->dials, dial);
                        if (found == nullptr) {
                          return std::unexpected(NoDial(doll, dial));
                        }
                        found->name = name;
                        return {};
                      });
  if (next) {
    next = WithDollLayers(std::move(*next), doll, [&](DollLayer* layer) {
      auto keys = layer->dials.extract(dial);
      if (!keys.empty()) {
        keys.key() = name;
        layer->dials.insert(std::move(keys));
      }
    });
  }
  return history_->Apply(Tr("Rename %1 dial").arg(dial), std::move(next));
}

Result<void> DialManager::SetPieces(const QString& doll, const QString& dial,
                                    const std::vector<QString>& pieces) {
  assert(history_ != nullptr);
  const bool is_valid =
      !pieces.empty() && pieces.size() <= static_cast<size_t>(kMaxDialPieces);
  if (!is_valid) {
    return std::unexpected(Error{Tr("A dial drives at least one piece.")});
  }
  return history_->Apply(
      Tr("Pieces of %1 dial").arg(dial),
      WithDial(history_->current(), doll, dial,
               [&pieces](SmartDial* found) -> Result<void> {
                 found->pieces = pieces;
                 return {};
               }));
}

Result<void> DialManager::MoveDial(const QString& doll, const QString& dial,
                                   int by) {
  assert(history_ != nullptr);
  assert(by == -1 || by == 1);
  return history_->Apply(
      Tr("Move %1 dial").arg(dial),
      WithRig(history_->current(), doll, [&](Rig* rig) -> Result<void> {
        const auto found = std::ranges::find_if(
            rig->dials, [&dial](const SmartDial& d) { return d.name == dial; });
        if (found == rig->dials.end()) {
          return std::unexpected(NoDial(doll, dial));
        }
        const long at = found - rig->dials.begin();
        const long to = at + by;
        const bool is_inside =
            to >= 0 && to < static_cast<long>(rig->dials.size());
        if (!is_inside) {
          return std::unexpected(Error{Tr("It is already at the end.")});
        }
        std::swap(rig->dials[static_cast<size_t>(at)],
                  rig->dials[static_cast<size_t>(to)]);
        return {};
      }));
}

Result<void> DialManager::SetStop(ShotId shot, LayerId layer, Frame frame,
                                  const QString& dial, double value,
                                  const QString& name) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const Project& project = history_->current();
  const DollLayer* posed = PosedLayerOf(project, shot, layer);
  const Doll* doll = DollOfLayer(project, shot, layer);
  if (posed == nullptr || doll == nullptr) {
    return std::unexpected(Error{Tr("Pick a doll first.")});
  }
  const SmartDial* found = FindDial(doll->rig.dials, dial);
  if (found == nullptr) {
    return std::unexpected(NoDial(doll->name, dial));
  }
  const double at =
      std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : RestValue(*found);
  const int near = StopNear(*found, at);
  const bool is_new = near < 0;
  const bool is_full =
      is_new && found->stops.size() >= static_cast<size_t>(kMaxDialStops);
  if (is_full) {
    return std::unexpected(Error{
        Tr("A dial holds at most %1 stops.").arg(kMaxDialStops)});
  }
  // What shows now: the dial's own change with the hand pose on top.
  const auto changes = DialPoses(*doll, *found, at);
  DialStop stop;
  stop.at = is_new ? at : found->stops[static_cast<size_t>(near)].at;
  stop.name = !is_new ? found->stops[static_cast<size_t>(near)].name
              : name.trimmed().isEmpty()
                  ? Tr("Stop %1").arg(found->stops.size() + 1)
                  : name.trimmed();
  for (const QString& piece : found->pieces) {
    const auto keyed = posed->pieces.find(piece);
    const PiecePose hand = keyed == posed->pieces.end()
                               ? PiecePose()
                               : Sample(keyed->second, frame, PiecePose());
    const auto change = changes.find(piece);
    PiecePose shown = DialUnder(
        change == changes.end() ? PiecePose() : change->second, hand);
    const int drawing = ShownDrawing(*doll, piece, shown.drawing);
    const ArtPiece* art = FindArt(*doll, piece);
    const bool has_drawing =
        art != nullptr && drawing >= 0 &&
        drawing < static_cast<int>(art->drawings.size());
    if (has_drawing) {
      stop.drawings[piece] = DrawingName(*art, drawing);
    }
    shown.drawing = -1;
    shown.lean = 0.0;
    shown.swivel = 0.0;
    shown.warp.clear();
    stop.poses[piece] = shown;
  }
  auto next = WithDial(project, doll->name, dial,
                       [&](SmartDial* edited) -> Result<void> {
                         if (is_new) {
                           edited->stops.push_back(stop);
                         } else {
                           edited->stops[static_cast<size_t>(near)] = stop;
                         }
                         Sorted(edited);
                         return {};
                       });
  for (const QString& piece : found->pieces) {
    if (!next) {
      break;
    }
    next = KeyedAt(*next, TrackRef{shot, TrackKind::kPiece, layer, piece},
                   frame, PiecePose(), [](PiecePose* pose) {
                     *pose = Emptied(*pose);
                     return Result<void>();
                   });
  }
  if (next) {
    next = KeyedAt(*next, TrackRef{shot, TrackKind::kDial, layer, dial},
                   frame, RestValue(*found), [&stop](double* keyed) {
                     *keyed = stop.at;
                     return Result<void>();
                   });
  }
  return history_->Apply(Tr("Set %1 stop %2").arg(dial, stop.name),
                         std::move(next));
}

Result<void> DialManager::RemoveStop(const QString& doll, const QString& dial,
                                     int stop) {
  assert(history_ != nullptr);
  assert(stop >= -1);
  return history_->Apply(
      Tr("Delete stop of %1").arg(dial),
      WithDial(history_->current(), doll, dial,
               [stop](SmartDial* found) -> Result<void> {
                 if (!IsStop(*found, stop)) {
                   return std::unexpected(Error{Tr("That stop is gone.")});
                 }
                 found->stops.erase(found->stops.begin() + stop);
                 const bool was_after = found->rest > stop;
                 found->rest = was_after ? found->rest - 1 : found->rest;
                 const int last = static_cast<int>(found->stops.size()) - 1;
                 found->rest = std::clamp(found->rest, 0, std::max(0, last));
                 return {};
               }));
}

Result<void> DialManager::RenameStop(const QString& doll, const QString& dial,
                                     int stop, const QString& to) {
  assert(history_ != nullptr);
  const QString name = to.trimmed();
  if (name.isEmpty()) {
    return std::unexpected(Error{Tr("A stop needs a name.")});
  }
  return history_->Apply(
      Tr("Rename stop of %1").arg(dial),
      WithDial(history_->current(), doll, dial,
               [stop, &name](SmartDial* found) -> Result<void> {
                 if (!IsStop(*found, stop)) {
                   return std::unexpected(Error{Tr("That stop is gone.")});
                 }
                 found->stops[static_cast<size_t>(stop)].name = name;
                 return {};
               }));
}

Result<void> DialManager::MoveStop(const QString& doll, const QString& dial,
                                   int stop, double at) {
  assert(history_ != nullptr);
  const bool is_number = std::isfinite(at);
  if (!is_number) {
    return std::unexpected(Error{Tr("That is not a place on the dial.")});
  }
  const double place = std::clamp(at, 0.0, 1.0);
  return history_->Apply(
      Tr("Move stop of %1").arg(dial),
      WithDial(history_->current(), doll, dial,
               [stop, place](SmartDial* found) -> Result<void> {
                 if (!IsStop(*found, stop)) {
                   return std::unexpected(Error{Tr("That stop is gone.")});
                 }
                 for (size_t i = 0; i < found->stops.size(); ++i) {
                   const bool is_crowded =
                       static_cast<int>(i) != stop &&
                       std::abs(found->stops[i].at - place) < kDialSnap;
                   if (is_crowded) {
                     return std::unexpected(
                         Error{Tr("Another stop is right there.")});
                   }
                 }
                 found->stops[static_cast<size_t>(stop)].at = place;
                 Sorted(found);
                 return {};
               }));
}

Result<void> DialManager::SetRest(const QString& doll, const QString& dial,
                                  int stop) {
  assert(history_ != nullptr);
  assert(stop >= -1);
  return history_->Apply(
      Tr("Rest of %1 dial").arg(dial),
      WithDial(history_->current(), doll, dial,
               [stop](SmartDial* found) -> Result<void> {
                 if (!IsStop(*found, stop)) {
                   return std::unexpected(Error{Tr("That stop is gone.")});
                 }
                 found->rest = stop;
                 return {};
               }));
}

Result<void> DialManager::SetSwap(const QString& doll, const QString& dial,
                                  DialSwap swap) {
  assert(history_ != nullptr);
  assert(!dial.isEmpty());
  return history_->Apply(
      Tr("Swap point of %1 dial").arg(dial),
      WithDial(history_->current(), doll, dial,
               [swap](SmartDial* found) -> Result<void> {
                 found->swap = swap;
                 return {};
               }));
}

Result<void> DialManager::Turn(ShotId shot, const std::vector<LayerId>& layers,
                               Frame frame, const QString& dial,
                               double value) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_number = std::isfinite(value);
  if (!is_number) {
    return std::unexpected(Error{Tr("That is not a place on the dial.")});
  }
  const double place = std::clamp(value, 0.0, 1.0);
  Result<Project> next = history_->current();
  bool has_dial = false;
  for (const LayerId layer : layers) {
    const Doll* doll = DollOfLayer(*next, shot, layer);
    const SmartDial* found =
        doll != nullptr ? FindDial(doll->rig.dials, dial) : nullptr;
    if (found == nullptr) {
      continue;
    }
    has_dial = true;
    next = KeyedAt(*next, TrackRef{shot, TrackKind::kDial, layer, dial},
                   frame, RestValue(*found), [place](double* keyed) {
                     *keyed = place;
                     return Result<void>();
                   });
    if (!next) {
      break;
    }
  }
  if (!has_dial) {
    return std::unexpected(
        Error{Tr("No picked doll has a dial called %1.").arg(dial)});
  }
  return history_->Apply(Tr("%1 dial").arg(dial), std::move(next));
}

std::vector<SmartDial> DialManager::DialsOf(ShotId shot, LayerId layer,
                                            const QString& piece) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const Doll* doll = DollOfLayer(history_->current(), shot, layer);
  std::vector<SmartDial> dials;
  if (doll == nullptr) {
    return dials;
  }
  for (const SmartDial& dial : doll->rig.dials) {
    const bool is_shown =
        piece.isEmpty() ||
        std::ranges::find(dial.pieces, piece) != dial.pieces.end();
    if (is_shown) {
      dials.push_back(dial);
    }
  }
  return dials;
}

double DialManager::ValueOf(ShotId shot, LayerId layer, Frame frame,
                            const QString& dial) const {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const Project& project = history_->current();
  const DollLayer* posed = PosedLayerOf(project, shot, layer);
  const Doll* doll = DollOfLayer(project, shot, layer);
  const SmartDial* found =
      doll != nullptr ? FindDial(doll->rig.dials, dial) : nullptr;
  if (found == nullptr || posed == nullptr) {
    return 0.5;
  }
  return DialValue(*found, *posed, frame);
}

QString DialManager::WhyNoDial(ShotId shot, LayerId layer,
                               const std::vector<QString>& pieces) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const Doll* doll = DollOfLayer(history_->current(), shot, layer);
  if (doll == nullptr) {
    return Tr("Pick a doll first.");
  }
  if (pieces.empty()) {
    return Tr("Pick the pieces the dial should move first.");
  }
  const bool is_full =
      doll->rig.dials.size() >= static_cast<size_t>(kMaxSmartDials);
  if (is_full) {
    return Tr("%1 has as many dials as it can hold.").arg(doll->name);
  }
  return QString();
}

}  // namespace snapper
