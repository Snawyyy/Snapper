// DrawManager's Done: saving the drawing next to Krita's and listing it.

#include <QDir>
#include <QFile>
#include <QImage>

#include <algorithm>
#include <cassert>

#include "base/text.h"
#include "edit/draw_manager.h"
#include "edit/history_manager.h"
#include "anim/mirror.h"
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

void DrawManager::SetMirrors(bool mirrors) {
  assert(history_ != nullptr);
  assert(mirrors || !mirrors);
  mirrors_ = mirrors;
  emit Changed();
}

std::optional<DrawManager::MirrorCopy> DrawManager::SaveMirror(
    const Doll& doll, const QString& name, const QString& folder) const {
  assert(!name.isEmpty());
  assert(!folder.isEmpty());
  const QString twin = MirrorName(track_.piece);
  const bool has_twin = twin != track_.piece &&
                        FindArt(doll, twin) != nullptr &&
                        FindRig(doll.rig, twin) != nullptr;
  const QString piece = has_twin ? twin : track_.piece;
  const ArtPiece& art = *FindArt(doll, piece);
  QString copy_name = has_twin ? name : Tr("%1 flipped").arg(name);
  for (int i = 0; i < static_cast<int>(art.drawings.size()); ++i) {
    const bool is_clash = DrawingName(art, i) == copy_name;
    if (is_clash) {
      copy_name = Tr("%1 mirror").arg(copy_name);
      break;
    }
  }
  // Flipped left to right, fitted to the twin's frame when it differs.
  QImage flipped = image_.flipped(Qt::Horizontal);
  const bool is_other_size = flipped.size() != art.size;
  if (is_other_size) {
    flipped = flipped.scaled(art.size, Qt::IgnoreAspectRatio,
                             Qt::FastTransformation);
  }
  const QString file = FreeFile(folder, Tr("%1 %2").arg(piece, copy_name));
  const bool is_saved = !file.isEmpty() &&
                        flipped.save(QDir(doll.folder).filePath(file), "PNG");
  if (!is_saved) {
    return std::nullopt;
  }
  return MirrorCopy{piece, file, copy_name};
}

Result<Project> DrawManager::WithMirror(Result<Project> next,
                                        const QString& doll_name,
                                        const MirrorCopy& copy) {
  assert(!copy.file.isEmpty());
  assert(!doll_name.isEmpty());
  if (!next) {
    return next;
  }
  const Doll* found = FindDoll(*next, doll_name);
  const bool has_piece =
      found != nullptr && FindRig(found->rig, copy.piece) != nullptr;
  if (!has_piece) {
    return next;
  }
  Doll doll = *found;
  FindRig(&doll.rig, copy.piece)
      ->own_drawings.push_back(OwnDrawing{copy.file, copy.name});
  AddOwnDrawings(&doll);
  next->dolls[doll_name] = std::make_shared<const Doll>(std::move(doll));
  return next;
}

Result<void> DrawManager::Done(const QString& name) {
  assert(history_ != nullptr);
  assert(name.size() < 100000);
  if (!is_drawing_) {
    return std::unexpected(Error{Tr("Nothing is being drawn.")});
  }
  // A job's picture is kept by whoever handed it in, unnamed.
  if (target_ != DrawTarget::kPiece) {
    assert(keep_ != nullptr);
    auto kept = keep_(image_);
    if (kept) {
      End();
    }
    return kept;
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
  // A new drawing also goes to the other side, or flipped beside itself;
  // a new version of one stays where it is.
  const bool wants_mirror = mirrors_ && !is_replacing();
  const auto mirror = wants_mirror ? SaveMirror(*doll, trimmed, folder)
                                   : std::nullopt;
  Result<Project> next = WithDrawing(file, trimmed);
  if (mirror) {
    next = WithMirror(std::move(next), doll_, *mirror);
  }
  auto applied = history_->Apply(label, std::move(next));
  if (!applied) {
    QFile::remove(QDir(doll->folder).filePath(file));
    if (mirror) {
      QFile::remove(QDir(doll->folder).filePath(mirror->file));
    }
    return applied;
  }
  End();
  return {};
}

}  // namespace snapper
