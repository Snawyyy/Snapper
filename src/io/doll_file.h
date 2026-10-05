#ifndef SNAPPER_IO_DOLL_FILE_H_
#define SNAPPER_IO_DOLL_FILE_H_

#include <QString>
#include <QStringList>

#include "base/error.h"
#include "model/doll.h"

namespace snapper {

// File names inside a doll folder.
inline constexpr char kArtFileName[] = "art.json";
inline constexpr char kRigFileName[] = "rig.json";
// What the first Krita exporter wrote; read so old dolls still open.
inline constexpr char kLegacyFileName[] = "doll.json";

// Krita's side of a doll folder: art.json, or an old doll.json.
Result<DollArt> ReadArt(const QString& folder);
// Snapper's side. A folder without rig.json has an empty rig.
Result<Rig> ReadRig(const QString& folder);
Result<void> WriteRig(const QString& folder, const Rig& rig);

// What matching art to a rig changed, to tell the user.
struct ReconcileReport final {
  // New art pieces that got a default rig.
  QStringList added;
  // Rig pieces whose art is gone; their rig was dropped.
  QStringList missing;
  // IK chains dropped because a piece they used is gone.
  QStringList dropped_chains;

  bool IsClean() const {
    return added.isEmpty() && missing.isEmpty() && dropped_chains.isEmpty();
  }
};

// The rig fitted to art, matched by piece name: every art piece gets a
// rig piece (new ones pivot at their middle and stack as in Krita),
// and nothing whose art is gone survives. report says what changed.
Rig Reconcile(const DollArt& art, const Rig& rig, ReconcileReport* report);

struct LoadedDoll final {
  Doll doll;
  ReconcileReport report;
};

// A doll folder read and reconciled, named after the folder.
Result<LoadedDoll> LoadDoll(const QString& folder);

}  // namespace snapper

#endif  // SNAPPER_IO_DOLL_FILE_H_
