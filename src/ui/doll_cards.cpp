#include "ui/doll_cards.h"

#include <QDir>
#include <QPaintEvent>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <string_view>

#include "io/doll_file.h"

namespace snapper {
namespace {

// Each card keeps its doll's name here (the cast panel sets it).
constexpr int kNameRole = Qt::UserRole;
constexpr QSize kCardIcon(72, 72);
constexpr QSize kCardCell(92, 112);

}  // namespace

QStringList DollsOfDrop(const QMimeData* data) {
  assert(data != nullptr);
  assert(std::string_view(kDollMime).size() > 0);
  const bool has_dolls = data->hasFormat(QString::fromLatin1(kDollMime));
  if (!has_dolls) {
    return {};
  }
  return QString::fromUtf8(data->data(QString::fromLatin1(kDollMime)))
      .split('\n', Qt::SkipEmptyParts);
}

QPixmap DollPicture(const QString& folder, int size) {
  assert(size > 0);
  assert(!folder.isEmpty());
  const auto art = ReadArt(folder);
  const bool is_readable = art.has_value() && !art->pieces.empty();
  if (!is_readable) {
    return QPixmap();
  }
  // The box round every piece (the art's canvas may be unset).
  QRectF box;
  for (const ArtPiece& piece : art->pieces) {
    const QRectF here(piece.position, QSizeF(piece.size));
    box = box.isNull() ? here : box.united(here);
  }
  const bool has_box = box.width() > 0.0 && box.height() > 0.0;
  if (!has_box) {
    return QPixmap();
  }
  const double scale = static_cast<double>(size) /
                       std::max(box.width(), box.height());
  QImage canvas(QSize(static_cast<int>(std::ceil(box.width() * scale)),
                      static_cast<int>(std::ceil(box.height() * scale))),
                QImage::Format_ARGB32_Premultiplied);
  canvas.fill(Qt::transparent);
  QPainter painter(&canvas);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  for (const ArtPiece& piece : art->pieces) {
    const bool has_drawing = !piece.drawings.empty();
    if (!has_drawing) {
      continue;
    }
    const size_t at = static_cast<size_t>(std::clamp(
        piece.default_drawing, 0,
        static_cast<int>(piece.drawings.size()) - 1));
    const QImage drawing(QDir(folder).filePath(piece.drawings[at]));
    painter.drawImage(
        QRectF((piece.position - box.topLeft()) * scale,
               QSizeF(piece.size) * scale),
        drawing);
  }
  painter.end();
  return QPixmap::fromImage(canvas);
}

DollCards::DollCards(QWidget* parent) : QListWidget(parent) {
  assert(kCardIcon.width() > 0);
  assert(kCardCell.width() > kCardIcon.width());
  setViewMode(QListView::IconMode);
  setIconSize(kCardIcon);
  setGridSize(kCardCell);
  setResizeMode(QListView::Adjust);
  setMovement(QListView::Static);
  setWordWrap(true);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setDefaultDropAction(Qt::CopyAction);
  assert(dragEnabled());
}

QStringList DollCards::mimeTypes() const {
  assert(dragEnabled());
  assert(std::string_view(kDollMime).size() > 0);
  return {QString::fromLatin1(kDollMime)};
}

void DollCards::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(viewport() != nullptr);
  QListWidget::paintEvent(event);
  const bool is_empty = count() == 0;
  if (is_empty) {
    QPainter painter(viewport());
    painter.setPen(palette().color(QPalette::Disabled, QPalette::Text));
    painter.drawText(viewport()->rect().adjusted(8, 8, -8, -8),
                     Qt::AlignCenter | Qt::TextWordWrap,
                     tr("Export a doll from Krita, then Import."));
  }
}

QMimeData* DollCards::mimeData(const QList<QListWidgetItem*>& items) const {
  assert(items.size() >= 0);
  assert(dragEnabled());
  QStringList names;
  for (const QListWidgetItem* item : items) {
    names.append(item->data(kNameRole).toString());
  }
  const bool is_empty = names.isEmpty();
  if (is_empty) {
    return nullptr;
  }
  // The drag owns the data from here.
  QMimeData* dragged = std::make_unique<QMimeData>().release();
  dragged->setData(QString::fromLatin1(kDollMime), names.join('\n').toUtf8());
  return dragged;
}

}  // namespace snapper
