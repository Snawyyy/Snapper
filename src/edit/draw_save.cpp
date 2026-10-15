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

// The part of sheet that holds a drawing: everything drawn, and at
// least frame, so the drawing keeps its frame whole.
QRect DrawnBounds(const QImage& sheet, QRect frame) {
  assert(!sheet.isNull());
  assert(sheet.rect().contains(frame));
  int left = frame.left();
  int top = frame.top();
  int right = frame.right();
  int bottom = frame.bottom();
  for (int y = 0; y < sheet.height(); ++y) {
    const auto* line = reinterpret_cast<const QRgb*>(sheet.constScanLine(y));
    for (int x = 0; x < sheet.width(); ++x) {
      const bool is_drawn = qAlpha(line[x]) > 0;
      if (is_drawn) {
        left = std::min(left, x);
        right = std::max(right, x);
        top = std::min(top, y);
        bottom = std::max(bottom, y);
      }
    }
  }
  return QRect(QPoint(left, top), QPoint(right, bottom));
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
                                         const QString& name,
                                         QPoint offset) const {
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
    *own = OwnDrawing{file, name, offset};
    art.layer_names.resize(art.drawings.size());
    art.drawings[static_cast<size_t>(replaces_)] = file;
    art.layer_names[static_cast<size_t>(replaces_)] = name;
    index = replaces_;
  } else {
    rig.own_drawings.push_back(OwnDrawing{file, name, offset});
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
    const Doll& doll, const QString& name, const QString& folder,
    const QImage& picture, QPoint offset) const {
  assert(!name.isEmpty());
  assert(!folder.isEmpty());
  const QString twin = MirrorName(track_.piece);
  const bool has_twin = twin != track_.piece &&
                        FindArt(doll, twin) != nullptr &&
                        FindRig(doll.rig, twin) != nullptr;
  const QString piece = has_twin ? twin : track_.piece;
  const ArtPiece& art = *FindArt(doll, piece);
  // A sided drawing name gets the other side's ("turn_L" to "turn_R");
  // else the twin takes the same name, an unsided piece "name flipped".
  const bool is_sided_name = SideOf(name) != 0;
  QString copy_name = is_sided_name ? MirrorName(name)
                      : has_twin    ? name
                                    : Tr("%1 flipped").arg(name);
  for (int i = 0; i < static_cast<int>(art.drawings.size()); ++i) {
    // A copy made before is redone in place; only a Krita drawing of
    // that name pushes it to another.
    const bool is_clash = DrawingName(art, i) == copy_name &&
                          OwnDrawingAt(doll, piece, i) == nullptr;
    if (is_clash) {
      copy_name = Tr("%1 mirror").arg(copy_name);
      break;
    }
  }
  // Flipped left to right: its frame flips with it, from the right.
  QImage flipped = picture.flipped(Qt::Horizontal);
  QPoint flipped_offset(picture.width() - frame_size_.width() - offset.x(),
                        offset.y());
  // Fitted to the twin's frame when that is another size.
  const bool is_other_size = art.size != frame_size_ && !frame_size_.isEmpty();
  if (is_other_size) {
    const double x = art.size.width() / double(frame_size_.width());
    const double y = art.size.height() / double(frame_size_.height());
    flipped = flipped.scaled(qRound(flipped.width() * x),
                             qRound(flipped.height() * y),
                             Qt::IgnoreAspectRatio, Qt::FastTransformation);
    flipped_offset = QPoint(qRound(flipped_offset.x() * x),
                            qRound(flipped_offset.y() * y));
  }
  const QString file = FreeFile(folder, Tr("%1 %2").arg(piece, copy_name));
  const bool is_saved = !file.isEmpty() &&
                        flipped.save(QDir(doll.folder).filePath(file), "PNG");
  if (!is_saved) {
    return std::nullopt;
  }
  return MirrorCopy{piece, file, copy_name, flipped_offset};
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
  RigPiece& rig = *FindRig(&doll.rig, copy.piece);
  // The copy made last time (same piece, same name) takes the new one;
  // its keys keep showing it.
  const auto old = std::find_if(
      rig.own_drawings.begin(), rig.own_drawings.end(),
      [&copy](const OwnDrawing& own) { return own.name == copy.name; });
  const bool is_redone = old != rig.own_drawings.end();
  if (is_redone) {
    const QString old_file = old->file;
    *old = OwnDrawing{copy.file, copy.name, copy.offset};
    for (ArtPiece& art : doll.art.pieces) {
      const bool is_piece = art.name == copy.piece;
      if (!is_piece) {
        continue;
      }
      for (QString& drawing : art.drawings) {
        drawing = drawing == old_file ? copy.file : drawing;
      }
    }
  } else {
    rig.own_drawings.push_back(
        OwnDrawing{copy.file, copy.name, copy.offset});
    AddOwnDrawings(&doll);
  }
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
  // Only what was drawn is kept, with its frame whole, and where the
  // frame starts in it.
  const QRect kept = DrawnBounds(image_, frame_rect());
  const QImage picture = image_.copy(kept);
  const QPoint offset = origin_ - kept.topLeft();
  const bool is_saved = !file.isEmpty() && QDir().mkpath(folder) &&
                        picture.save(QDir(doll->folder).filePath(file), "PNG");
  if (!is_saved) {
    return std::unexpected(Error{Tr("Can't save the drawing in %1.").arg(folder)});
  }
  const QString label = is_replacing() ? Tr("Edit drawing %1").arg(trimmed)
                                       : Tr("New drawing %1").arg(trimmed);
  // A drawing also goes to the other side, or flipped beside itself; a
  // new version redoes the copy made before.
  const bool wants_mirror = mirrors_;
  const auto mirror =
      wants_mirror ? SaveMirror(*doll, trimmed, folder, picture, offset)
                   : std::nullopt;
  Result<Project> next = WithDrawing(file, trimmed, offset);
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
