#include <cassert>
#include <variant>

#include "base/text.h"
#include "edit/comp_manager.h"
#include "edit/history_manager.h"

namespace snapper {
namespace {

// One part per picked layer: its picked pieces, or the whole layer when
// the layer itself is picked. Effect layers draw nothing, so they and
// layers the shot lacks are left out.
std::vector<CompTargetPart> PartsOf(const Shot* shot,
                                    const std::set<snapper::Pick>& picks) {
  assert(picks.size() < 100000);
  std::vector<CompTargetPart> parts;
  const bool has_shot = shot != nullptr;
  assert(!has_shot || shot->layers.size() <=
                          static_cast<size_t>(kMaxLayersPerShot));
  for (const Pick& pick : picks) {
    const Layer* layer = has_shot ? FindLayer(*shot, pick.layer) : nullptr;
    const bool is_drawn = layer != nullptr &&
                          !std::holds_alternative<EffectLayer>(layer->content);
    if (!is_drawn) {
      continue;
    }
    const bool is_new = parts.empty() || parts.back().layer != pick.layer;
    if (is_new) {
      parts.push_back(CompTargetPart{pick.layer, {}});
    }
    const bool is_piece = !pick.piece.isEmpty();
    if (is_piece) {
      parts.back().pieces.push_back(pick.piece);
    }
  }
  return parts;
}

// "1 piece" or "3 pieces".
QString Counted(size_t count, const char* one, const char* many) {
  assert(count < 10000000);
  assert(one != nullptr && many != nullptr);
  return Tr(count == 1 ? one : many).arg(count);
}

// The words for a target of several layers: how many pieces on how many
// dolls, and any whole layers.
QString SeveralText(const std::vector<CompTargetPart>& parts) {
  assert(parts.size() > 1);
  assert(parts.size() <= static_cast<size_t>(kMaxLayersPerShot));
  size_t pieces = 0;
  size_t dolls = 0;
  for (const CompTargetPart& part : parts) {
    const bool has_pieces = !part.pieces.empty();
    dolls += has_pieces ? 1 : 0;
    pieces += part.pieces.size();
  }
  const size_t whole = parts.size() - dolls;
  QString text;
  const bool has_dolls = dolls > 0;
  const bool has_whole = whole > 0;
  if (has_dolls) {
    text = Tr("%1 on %2")
               .arg(Counted(pieces, "%1 piece", "%1 pieces"),
                    Counted(dolls, "%1 doll", "%1 dolls"));
  }
  if (has_whole) {
    const QString layers = Counted(whole, "%1 layer", "%1 layers");
    text += has_dolls ? QStringLiteral(", ") + layers : layers;
  }
  return text;
}

}  // namespace

TargetTile CompManager::TileOf(const CompEffect& effect) {
  const std::vector<CompTargetPart>& parts = effect.target.parts;
  assert(parts.size() <= static_cast<size_t>(kMaxLayersPerShot));
  assert(effect.id.value() >= 0);
  const bool is_lone_layer = parts.size() == 1 && parts.front().pieces.empty();
  return parts.empty()   ? TargetTile::kEverything
         : is_lone_layer ? TargetTile::kLayer
                         : TargetTile::kPieces;
}

QString CompManager::TargetText(ShotId shot, const CompEffect& effect) const {
  assert(history_ != nullptr);
  assert(effect.target.parts.size() <= static_cast<size_t>(kMaxLayersPerShot));
  const std::vector<CompTargetPart>& parts = effect.target.parts;
  const Shot* found = FindShot(history_->current(), shot);
  QString text = Tr("Everything below");
  const bool is_several = parts.size() > 1;
  if (is_several) {
    text = SeveralText(parts);
  }
  const bool is_lone = parts.size() == 1;
  if (is_lone) {
    const CompTargetPart& part = parts.front();
    const Layer* layer =
        found != nullptr ? FindLayer(*found, part.layer) : nullptr;
    text = layer != nullptr ? layer->name : Tr("A removed layer");
    const size_t pieces = part.pieces.size();
    const bool is_one = pieces == 1;
    const bool is_many = pieces > 1;
    if (is_one) {
      text += QStringLiteral(": ") + part.pieces.front();
    } else if (is_many) {
      text += Tr(": %1 pieces").arg(pieces);
    }
  }
  const size_t colors = effect.target.colors.size();
  const bool has_colors = colors > 0;
  if (has_colors) {
    text += colors == 1 ? Tr(", 1 color") : Tr(", %1 colors").arg(colors);
  }
  return text;
}

QString CompManager::WhyNoTargetLayer(ShotId shot, LayerId layer) const {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  const Shot* found = FindShot(history_->current(), shot);
  const bool is_known = found != nullptr && layer.IsValid();
  const Layer* picked = is_known ? FindLayer(*found, layer) : nullptr;
  const bool is_missing = picked == nullptr;
  if (is_missing) {
    return Tr("Pick a doll, picture or text on the stage first.");
  }
  const bool is_effect = std::holds_alternative<EffectLayer>(picked->content);
  return is_effect ? Tr("An effect layer draws nothing to work on.")
                   : QString();
}

QString CompManager::WhyNoTargetPieces(
    ShotId shot, const std::set<snapper::Pick>& picks) const {
  assert(history_ != nullptr);
  assert(picks.size() < 100000);
  const Shot* found = FindShot(history_->current(), shot);
  const std::vector<CompTargetPart> parts = PartsOf(found, picks);
  bool has_piece = false;
  for (const CompTargetPart& part : parts) {
    has_piece = has_piece || !part.pieces.empty();
  }
  return has_piece ? QString()
                   : Tr("Pick pieces of dolls on the stage first (Ctrl "
                        "flips one, drag a box).");
}

Result<void> CompManager::TargetEverything(ShotId shot, CompEffectId id) {
  assert(id.IsValid());
  assert(history_ != nullptr);
  return Edit(shot, id, Tr("Applies to everything"),
              [](CompEffect* effect) -> Result<void> {
                effect->target.parts.clear();
                return {};
              });
}

Result<void> CompManager::TargetLayer(ShotId shot, CompEffectId id,
                                      LayerId layer) {
  assert(id.IsValid());
  assert(history_ != nullptr);
  const QString why = WhyNoTargetLayer(shot, layer);
  const bool is_refused = !why.isEmpty();
  if (is_refused) {
    return std::unexpected(Error{why});
  }
  return Edit(shot, id, Tr("Applies to a layer"),
              [layer](CompEffect* effect) -> Result<void> {
                effect->target.parts = {CompTargetPart{layer, {}}};
                return {};
              });
}

Result<void> CompManager::TargetPieces(ShotId shot, CompEffectId id,
                                       const std::set<snapper::Pick>& picks) {
  assert(id.IsValid());
  assert(history_ != nullptr);
  const QString why = WhyNoTargetPieces(shot, picks);
  const bool is_refused = !why.isEmpty();
  if (is_refused) {
    return std::unexpected(Error{why});
  }
  const std::vector<CompTargetPart> parts =
      PartsOf(FindShot(history_->current(), shot), picks);
  return Edit(shot, id, Tr("Applies to picked pieces"),
              [&parts](CompEffect* effect) -> Result<void> {
                effect->target.parts = parts;
                return {};
              });
}

}  // namespace snapper
