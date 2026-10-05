#ifndef SNAPPER_IO_LEGACY_DOLL_H_
#define SNAPPER_IO_LEGACY_DOLL_H_

#include <QJsonObject>
#include <QString>

#include "model/doll.h"

namespace snapper {

// A doll.json from the first Snapper (and its Krita exporter), split
// into today's art and rig. There, each piece hung from its parent by a
// pin (a spot in the parent's drawing) at its pivot (a spot in its own),
// turned by a rest angle; roots pinned in doll space. Sizes come from
// the drawings in folder.
struct LegacyDoll final {
  DollArt art;
  Rig rig;
};

LegacyDoll FromLegacy(const QJsonObject& object, const QString& folder);

}  // namespace snapper

#endif  // SNAPPER_IO_LEGACY_DOLL_H_
