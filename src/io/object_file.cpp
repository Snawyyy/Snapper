#include "io/object_file.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>

#include <algorithm>
#include <cassert>

#include "base/text.h"
#include "io/doll_file.h"

namespace snapper {
namespace {

// The most frames read from one file before thinning, so a huge GIF
// can't hold the whole app.
constexpr int kMaxReadFrames = 4096;

const QStringList& PictureSuffixes() {
  static const QStringList suffixes = {
      QStringLiteral("png"),  QStringLiteral("apng"), QStringLiteral("gif"),
      QStringLiteral("webp"), QStringLiteral("jpg"),  QStringLiteral("jpeg"),
      QStringLiteral("bmp")};
  return suffixes;
}

// image as ARGB32 on a canvas of size, top-left kept, so every frame of
// an object has one size.
QImage OnCanvas(const QImage& image, QSize size) {
  assert(!image.isNull());
  assert(size.isValid());
  QImage converted =
      image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  const bool is_fitting = converted.size() == size;
  if (is_fitting) {
    return converted;
  }
  QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
  canvas.fill(Qt::transparent);
  QPainter painter(&canvas);
  painter.drawImage(QPoint(0, 0), converted);
  painter.end();
  return canvas;
}

}  // namespace

bool IsObjectPicture(const QString& path) {
  assert(path.size() < 100000);
  return PictureSuffixes().contains(QFileInfo(path).suffix().toLower());
}

QStringList ObjectPictures(const QStringList& paths) {
  assert(paths.size() < 100000);
  QStringList pictures;
  for (const QString& path : paths) {
    const QFileInfo info(path);
    const bool is_folder = info.isDir();
    if (is_folder) {
      const QDir dir(path);
      for (const QString& entry :
           dir.entryList(QDir::Files, QDir::Name | QDir::IgnoreCase)) {
        const QString inside = dir.filePath(entry);
        if (IsObjectPicture(inside)) {
          pictures.append(inside);
        }
      }
      continue;
    }
    const bool is_picture = info.isFile() && IsObjectPicture(path);
    if (is_picture) {
      pictures.append(path);
    }
  }
  return pictures;
}

Result<ObjectFrames> ReadObjectFrames(const QString& path) {
  assert(!path.isEmpty());
  assert(kMaxObjectFrames > 0);
  QImageReader reader(path);
  const bool can_read = reader.canRead();
  if (!can_read) {
    return std::unexpected(
        Error{Tr("%1 isn't a picture Snapper can read.").arg(path)});
  }
  std::vector<QImage> all;
  std::vector<int> all_ms;
  for (int i = 0; i < kMaxReadFrames; ++i) {
    const QImage image = reader.read();
    const bool is_end = image.isNull();
    if (is_end) {
      break;
    }
    all.push_back(image);
    all_ms.push_back(std::max(reader.nextImageDelay(), 0));
    const bool has_more = reader.supportsAnimation() && reader.canRead();
    if (!has_more) {
      break;
    }
  }
  const bool is_empty = all.empty();
  if (is_empty) {
    return std::unexpected(Error{Tr("%1 has no picture in it.").arg(path)});
  }
  ObjectFrames read;
  read.source_count = static_cast<int>(all.size());
  read.kept_every = (read.source_count + kMaxObjectFrames - 1) /
                    kMaxObjectFrames;
  const QSize size = all.front().size();
  for (size_t i = 0; i < all.size(); ++i) {
    const bool is_kept = i % static_cast<size_t>(read.kept_every) == 0;
    if (is_kept) {
      read.frames.push_back(OnCanvas(all[i], size));
      read.frame_ms.push_back(all_ms[i]);
    } else {
      read.frame_ms.back() += all_ms[i];
    }
  }
  assert(read.frames.size() <= static_cast<size_t>(kMaxObjectFrames));
  assert(read.frames.size() == read.frame_ms.size());
  return read;
}

Result<void> WriteObjectFolder(const QString& folder, const QString& name,
                               const ObjectFrames& frames) {
  assert(!folder.isEmpty() && !name.isEmpty());
  assert(!frames.frames.empty());
  const bool is_made = QDir().mkpath(folder);
  if (!is_made) {
    return std::unexpected(Error{Tr("Can't make %1.").arg(folder)});
  }
  const QDir dir(folder);
  const QSize size = frames.frames.front().size();
  ArtPiece piece;
  piece.name = name;
  piece.position = QPointF(-size.width() / 2.0, -size.height() / 2.0);
  piece.size = size;
  for (size_t i = 0; i < frames.frames.size(); ++i) {
    const QString label = Tr("frame %1").arg(static_cast<int>(i) + 1);
    const QString file = label + QStringLiteral(".png");
    const bool is_saved = frames.frames[i].save(dir.filePath(file), "PNG");
    if (!is_saved) {
      return std::unexpected(
          Error{Tr("Can't write %1.").arg(dir.filePath(file))});
    }
    piece.drawings.push_back(file);
    piece.layer_names.push_back(label);
  }
  DollArt art;
  art.canvas = size;
  art.pieces.push_back(piece);
  Rig rig;
  RigPiece jointed;
  jointed.name = name;
  jointed.pivot = QPointF(size.width() / 2.0, size.height() / 2.0);
  rig.pieces.push_back(jointed);
  rig.look = DrawingLook::kSmooth;
  rig.object.is_object = true;
  rig.object.frame_ms = frames.frame_ms;
  auto wrote_art = WriteArt(folder, art);
  if (!wrote_art) {
    return wrote_art;
  }
  return WriteRig(folder, rig);
}

}  // namespace snapper
