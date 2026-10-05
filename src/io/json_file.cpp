#include "io/json_file.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

#include <cassert>

#include "base/text.h"

namespace snapper {

Result<QJsonObject> ReadJsonFile(const QString& path) {
  assert(!path.isEmpty());
  assert(kMaxJsonFileBytes > 0);
  QFile file(path);
  const bool is_open = file.open(QIODevice::ReadOnly);
  if (!is_open) {
    return std::unexpected(
        Error{Tr("Can't open %1: %2").arg(path, file.errorString())});
  }
  const bool is_too_big = file.size() > kMaxJsonFileBytes;
  if (is_too_big) {
    return std::unexpected(Error{Tr("%1 is too big to be a Snapper file.")
                                     .arg(path)});
  }
  QJsonParseError problem;
  const QJsonDocument document =
      QJsonDocument::fromJson(file.readAll(), &problem);
  const bool is_object = problem.error == QJsonParseError::NoError &&
                         document.isObject();
  if (!is_object) {
    return std::unexpected(Error{
        Tr("%1 is damaged: %2").arg(path, problem.errorString())});
  }
  return document.object();
}

Result<void> WriteJsonFile(const QString& path, const QJsonObject& object) {
  assert(!path.isEmpty());
  assert(!object.isEmpty());
  QSaveFile file(path);
  const bool is_open = file.open(QIODevice::WriteOnly);
  if (!is_open) {
    return std::unexpected(
        Error{Tr("Can't write %1: %2").arg(path, file.errorString())});
  }
  const QByteArray bytes = QJsonDocument(object).toJson();
  const bool is_written = file.write(bytes) == bytes.size() && file.commit();
  if (!is_written) {
    return std::unexpected(
        Error{Tr("Can't save %1: %2").arg(path, file.errorString())});
  }
  return {};
}

}  // namespace snapper
