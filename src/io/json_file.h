#ifndef SNAPPER_IO_JSON_FILE_H_
#define SNAPPER_IO_JSON_FILE_H_

#include <QJsonObject>
#include <QString>

#include "base/error.h"

namespace snapper {

// Files past this are not Snapper files; refusing them keeps a wrong
// pick from eating memory.
constexpr qint64 kMaxJsonFileBytes = 256LL * 1024 * 1024;

Result<QJsonObject> ReadJsonFile(const QString& path);

// Writes through a temporary file and swaps it in, so a crash or a full
// disk never leaves half a file where the old one was.
Result<void> WriteJsonFile(const QString& path, const QJsonObject& object);

}  // namespace snapper

#endif  // SNAPPER_IO_JSON_FILE_H_
