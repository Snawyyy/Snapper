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

// The dolls for a drop, or none when it carries none.
QStringList DollsOfDrop(const QMimeData* data);

// A doll's folder as a small picture: the default drawings of its
// pieces stacked as Krita had them, scaled to fit size by size. Empty
// when the art can't be read.
QPixmap DollPicture(const QString& folder, int size);

// The doll library as cards: a picture of each doll over its name and
// a badge (NEW ART, IN SHOT), picked like any list and dragged out
// onto the stage, which puts the dolls there.
class DollCards final : public QListWidget {
  Q_OBJECT

 public:
  explicit DollCards(QWidget* parent = nullptr);

 protected:
  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override;
  // An empty library says what to do.
  void paintEvent(QPaintEvent* event) override;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DOLL_CARDS_H_
