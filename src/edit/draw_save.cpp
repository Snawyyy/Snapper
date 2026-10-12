// DrawManager's Done: saving the drawing next to Krita's and listing it.

#include <QDir>
#include <QFile>

#include <algorithm>
#include <cassert>

#include "base/text.h"
#include "edit/draw_manager.h"
#include "edit/history_manager.h"
#include "edit/palette_manager.h"
#include "edit/pose_edits.h"
#include "io/doll_file.h"

namespace snapper {
namespace {

// The folder inside a doll folder that holds the drawings made in
// Snapper; the Krita exporter carries it over like rig.json.
constexpr char kOwnFolder[] = "snapper";

// name made safe as a file name: no folders, no characters that some
// file systems refuse.
QString FileSafe(const QString& name) {
  assert(!name.isEmpty());
  QString safe = name;
  for (QChar& letter : safe) {
    const bool is_bad = QStringLiteral("/\\:*?\"<>|").contains(letter) ||
                        letter.unicode() < 32;
    if (is_bad) {
      letter = QLatin1Char('_');
    }
  }
  assert(safe.size() == name.size());
  return safe;
}

}  // namespace

QString DrawManager::FreeFile(const QString& folder,
                              const QString& name) const {
  assert(!folder.isEmpty());
  assert(!name.isEmpty());
  const Doll* doll = FindDoll(history_->current(), doll_);
  const ArtPiece* art = doll != nullptr ? FindArt(*doll, track_.piece)
                                        : nullptr;
  const QString base = FileSafe(name);
  for (int number = 1; number <= 4 * kMaxPieceDrawings; ++number) {
    const QString file =
        number == 1 ? base + QStringLiteral(".png")
                    : QStringLiteral("%1 %2.png").arg(base).arg(number);
    const QString listed = QLatin1String(kOwnFolder) + QLatin1Char('/') + file;
    const bool is_taken =
        QFile::exists(QDir(folder).filePath(file)) ||
        (art != nullptr && std::find(art->drawings.begin(), art->drawings.end(),
                                     listed) != art->drawings.end());
    if (!is_taken) {
      return listed;
    }
  }
  return QString();
}

Result<Project> DrawManager::WithDrawing(const QString& file,
                                         const QString& name) const {
  assert(!file.isEmpty());
  assert(!name.isEmpty());
  Project next = history_->current();
  const Doll* found = FindDoll(next, doll_);
  if (found == nullptr || FindArt(*found, track_.piece) == nullptr ||
      FindRig(found->rig, track_.piece) == nullptr) {
    return std::unexpected(
        Error{Tr("%1 is no longer in the project.").arg(track_.piece)});
  }
  Doll doll = *found;
  ArtPiece& art = *std::find_if(
      doll.art.pieces.begin(), doll.art.pieces.end(),
      [this](const ArtPiece& piece) { return piece.name == track_.piece; });
  RigPiece& rig = *FindRig(&doll.rig, track_.piece);
  const bool is_replacing = replaces_ >= 0;
  int index = static_cast<int>(art.drawings.size());
  if (is_replacing) {
    const bool is_in_range =
        replaces_ < static_cast<int>(art.drawings.size());
    const QString old = is_in_range
                            ? art.drawings[static_cast<size_t>(replaces_)]
                            : QString();
    const auto own = std::find_if(
        rig.own_drawings.begin(), rig.own_drawings.end(),
        [&old](const OwnDrawing& drawing) { return drawing.file == old; });
    if (old.isEmpty() || own == rig.own_drawings.end()) {
      return std::unexpected(Error{Tr("That drawing is gone.")});
    }
    *own = OwnDrawing{file, name};
    art.layer_names.resize(art.drawings.size());
    art.drawings[static_cast<size_t>(replaces_)] = file;
    art.layer_names[static_cast<size_t>(replaces_)] = name;
    index = replaces_;
  } else {
    rig.own_drawings.push_back(OwnDrawing{file, name});
    AddOwnDrawings(&doll);
  }
  next.dolls[doll_] = std::make_shared<const Doll>(std::move(doll));
  next = PaletteManager::WithRecent(std::move(next), color());
  // A new drawing shows on the piece where drawing began, so Done shows
  // what was drawn; a new version shows wherever the old one did.
  if (is_replacing) {
    return next;
  }
  return KeyedAt(next, track_, frame_, PiecePose(),
                 [index](PiecePose* pose) {
                   pose->drawing = index;
                   return Result<void>();
                 });
}

Result<void> DrawManager::Done(const QString& name) {
  assert(history_ != nullptr);
  assert(name.size() < 100000);
  if (!is_drawing_) {
    return std::unexpected(Error{Tr("Nothing is being drawn.")});
  }
  const QString trimmed = name.trimmed();
  const Doll* doll = FindDoll(history_->current(), doll_);
  const ArtPiece* art = doll != nullptr ? FindArt(*doll, track_.piece)
                                        : nullptr;
  if (art == nullptr) {
    return std::unexpected(
        Error{Tr("%1 is no longer in the project.").arg(track_.piece)});
  }
  if (trimmed.isEmpty()) {
    return std::unexpected(Error{Tr("Name the drawing first.")});
  }
  for (int i = 0; i < static_cast<int>(art->drawings.size()); ++i) {
    const bool is_clash = i != replaces_ && DrawingName(*art, i) == trimmed;
    if (is_clash) {
      return std::unexpected(Error{
          Tr("%1 already has a drawing called %2.").arg(track_.piece, trimmed)});
    }
  }
  const QString folder = QDir(doll->folder).filePath(QLatin1String(kOwnFolder));
  const QString file = FreeFile(folder, trimmed);
  const bool is_saved = !file.isEmpty() && QDir().mkpath(folder) &&
                        image_.save(QDir(doll->folder).filePath(file), "PNG");
  if (!is_saved) {
    return std::unexpected(Error{Tr("Can't save the drawing in %1.").arg(folder)});
  }
  const QString label = is_replacing() ? Tr("Edit drawing %1").arg(trimmed)
                                       : Tr("New drawing %1").arg(trimmed);
  auto applied = history_->Apply(label, WithDrawing(file, trimmed));
  if (!applied) {
    QFile::remove(QDir(doll->folder).filePath(file));
    return applied;
  }
  End();
  return {};
}

}  // namespace snapper
