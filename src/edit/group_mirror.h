#ifndef SNAPPER_EDIT_GROUP_MIRROR_H_
#define SNAPPER_EDIT_GROUP_MIRROR_H_

#include <QString>

#include <vector>

#include "model/project.h"

namespace snapper {

// The project with doll's rig set to after (before with the pieces named
// in copied made the mirror of their twins), and for every dot group
// holding dots of a copied piece, in the doll's defaults and in each
// shot's groups on this doll's layers, a twin group holding the mirrored
// dots on the twin piece ("arm_l" groups become "arm_r", other names get
// " mirrored"). A twin that exists is made again, so copying twice never
// piles up groups.
Project WithTwinGroups(const Project& project, const QString& doll,
                       const Doll& before, Doll after,
                       const std::vector<QString>& copied);

}  // namespace snapper

#endif  // SNAPPER_EDIT_GROUP_MIRROR_H_
