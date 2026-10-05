#include "io/doll_file.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "base/text.h"
#include "io/doll_json.h"
#include "io/json_file.h"

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

// The first exporter wrote one piece per layer with its pivot in the
// middle and its pin where that middle sits in doll space.
DollArt ArtFromLegacy(const QJsonObject& object) {
  const QJsonArray pieces = object.value("pieces").toArray();
  assert(pieces.size() >= 0);
  DollArt art;
  const qsizetype count = std::min<qsizetype>(pieces.size(), kMaxDollPieces);
  for (qsizetype i = 0; i < count; ++i) {
    const QJsonObject piece = pieces.at(i).toObject();
    const QPointF pivot(piece.value("pivot_x").toDouble(),
                        piece.value("pivot_y").toDouble());
    const QPointF pin(piece.value("pin_x").toDouble(),
                      piece.value("pin_y").toDouble());
    ArtPiece out;
    out.name = piece.value("name").toString();
    const QJsonArray drawings = piece.value("drawings").toArray();
    for (const QJsonValue& drawing : drawings) {
      out.drawings.push_back(drawing.toString());
    }
    out.position = pin - pivot;
    out.size = QSize(static_cast<int>(std::lround(pivot.x() * 2.0)),
                     static_cast<int>(std::lround(pivot.y() * 2.0)));
    art.pieces.push_back(out);
  }
  assert(art.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  return art;
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
    return ArtFromLegacy(*legacy);
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
  const QString path = QDir(folder).filePath(QLatin1String(kRigFileName));
  const bool is_new_doll = !QFileInfo::exists(path);
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
