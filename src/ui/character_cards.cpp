#include "ui/character_cards.h"

#include <QPaintEvent>
#include <QPainter>
#include <QUrl>

#include <cassert>
#include <memory>
#include <string_view>

#include "ui/pick_list.h"

namespace snapper {
namespace {

// Each card keeps its character's name here (the cast panel sets it).
constexpr int kNameRole = Qt::UserRole;
constexpr QSize kCardIcon(72, 72);
constexpr QSize kCardCell(92, 112);

}  // namespace

QStringList CharactersOfDrop(const QMimeData* data) {
  assert(data != nullptr);
  assert(std::string_view(kCharacterMime).size() > 0);
  const QString mime = QString::fromLatin1(kCharacterMime);
  const bool has_characters = data->hasFormat(mime);
  if (!has_characters) {
    return {};
  }
  return QString::fromUtf8(data->data(mime)).split('\n', Qt::SkipEmptyParts);
}

QStringList FilesOfDrop(const QMimeData* data) {
  assert(data != nullptr);
  QStringList paths;
  for (const QUrl& url : data->urls()) {
    const bool is_local = url.isLocalFile();
    if (is_local) {
      paths.append(url.toLocalFile());
    }
  }
  assert(paths.size() <= data->urls().size());
  return paths;
}

CharacterCards::CharacterCards(QWidget* parent) : QListWidget(parent) {
  assert(kCardIcon.width() > 0);
  assert(kCardCell.width() > kCardIcon.width());
  setViewMode(QListView::IconMode);
  setIconSize(kCardIcon);
  setGridSize(kCardCell);
  setResizeMode(QListView::Adjust);
  setMovement(QListView::Static);
  setWordWrap(true);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setDefaultDropAction(Qt::CopyAction);
  connect(PickList::Install(this), &PickList::DeleteAsked, this,
          &CharacterCards::DeleteAsked);
  assert(dragEnabled());
}

QStringList CharacterCards::mimeTypes() const {
  assert(dragEnabled());
  assert(std::string_view(kCharacterMime).size() > 0);
  return {QString::fromLatin1(kCharacterMime)};
}

void CharacterCards::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(viewport() != nullptr);
  QListWidget::paintEvent(event);
  const bool is_empty = count() == 0;
  if (is_empty) {
    QPainter painter(viewport());
    painter.setPen(palette().color(QPalette::Disabled, QPalette::Text));
    painter.drawText(viewport()->rect().adjusted(8, 8, -8, -8),
                     Qt::AlignCenter | Qt::TextWordWrap,
                     tr("No characters yet: press New to make one."));
  }
}

QMimeData* CharacterCards::mimeData(
    const QList<QListWidgetItem*>& items) const {
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
  dragged->setData(QString::fromLatin1(kCharacterMime),
                   names.join('\n').toUtf8());
  return dragged;
}

}  // namespace snapper
