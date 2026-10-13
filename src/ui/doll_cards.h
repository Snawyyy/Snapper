#ifndef SNAPPER_UI_DOLL_CARDS_H_
#define SNAPPER_UI_DOLL_CARDS_H_

#include <QListWidget>
#include <QMimeData>
#include <QPixmap>
#include <QString>
#include <QStringList>

namespace snapper {

// What dolls dragged out of the library carry: their names, one a line.
inline constexpr char kDollMime[] = "application/x-snapper-doll";

// A folder card keeps its path from the library's top in this role (a
// doll card has nothing there); the doll's name is in Qt::UserRole as
// ever.
inline constexpr int kFolderRole = Qt::UserRole + 1;
// Whether item is a folder card.
bool IsFolderCard(const QListWidgetItem* item);

// The dolls for a drop, or none when it carries none.
QStringList DollsOfDrop(const QMimeData* data);

// A doll's folder as a small picture: the default drawings of its
// pieces stacked as Krita had them, scaled to fit size by size. Empty
// when the art can't be read.
QPixmap DollPicture(const QString& folder, int size);

// The doll library as cards: a picture of each doll over its name and
// a badge (NEW ART, IN SHOT), picked by the one pick rule (PickList:
// Shift adds, Ctrl flips, Ctrl+A, Escape, Delete asks) and dragged out
// onto the stage, which puts the dolls there.
class DollCards final : public QListWidget {
  Q_OBJECT

 public:
  explicit DollCards(QWidget* parent = nullptr);

  // What an empty row says to do.
  void SetEmptyText(const QString& text) { empty_text_ = text; }
  // Lets files be dropped on the row (the Objects row): FilesDropped
  // hands their paths on.
  void AcceptFiles();
  // Lets dolls dragged out of this row be dropped on its folder cards:
  // DollsDropped hands them on.
  void AcceptDollsOnFolders();

 signals:
  void FilesDropped(const QStringList& paths);
  // Delete or Backspace with cards picked.
  void DeleteAsked();
  // Dolls dropped on the folder card at folder (its path).
  void DollsDropped(const QStringList& dolls, const QString& folder);

 protected:
  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override;
  // An empty library says what to do.
  void paintEvent(QPaintEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  QString empty_text_;
  bool is_file_target_ = false;
  bool is_folder_target_ = false;
  // The folder card under at, or nullptr.
  const QListWidgetItem* FolderAt(const QPoint& at) const;
};

// The local files and folders a drop carries, or none.
QStringList FilesOfDrop(const QMimeData* data);

}  // namespace snapper

#endif  // SNAPPER_UI_DOLL_CARDS_H_
