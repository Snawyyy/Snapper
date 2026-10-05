#ifndef SNAPPER_IO_PROJECT_FILE_H_
#define SNAPPER_IO_PROJECT_FILE_H_

#include <QString>

#include "base/error.h"
#include "model/project.h"

namespace snapper {

// A .snapper file holds everything, dolls included, so it opens the
// same even after the doll library changes.
inline constexpr char kProjectSuffix[] = "snapper";

Result<Project> ReadProject(const QString& path);
Result<void> WriteProject(const QString& path, const Project& project);

}  // namespace snapper

#endif  // SNAPPER_IO_PROJECT_FILE_H_
