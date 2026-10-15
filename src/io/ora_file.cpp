#include "io/ora_file.h"

#include <QBuffer>
#include <QFile>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QSaveFile>
#include <QXmlStreamWriter>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "base/text.h"
#include "io/zip_writer.h"

namespace snapper {
namespace {

// Biggest canvas side written; past it the doll is not one Krita can
// hold well, and a bad position cannot ask for gigabytes.
constexpr int kMaxCanvasSide = 32768;
constexpr int kThumbnailSide = 256;
constexpr int kCrossHalf = 6;

// One entry of stack.xml: a group (stack) or a layer.
struct OraNode final {
  QString name;
  bool is_group = false;
  bool is_visible = true;
  // Layers only: the PNG inside the zip and its top-left corner.
  QString source;
  QPoint at;
  // Groups only, bottom to top.
  std::vector<OraNode> children;
};

// How far the canvas grows on each side so every piece fits.
QSize GrowthFor(const DollArt& art) {
  const double half_w = art.canvas.width() / 2.0;
  const double half_h = art.canvas.height() / 2.0;
  double need_x = 0.0;
  double need_y = 0.0;
  for (const ArtPiece& piece : art.pieces) {
    const double left = piece.position.x() + half_w;
    const double top = piece.position.y() + half_h;
    const double right = left + piece.size.width() - art.canvas.width();
    const double bottom = top + piece.size.height() - art.canvas.height();
    need_x = std::max({need_x, -left, right});
    need_y = std::max({need_y, -top, bottom});
  }
  assert(need_x >= 0.0 && need_y >= 0.0);
  return QSize(static_cast<int>(std::ceil(need_x)),
               static_cast<int>(std::ceil(need_y)));
}

// The group called name among the newest of nodes (the top so far),
// made if the top is not already that group, so pieces sharing a
// folder stay together without reordering anything.
OraNode* GroupIn(std::vector<OraNode>* nodes, const QString& name) {
  assert(nodes != nullptr);
  const bool is_open = !nodes->empty() && nodes->back().is_group &&
                       nodes->back().name == name &&
                       nodes->back().source.isEmpty();
  if (!is_open) {
    OraNode group;
    group.name = name;
    group.is_group = true;
    nodes->push_back(group);
  }
  return &nodes->back();
}

void WriteNodes(QXmlStreamWriter* xml, const std::vector<OraNode>& nodes) {
  assert(xml != nullptr);
  // stack.xml lists the top first.
  for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
    const OraNode& node = *it;
    const QString shown =
        node.is_visible ? QStringLiteral("visible") : QStringLiteral("hidden");
    if (node.is_group) {
      xml->writeStartElement("stack");
      xml->writeAttribute("name", node.name);
      xml->writeAttribute("visibility", shown);
      WriteNodes(xml, node.children);
      xml->writeEndElement();
      continue;
    }
    xml->writeEmptyElement("layer");
    xml->writeAttribute("name", node.name);
    xml->writeAttribute("src", node.source);
    xml->writeAttribute("x", QString::number(node.at.x()));
    xml->writeAttribute("y", QString::number(node.at.y()));
    xml->writeAttribute("opacity", "1.0");
    xml->writeAttribute("visibility", shown);
  }
}

QByteArray StackXml(QSize canvas, const std::vector<OraNode>& nodes) {
  QByteArray out;
  QXmlStreamWriter xml(&out);
  xml.writeStartDocument();
  xml.writeStartElement("image");
  xml.writeAttribute("version", "0.0.5");
  xml.writeAttribute("w", QString::number(canvas.width()));
  xml.writeAttribute("h", QString::number(canvas.height()));
  xml.writeStartElement("stack");
  WriteNodes(&xml, nodes);
  xml.writeEndElement();
  xml.writeEndElement();
  xml.writeEndDocument();
  return out;
}

QByteArray Png(const QImage& image) {
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  const bool is_saved = image.save(&buffer, "PNG");
  assert(is_saved);
  return bytes;
}

// Each joint as a small cross with its name, in canvas pixels.
QImage RigPicture(const Doll& doll, QSize canvas, QPointF middle) {
  QImage image(canvas, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(QColor(230, 40, 60), 1.5));
  for (const RigPiece& rig : doll.rig.pieces) {
    const ArtPiece* art = FindArt(doll, rig.name);
    if (art == nullptr) {
      continue;
    }
    const QPointF joint = art->position + rig.pivot + middle;
    painter.drawLine(joint - QPointF(kCrossHalf, 0),
                     joint + QPointF(kCrossHalf, 0));
    painter.drawLine(joint - QPointF(0, kCrossHalf),
                     joint + QPointF(0, kCrossHalf));
    painter.drawText(joint + QPointF(kCrossHalf + 2, -kCrossHalf),
                     rig.name);
  }
  painter.end();
  return image;
}

Result<QByteArray> ReadPng(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return std::unexpected(Error{Tr("Can't read %1.").arg(path)});
  }
  return file.readAll();
}

// Why doll can't be written, or empty.
QString WhyNot(const Doll& doll, QSize canvas) {
  if (doll.art.pieces.size() > static_cast<size_t>(kMaxDollPieces)) {
    return Tr("A doll holds at most %1 pieces.").arg(kMaxDollPieces);
  }
  for (const ArtPiece& piece : doll.art.pieces) {
    const bool is_crowded =
        piece.drawings.size() > static_cast<size_t>(kMaxPieceDrawings);
    if (is_crowded || piece.drawings.empty()) {
      return Tr("%1 has %2 drawings; a piece holds 1 to %3.")
          .arg(piece.name)
          .arg(piece.drawings.size())
          .arg(kMaxPieceDrawings);
    }
  }
  const bool is_huge = canvas.width() > kMaxCanvasSide ||
                       canvas.height() > kMaxCanvasSide ||
                       canvas.isEmpty();
  if (is_huge) {
    return Tr("The doll's canvas would be %1 by %2 pixels, too big for "
              "one file.")
        .arg(canvas.width())
        .arg(canvas.height());
  }
  return {};
}

}  // namespace

Result<void> WriteDollOra(const Doll& doll, const QString& path) {
  assert(!path.isEmpty());
  assert(!doll.folder.isEmpty());
  const QSize grow = GrowthFor(doll.art);
  const QSize canvas = doll.art.canvas + 2 * grow;
  const QString why = WhyNot(doll, canvas);
  if (!why.isEmpty()) {
    return std::unexpected(Error{why});
  }
  const QPointF middle(canvas.width() / 2.0, canvas.height() / 2.0);
  const QDir folder(doll.folder);
  std::vector<ZipEntry> entries{
      {QStringLiteral("mimetype"), QByteArrayLiteral("image/openraster")}};
  std::vector<OraNode> nodes;
  QImage merged(canvas, QImage::Format_ARGB32_Premultiplied);
  merged.fill(Qt::transparent);
  QPainter merge(&merged);
  for (const ArtPiece& piece : doll.art.pieces) {
    std::vector<OraNode>* into = &nodes;
    for (const QString& group : piece.folder) {
      into = &GroupIn(into, group)->children;
    }
    const QPointF corner = piece.position + middle;
    const QPoint at(static_cast<int>(std::lround(corner.x())),
                    static_cast<int>(std::lround(corner.y())));
    const bool is_single = piece.drawings.size() == 1;
    std::vector<OraNode> layers;
    for (size_t i = 0; i < piece.drawings.size(); ++i) {
      const auto bytes = ReadPng(folder.filePath(piece.drawings[i]));
      if (!bytes) {
        return std::unexpected(bytes.error());
      }
      const QString source =
          QStringLiteral("data/%1.png").arg(entries.size(), 4, 10, QChar('0'));
      entries.push_back({source, *bytes});
      const int index = static_cast<int>(i);
      const bool is_default = index == piece.default_drawing;
      // A drawing that reaches past its frame starts that much before it.
      const QPoint drawn_at = at - DrawingOffset(doll, piece.name, index);
      if (is_default) {
        merge.drawImage(drawn_at, QImage::fromData(*bytes, "PNG"));
      }
      layers.push_back({is_single ? piece.name : DrawingName(piece, index),
                        false, is_default || is_single, source, drawn_at,
                        {}});
    }
    if (is_single) {
      into->push_back(layers.front());
    } else {
      into->push_back({piece.name, true, true, {}, {}, layers});
    }
  }
  merge.end();
  const QString rig_source = QStringLiteral("data/rig.png");
  entries.push_back({rig_source, Png(RigPicture(doll, canvas, middle))});
  nodes.push_back({QLatin1String(kRigLayerName), false, false, rig_source,
                   QPoint(0, 0), {}});
  entries.push_back({QStringLiteral("stack.xml"), StackXml(canvas, nodes)});
  entries.push_back({QStringLiteral("mergedimage.png"), Png(merged)});
  entries.push_back({QStringLiteral("Thumbnails/thumbnail.png"),
                     Png(merged.scaled(kThumbnailSide, kThumbnailSide,
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation))});
  if (entries.size() > static_cast<size_t>(kMaxZipEntries)) {
    return std::unexpected(Error{Tr("The doll has too many drawings.")});
  }
  QSaveFile file(path);
  const bool is_written = file.open(QIODevice::WriteOnly) &&
                          file.write(StoredZip(entries)) >= 0 &&
                          file.commit();
  if (!is_written) {
    return std::unexpected(Error{Tr("Can't write %1.").arg(path)});
  }
  return {};
}

}  // namespace snapper
