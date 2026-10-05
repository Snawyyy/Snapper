#include "io/doll_file.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "base/text.h"
#include "io/doll_json.h"
#include "io/json_file.h"
#include "io/legacy_doll.h"

namespace snapper {
namespace {

constexpr int kArtVersion = 2;
constexpr int kRigVersion = 1;

Result<QJsonObject> CheckFormat(Result<QJsonObject> read, const char* format,
                                int newest, const QString& path) {
  assert(format != nullptr);
  assert(newest > 0);
  if (!read) {
    return read;
  }
  const bool is_format =
      read->value("format").toString() == QLatin1String(format);
  const int version = read->value("version").toInt();
  const bool is_known = is_format && version >= 1 && version <= newest;
  if (!is_known) {
    return std::unexpected(Error{
        Tr("%1 is not a doll file this Snapper can read.").arg(path)});
  }
  return read;
}

}  // namespace

Result<DollArt> ReadArt(const QString& folder) {
  assert(!folder.isEmpty());
  const QDir dir(folder);
  const QString path = dir.filePath(QLatin1String(kArtFileName));
  const bool is_legacy = !QFileInfo::exists(path) &&
                         QFileInfo::exists(dir.filePath(kLegacyFileName));
  if (is_legacy) {
    const auto legacy = ReadJsonFile(dir.filePath(kLegacyFileName));
    if (!legacy) {
      return std::unexpected(legacy.error());
    }
    return FromLegacy(*legacy, folder).art;
  }
  const auto read =
      CheckFormat(ReadJsonFile(path), "snapper-art", kArtVersion, path);
  if (!read) {
    return std::unexpected(read.error());
  }
  JsonIssues issues;
  DollArt art = ArtFromJson(*read, &issues);
  const bool is_damaged = issues.HasIssue();
  if (is_damaged) {
    return std::unexpected(Error{Tr("%1: %2").arg(path, issues.first())});
  }
  assert(art.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  return art;
}

Result<Rig> ReadRig(const QString& folder) {
  assert(!folder.isEmpty());
  const QDir dir(folder);
  const QString path = dir.filePath(QLatin1String(kRigFileName));
  const QString legacy = dir.filePath(QLatin1String(kLegacyFileName));
  const bool is_new_doll = !QFileInfo::exists(path);
  const bool has_legacy_rig = is_new_doll && QFileInfo::exists(legacy);
  if (has_legacy_rig) {
    const auto old = ReadJsonFile(legacy);
    if (!old) {
      return std::unexpected(old.error());
    }
    return FromLegacy(*old, folder).rig;
  }
  if (is_new_doll) {
    return Rig();
  }
  const auto read =
      CheckFormat(ReadJsonFile(path), "snapper-rig", kRigVersion, path);
  if (!read) {
    return std::unexpected(read.error());
  }
  JsonIssues issues;
  Rig rig = RigFromJson(*read, &issues);
  const bool is_damaged = issues.HasIssue();
  if (is_damaged) {
    return std::unexpected(Error{Tr("%1: %2").arg(path, issues.first())});
  }
  assert(rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  return rig;
}

Result<void> WriteRig(const QString& folder, const Rig& rig) {
  assert(!folder.isEmpty());
  assert(rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  QJsonObject object = RigToJson(rig);
  object.insert("format", "snapper-rig");
  object.insert("version", kRigVersion);
  return WriteJsonFile(QDir(folder).filePath(kRigFileName), object);
}

Result<bool> ConvertLegacyDoll(const QString& folder) {
  assert(!folder.isEmpty());
  const QDir dir(folder);
  const QString legacy = dir.filePath(QLatin1String(kLegacyFileName));
  const bool is_old = QFileInfo::exists(legacy) &&
                      !QFileInfo::exists(dir.filePath(kArtFileName));
  if (!is_old) {
    return false;
  }
  const auto read = ReadJsonFile(legacy);
  if (!read) {
    return std::unexpected(read.error());
  }
  const LegacyDoll doll = FromLegacy(*read, folder);
  QJsonObject art = ArtToJson(doll.art);
  art.insert("format", "snapper-art");
  art.insert("version", kArtVersion);
  const auto rig = WriteRig(folder, doll.rig);
  const auto wrote = rig ? WriteJsonFile(dir.filePath(kArtFileName), art)
                         : rig;
  const bool is_kept =
      wrote.has_value() &&
      QFile::rename(legacy, dir.filePath(QLatin1String(kLegacyKeptName)));
  if (!is_kept) {
    return std::unexpected(wrote ? Error{Tr("Can't rename %1.").arg(legacy)}
                                 : wrote.error());
  }
  assert(QFileInfo::exists(dir.filePath(kArtFileName)));
  return true;
}

Result<LoadedDoll> LoadDoll(const QString& folder) {
  assert(!folder.isEmpty());
  const auto art = ReadArt(folder);
  if (!art) {
    return std::unexpected(art.error());
  }
  const auto rig = ReadRig(folder);
  if (!rig) {
    return std::unexpected(rig.error());
  }
  LoadedDoll loaded;
  loaded.doll.name = QFileInfo(folder).completeBaseName();
  loaded.doll.folder = QDir(folder).absolutePath();
  loaded.doll.art = *art;
  loaded.doll.rig = Reconcile(*art, *rig, &loaded.report);
  assert(loaded.doll.rig.pieces.size() == loaded.doll.art.pieces.size());
  return loaded;
}

}  // namespace snapper
