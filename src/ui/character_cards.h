#ifndef SNAPPER_UI_CHARACTER_CARDS_H_
#define SNAPPER_UI_CHARACTER_CARDS_H_

#include <QListWidget>
#include <QMimeData>
#include <QString>
#include <QStringList>

namespace snapper {

// What characters dragged out of the library carry: their names, one a
// line.
inline constexpr char kCharacterMime[] = "application/x-snapper-character";

// The characters a drop carries, or none.
QStringList CharactersOfDrop(const QMimeData* data);
// The local files and folders a drop carries, or none.
QStringList FilesOfDrop(const QMimeData* data);

// The character library as cards: a picture of each character over its
// name and a badge (In shot, In project), picked by the one pick rule
// (PickList: Shift adds, Ctrl flips, Ctrl+A, Escape, Delete asks) and
// dragged out onto the stage, which puts the characters there.
class CharacterCards final : public QListWidget {
  Q_OBJECT

 public:
  explicit CharacterCards(QWidget* parent = nullptr);

 signals:
  // Delete or Backspace with cards picked.
  void DeleteAsked();

 protected:
  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override;
  // An empty library says what to do.
  void paintEvent(QPaintEvent* event) override;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CHARACTER_CARDS_H_
