#ifndef SNAPPER_EDIT_DOLL_CONVERT_H_
#define SNAPPER_EDIT_DOLL_CONVERT_H_

#include <QString>

#include <map>

#include "model/character.h"
#include "model/doll.h"
#include "model/layer.h"
#include "model/project.h"

namespace snapper {

// Turns the old PNG dolls into characters, so projects made before the
// characters open as they looked. A doll becomes a bone part with one
// bone per piece (from its joint toward its children); a piece with one
// drawing becomes a picture riding its bone, one with several a switch
// of pictures; its smart dials become smart bones whose actions turn
// and swap the pieces as the stops did. Mesh warps, physics, lean,
// swivel and skins have no match and are left out.

// What a doll's parts became, to carry its layers' keys across.
struct DollParts final {
  std::map<QString, BoneId> bones{};
  // The part each piece shows through: its picture or its switch.
  std::map<QString, PartId> parts{};
  // A switch's child for each drawing index.
  std::map<QString, std::vector<PartId>> drawings{};
  std::map<QString, BoneId> dials{};
  std::map<QString, double> dial_rests{};
};

// doll as a character, and what each piece became.
Character CharacterFromDoll(const Doll& doll, DollParts* parts);

// A doll layer's keys as a character layer's.
CharacterLayer CharacterLayerFromDoll(const Doll& doll, const DollLayer& layer,
                                      const DollParts& parts);

// project with every doll that is not an object (a prop picture keeps
// its GIF playing as a doll) made a character, and every layer showing
// it a character layer. A character already called like the doll wins
// and the doll stays.
Project WithDollsAsCharacters(Project project);

}  // namespace snapper

#endif  // SNAPPER_EDIT_DOLL_CONVERT_H_
